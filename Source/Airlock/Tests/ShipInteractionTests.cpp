// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/Interactable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Ship/ShipLine.h"
#include "Ship/ShipModule.h"
#include "Station/VoyageBoard.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// An empty transient world exercises the real game mode without loading or saving a map.
// A plain game instance avoids starting Airlock's file-console subsystem during tests.
struct FShipTestWorld
{
	UWorld* World;
	AAirlockGameMode* Mode;
	AAirlockGameState* State;
	explicit FShipTestWorld(FName Section = TEXT("Hauler_Mk1"))
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World));
		FURL URL;
		URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>();
		State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = Section;
		World->BeginPlay();
	}
	~FShipTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	AShipLine* FindLine(FName Id) const
	{
		for (TActorIterator<AShipLine> It(World); It; ++It)
		{
			if (It->GetFName() == Id) { return *It; }
		}
		return nullptr;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphPublicationTest, "Airlock.ShipGraph.AuthorityPublicationAndSectionSpawning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphPublicationTest::RunTest(const FString& Parameters)
{
	FShipTestWorld Ship;
	TestEqual(TEXT("Game mode publishes seven modules"), Ship.State->ShipModules.Num(), 7);
	TestEqual(TEXT("Game mode publishes four lines"), Ship.State->ShipLines.Num(), 4);
	int32 Modules = 0, Lines = 0;
	for (TActorIterator<AShipModule> It(Ship.World); It; ++It) { ++Modules; }
	for (TActorIterator<AShipLine> It(Ship.World); It; ++It) { ++Lines; }
	TestEqual(TEXT("One actor per module"), Modules, 7);
	TestEqual(TEXT("One actor per line"), Lines, 4);
	int32 Notifications = 0;
	const FDelegateHandle Handle = Ship.State->OnShipGraphChanged.AddLambda([&Notifications]() { ++Notifications; });
	TestTrue(TEXT("API accepts fault"), Ship.Mode->ShipSetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak));
	TestEqual(TEXT("Server notifies changed mirror"), Notifications, 1);
	Ship.Mode->ShipSetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
	TestEqual(TEXT("Unchanged mirror is not republished"), Notifications, 1);
	Ship.Mode->ShipDamageModule(TEXT("CoolantLoop"), 50.f);
	TestTrue(TEXT("API damage publishes immediately"), Ship.State->ShipModules[1].Status == EShipModuleStatus::Degraded);
	Ship.Mode->ShipRepairModule(TEXT("CoolantLoop"), 100.f);
	TestEqual(TEXT("API repair publishes clamped condition"), Ship.State->ShipModules[1].Condition, 100.f);
	Ship.Mode->ShipRepairLine(TEXT("Coolant_1"));
	TestTrue(TEXT("Repair clears mirrored fault"), Ship.State->ShipLines[0].Fault == EShipLineFault::None);
	TestFalse(TEXT("Unknown API id rejected"), Ship.Mode->ShipDamageModule(TEXT("Missing"), 5.f));
	Ship.State->OnShipGraphChanged.Remove(Handle);
	FShipTestWorld Hub(TEXT("StationHub"));
	TestTrue(TEXT("Hub has no ship state"), Hub.State->ShipModules.IsEmpty() && Hub.State->ShipLines.IsEmpty());
	TestNull(TEXT("Hub spawns no lines"), Hub.FindLine(TEXT("Coolant_1")));
	return true;
}

namespace
{
class FShipVoyageGateCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	FShipTestWorld Ship;
	int32 Step = 0;
public:
	explicit FShipVoyageGateCommand(FAutomationTestBase* InTest) : Test(InTest)
	{
		Ship.Mode->ShipSetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
	}
	virtual bool Update() override
	{
		// Each update gets a new engine frame: TimerManager rejects a second tick
		// in the same frame. The first update promotes its pending timers.
		Ship.World->GetTimerManager().Tick(1.f);
		if (Step == 1)
		{
			Test->TestEqual(TEXT("Inactive voyage leaves coolant at 100"), Ship.State->ShipModules[1].Condition, 100.f);
			Ship.State->bVoyageActive = true;
		}
		else if (Step == 2)
		{
			Test->TestEqual(TEXT("Four quarter-second ticks drain one point"), Ship.State->ShipModules[1].Condition, 99.f);
			Ship.State->bVoyageActive = false;
		}
		else if (Step == 3)
		{
			Test->TestEqual(TEXT("Ending voyage stops further drain"), Ship.State->ShipModules[1].Condition, 99.f);
			return true;
		}
		++Step;
		return false;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipVoyageGateTest, "Airlock.ShipGraph.TimerRunsOnlyDuringVoyage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipVoyageGateTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FShipVoyageGateCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipHoldLifecycleTest, "Airlock.ShipGraph.HoldPausesReleasesAndRepairs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipHoldLifecycleTest::RunTest(const FString& Parameters)
{
	FShipTestWorld Ship;
	AShipLine* Line = Ship.FindLine(TEXT("Coolant_1"));
	if (!TestNotNull(TEXT("Spawned repair target"), Line)) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAirlockCharacter* Crew = Ship.World->SpawnActor<AAirlockCharacter>(Params);
	Ship.Mode->ShipSetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
	Crew->ServerBeginHold(Line);
	TestNull(TEXT("Distant hold rejected"), Crew->GetHoldTarget());
	const FVector Near = Line->GetActorLocation() + FVector(0, 180, 96);
	Crew->SetActorLocation(Near);
	Crew->ServerBeginHold(Line);
	Crew->Tick(1.f);
	TestEqual(TEXT("Leak advances by a third in one second"), Crew->GetHoldProgress(), 1.f / 3.f);
	Crew->SetActorLocation(Near + FVector(1000, 0, 0));
	Crew->Tick(5.f);
	TestEqual(TEXT("Out of range pauses"), Crew->GetHoldProgress(), 1.f / 3.f);
	Crew->SetActorLocation(Near);
	Crew->Tick(1.f);
	TestEqual(TEXT("Returning resumes elapsed time"), Crew->GetHoldProgress(), 2.f / 3.f);
	Crew->ServerEndHold();
	TestNull(TEXT("Release clears target"), Crew->GetHoldTarget());
	TestEqual(TEXT("Release clears progress"), Crew->GetHoldProgress(), 0.f);
	Crew->ServerBeginHold(Line);
	Crew->Tick(2.f);
	TestTrue(TEXT("Release discarded earlier time"), Line->GetFault() == EShipLineFault::Leak);
	Crew->Tick(1.f);
	TestTrue(TEXT("Three-second completion repairs through game mode"), Line->GetFault() == EShipLineFault::None);
	TestNull(TEXT("Completion clears target"), Crew->GetHoldTarget());
	Crew->ServerBeginHold(Line);
	TestNull(TEXT("Healthy line rejects hold"), Crew->GetHoldTarget());
	TestEqual(TEXT("Voyage board remains a press interaction"),
		IInteractable::Execute_GetInteractHoldSeconds(GetMutableDefault<AVoyageBoard>()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipConcurrentHoldTest, "Airlock.ShipGraph.CompetingHoldsAndDestroyedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipConcurrentHoldTest::RunTest(const FString& Parameters)
{
	FShipTestWorld Ship;
	AShipLine* Line = Ship.FindLine(TEXT("Power_1"));
	if (!TestNotNull(TEXT("Power line"), Line)) { return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAirlockCharacter* First = Ship.World->SpawnActor<AAirlockCharacter>(Params);
	AAirlockCharacter* Second = Ship.World->SpawnActor<AAirlockCharacter>(Params);
	for (AAirlockCharacter* Crew : { First, Second }) { Crew->SetActorLocation(Line->GetActorLocation() + FVector(0, 180, 96)); }
	Ship.Mode->ShipSetLineFault(TEXT("Power_1"), EShipLineFault::Short);
	First->ServerBeginHold(Line);
	Second->ServerBeginHold(Line);
	First->Tick(3.f);
	Second->Tick(2.f);
	TestTrue(TEXT("Short needs four seconds"), Line->GetFault() == EShipLineFault::Short);
	First->Tick(1.f);
	Second->Tick(2.f);
	TestTrue(TEXT("First completion clears fault"), Line->GetFault() == EShipLineFault::None);
	TestNull(TEXT("Other hold ends on external repair"), Second->GetHoldTarget());
	TestEqual(TEXT("Other hold resets progress"), Second->GetHoldProgress(), 0.f);
	Ship.Mode->ShipSetLineFault(TEXT("Power_1"), EShipLineFault::Short);
	Second->ServerBeginHold(Line);
	Second->Tick(1.f);
	Line->Destroy();
	Second->Tick(5.f);
	TestNull(TEXT("Destroyed target cancels safely"), Second->GetHoldTarget());
	return true;
}
#endif
