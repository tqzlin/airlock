// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Voyage/IncidentDirectorComponent.h"
#include "Voyage/VoyageSubsystem.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FIncidentTestWorld
{
	UWorld* World;
	AAirlockGameMode* Mode;
	AAirlockGameState* State;
	UIncidentDirectorComponent* Director;
	FIncidentTestWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World));
		World->GetGameInstance()->Init();
		FVoyageSpec Spec;
		Spec.Captain = TEXT("TestCaptain");
		FGuid VoyageId;
		World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>()->RequestVoyage(Spec, VoyageId);
		FURL URL;
		URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>();
		State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = TEXT("Hauler_Mk1");
		World->BeginPlay();
		Director = Mode->GetIncidentDirector();
	}
	~FIncidentTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->GetGameInstance()->Shutdown();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	void MoveToElapsed(float Elapsed)
	{
		State->VoyageEndServerTime = State->GetServerWorldTimeSeconds() + 1200 - Elapsed;
		Director->Reschedule();
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentApplyTest, "Airlock.Incidents.DirectorAppliesAndSkipsInvalidTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentApplyTest::RunTest(const FString& Parameters)
{
	FIncidentTestWorld Ship;
	TestTrue(TEXT("Default leak picks first available line"), Ship.Director->FireNow(EIncidentType::CoolantLeak));
	TestTrue(TEXT("Fault mirrored immediately"), Ship.State->ShipLines[0].Fault == EShipLineFault::Leak);
	TestFalse(TEXT("Already faulted target skipped"), Ship.Director->FireNow(EIncidentType::CoolantLeak, TEXT("Coolant_1")));
	TestTrue(TEXT("Omitted target advances to next healthy line"), Ship.Director->FireNow(EIncidentType::CoolantLeak));
	TestFalse(TEXT("No available target skipped"), Ship.Director->FireNow(EIncidentType::CoolantLeak));
	TestFalse(TEXT("Missing target skipped"), Ship.Director->FireNow(EIncidentType::PowerShort, TEXT("Missing")));
	TestFalse(TEXT("Wrong line kind skipped"), Ship.Director->FireNow(EIncidentType::PowerShort, TEXT("Coolant_1")));
	TestTrue(TEXT("Short mirrored through API"), Ship.Director->FireNow(EIncidentType::PowerShort, TEXT("Power_1")));
	TestTrue(TEXT("Damage applied"), Ship.Director->FireNow(EIncidentType::ModuleDamage, TEXT("PowerCore"), 45.f));
	TestEqual(TEXT("Core now 55 percent"), Ship.State->ShipModules[0].Condition, 55.f);
	Ship.Mode->ShipDamageModule(TEXT("PowerCore"), 40.f);
	TestFalse(TEXT("Failed module skipped"), Ship.Director->FireNow(EIncidentType::ModuleDamage, TEXT("PowerCore"), 40.f));
	TestEqual(TEXT("Skipped damage does not change condition"), Ship.State->ShipModules[0].Condition, 15.f);
	TestFalse(TEXT("Non-finite damage rejected"), Ship.Director->FireNow(EIncidentType::ModuleDamage, TEXT("Engines"), std::numeric_limits<float>::infinity()));
	TestFalse(TEXT("Negative damage rejected"), Ship.Director->FireNow(EIncidentType::ModuleDamage, TEXT("Engines"), -5.f));
	Ship.State->bVoyageActive = false;
	TestFalse(TEXT("Inactive voyage cannot mutate graph"), Ship.Director->FireNow(EIncidentType::PowerShort, TEXT("Power_2")));
	Ship.State->bVoyageActive = true;
	Ship.State->CurrentSectionId = TEXT("StationHub");
	TestFalse(TEXT("Hub cannot receive incidents"), Ship.Director->FireNow(EIncidentType::PowerShort, TEXT("Power_2")));
	return true;
}

namespace
{
class FIncidentTimerCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	FIncidentTestWorld Ship;
	int32 Step = 0;
public:
	explicit FIncidentTimerCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		Ship.World->GetTimerManager().Tick(.1f);
		if (Step == 0)
		{
			Test->TestEqual(TEXT("Calm period has no consumed incidents"), Ship.Director->GetNextEntry(), 0);
			Ship.MoveToElapsed(160);
		}
		else if (Step == 1)
		{
			Test->TestEqual(TEXT("Forward move fires teaching leak"), Ship.Director->GetNextEntry(), 1);
			const FName Target = Ship.Director->GetSchedule()[0].TargetId;
			Ship.Mode->ShipRepairLine(Target);
			Ship.MoveToElapsed(0);
		}
		else if (Step == 2)
		{
			Test->TestEqual(TEXT("Backward move retains cursor"), Ship.Director->GetNextEntry(), 1);
			Ship.MoveToElapsed(160);
		}
		else if (Step == 3)
		{
			Test->TestEqual(TEXT("Repeat move does not replay repaired leak"), Ship.Director->GetNextEntry(), 1);
			const FName Target = Ship.Director->GetSchedule()[0].TargetId;
			const auto* Line = Ship.State->ShipLines.FindByPredicate([Target](const FShipLineState& L) { return L.Id == Target; });
			Test->TestTrue(TEXT("Teaching leak stays repaired"), Line && Line->Fault == EShipLineFault::None);
			Ship.MoveToElapsed(1130);
		}
		else if (Step == 4)
		{
			Test->TestEqual(TEXT("Catch-up consumes all remaining incidents"), Ship.Director->GetNextEntry(), 4);
			FVoyageSpec Spec;
			Ship.Director->StartVoyage(Spec, *ShipLayouts::Find(TEXT("Hauler_Mk1")), 4);
			Test->TestEqual(TEXT("Crew sampled only once"), Ship.Director->GetSchedule().Num(), 4);
			Ship.MoveToElapsed(0);
			Ship.State->bVoyageActive = false;
		}
		else if (Step == 5)
		{
			Test->TestEqual(TEXT("Ended voyage has no replay"), Ship.Director->GetNextEntry(), 4);
			return true;
		}
		++Step;
		return false;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentTimerTest, "Airlock.Incidents.DirectorTimerClockMovesAndCrewSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentTimerTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FIncidentTimerCommand(this));
	return true;
}

namespace
{
class FIncidentDockingJumpCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	FIncidentTestWorld Ship;
	int32 Step = 0;
public:
	explicit FIncidentDockingJumpCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		Ship.World->GetTimerManager().Tick(.1f);
		if (Step++ == 0)
		{
			Test->TestNotNull(TEXT("Booked ship creates the director"), Ship.Director);
			Ship.Mode->SetVoyageSecondsRemaining(0.f);
			return false;
		}
		Test->TestEqual(TEXT("Direct docking jump consumes every incident before ending voyage"), Ship.Director->GetNextEntry(), Ship.Director->GetSchedule().Num());
		Test->TestFalse(TEXT("Clock ends voyage after catch-up"), Ship.State->bVoyageActive);
		Test->TestFalse(TEXT("Docking clears the booking"), Ship.World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>()->HasPendingVoyage());
		return true;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentDockingJumpTest, "Airlock.Incidents.DockingJumpConsumesBeforeReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentDockingJumpTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FIncidentDockingJumpCommand(this));
	return true;
}

namespace
{
class FCargoIncidentLogCapture : public FOutputDevice
{
public:
	FString Lines;
	FCargoIncidentLogCapture() { GLog->AddOutputDevice(this); }
	~FCargoIncidentLogCapture() { GLog->RemoveOutputDevice(this); }
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Category == TEXT("LogAirlock")) { Lines += V; Lines += TEXT("\n"); }
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoIncidentApplyTest, "Airlock.Cargo.DirectorReleasesFilledClampAndSkipsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoIncidentApplyTest::RunTest(const FString& Parameters)
{
	FIncidentTestWorld Ship;
	Ship.Mode->CargoPickUp(TEXT("C1")); Ship.Mode->CargoPlace(TEXT("C1"), TEXT("Clamp_2"));
	FCargoIncidentLogCapture Capture;
	TestTrue(TEXT("Shift releases even nonvolatile crate"), Ship.Director->FireNow(EIncidentType::CargoShift, TEXT("Clamp_2")));
	TestTrue(TEXT("Crate now loose and slot cleared"), Ship.State->Cargo[0].State == ECargoState::Loose && Ship.State->Cargo[0].SlotId.IsNone());
	TestFalse(TEXT("Empty clamp skipped"), Ship.Director->FireNow(EIncidentType::CargoShift, TEXT("Clamp_2")));
	TestTrue(TEXT("Empty clamp reason logged"), Capture.Lines.Contains(TEXT("skipped: clamp empty")));
	TestTrue(TEXT("Shift magnitude unused"), Capture.Lines.Contains(TEXT("Incident CargoShift Clamp_2 (0.00)")));
	TestFalse(TEXT("Rack never shifts"), Ship.Director->FireNow(EIncidentType::CargoShift, TEXT("ColdRack")));
	Ship.Mode->CargoPickUp(TEXT("C2")); Ship.Mode->CargoPlace(TEXT("C2"), TEXT("Clamp_3"));
	TestTrue(TEXT("Omitted target finds filled clamp"), Ship.Director->FireNow(EIncidentType::CargoShift));
	TestTrue(TEXT("Filled target released"), Ship.State->Cargo[1].State == ECargoState::Loose);
	TestFalse(TEXT("No filled clamp skipped"), Ship.Director->FireNow(EIncidentType::CargoShift));
	return true;
}
#endif
