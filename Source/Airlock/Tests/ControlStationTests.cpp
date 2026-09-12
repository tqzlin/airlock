// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Components/CapsuleComponent.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/Interactable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ship/ControlStation.h"
#include "Ship/ShipLayouts.h"
#include "UI/ControlBoardWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// Same transient-world harness as ShipInteractionTests: the real game mode spawns the ship actors from the layout.
struct FStationTestWorld
{
	UWorld* World;
	AAirlockGameMode* Mode;
	AAirlockGameState* State;
	explicit FStationTestWorld(FName Section = TEXT("Hauler_Mk1"))
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
	~FStationTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	AControlStation* FindStation(FName Id) const
	{
		for (TActorIterator<AControlStation> It(World); It; ++It)
		{
			if (It->GetStationId() == Id) { return *It; }
		}
		return nullptr;
	}
	AAirlockCharacter* SpawnCrew(const FVector& Location) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAirlockCharacter* Crew = World->SpawnActor<AAirlockCharacter>(Params);
		Crew->SetActorLocation(Location);
		return Crew;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlStationSpawnTest, "Airlock.Ship.ControlStationSpawnsFromLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControlStationSpawnTest::RunTest(const FString& Parameters)
{
	const FShipLayout* Layout = ShipLayouts::Find(TEXT("Hauler_Mk1"));
	if (!TestNotNull(TEXT("Hauler layout"), Layout)) { return false; }
	TestEqual(TEXT("Hauler has one control station"), Layout->Stations.Num(), 1);
	FStationTestWorld Ship;
	AControlStation* Station = Ship.FindStation(TEXT("Bridge"));
	if (!TestNotNull(TEXT("Bridge station spawned"), Station)) { return false; }
	TestTrue(TEXT("Station sits at the layout seat point"), Station->GetActorLocation().Equals(Layout->Stations[0].Transform.GetLocation(), 1.f));
	TestTrue(TEXT("Nobody seated at start"), Station->GetOccupant() == nullptr);
	TestTrue(TEXT("Exit point lies behind the seat"),
		FVector::Dist(Station->GetExitLocation(), Station->GetSeatLocation() - Station->GetActorForwardVector() * Layout->Stations[0].ExitDistance) < 1.f);
	TestTrue(TEXT("Free seat prompt invites"), IInteractable::Execute_GetInteractPrompt(Station).ToString().Contains(TEXT("Take the control station")));
	TestEqual(TEXT("Seat is a press, not a hold"), IInteractable::Execute_GetInteractHoldSeconds(Station), 0.f);
	FStationTestWorld Hub(TEXT("StationHub"));
	TestNull(TEXT("Hub has no control station"), Hub.FindStation(TEXT("Bridge")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlStationSitStandTest, "Airlock.Ship.ControlStationSitAndStand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControlStationSitStandTest::RunTest(const FString& Parameters)
{
	FStationTestWorld Ship;
	AControlStation* Station = Ship.FindStation(TEXT("Bridge"));
	if (!TestNotNull(TEXT("Bridge station"), Station)) { return false; }
	const FVector Seat = Station->GetSeatLocation();
	AAirlockCharacter* Crew = Ship.SpawnCrew(Seat + FVector(-2000, 0, 96));
	Crew->GetCharacterMovement()->SetMovementMode(MOVE_Walking); // untouched pawns have not ticked yet (mode None)
	Crew->ServerSitAt(Station);
	TestNull(TEXT("Distant sit rejected"), Crew->GetSeatedStation());
	TestTrue(TEXT("Seat stays free after a rejected sit"), Station->GetOccupant() == nullptr);

	Crew->SetActorLocation(Seat + FVector(-200, 0, 96));
	Crew->ServerSitAt(Station);
	TestTrue(TEXT("In-range sit takes the seat"), Crew->GetSeatedStation() == Station && Crew->IsSeated());
	TestTrue(TEXT("Station records the occupant"), Station->GetOccupant() == Crew);
	const float HalfHeight = Crew->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TestTrue(TEXT("Seated pawn stands on the seat point"), Crew->GetActorLocation().Equals(Seat + FVector(0, 0, HalfHeight), 2.f));
	TestTrue(TEXT("Seated pawn cannot walk"), Crew->GetCharacterMovement()->MovementMode == MOVE_None);
	TestTrue(TEXT("Occupied prompt names the occupant"), IInteractable::Execute_GetInteractPrompt(Station).ToString().Contains(TEXT("at the controls")));
	Crew->ServerSitAt(Station);
	TestTrue(TEXT("Sitting again is a no-op"), Crew->GetSeatedStation() == Station && Station->GetOccupant() == Crew);

	Crew->ServerStandUp();
	TestNull(TEXT("Standing clears the seat"), Crew->GetSeatedStation());
	TestTrue(TEXT("Station is free again"), Station->GetOccupant() == nullptr);
	TestTrue(TEXT("Walking restored"), Crew->GetCharacterMovement()->MovementMode == MOVE_Walking);
	TestTrue(TEXT("Stood up at the exit point"), Crew->GetActorLocation().Equals(Station->GetExitLocation() + FVector(0, 0, HalfHeight), 2.f));
	Crew->ServerStandUp();
	TestTrue(TEXT("Standing twice is harmless"), Station->GetOccupant() == nullptr && Crew->GetCharacterMovement()->MovementMode == MOVE_Walking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlStationExclusiveTest, "Airlock.Ship.ControlStationIsExclusive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControlStationExclusiveTest::RunTest(const FString& Parameters)
{
	FStationTestWorld Ship;
	AControlStation* Station = Ship.FindStation(TEXT("Bridge"));
	if (!TestNotNull(TEXT("Bridge station"), Station)) { return false; }
	const FVector Near = Station->GetSeatLocation() + FVector(-200, 0, 96);
	AAirlockCharacter* First = Ship.SpawnCrew(Near);
	AAirlockCharacter* Second = Ship.SpawnCrew(Near + FVector(0, 100, 0));
	// A pawn spawned in the transient world has not ticked, so its movement mode is still None; give it legs first.
	Second->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	First->ServerSitAt(Station);
	Second->ServerSitAt(Station);
	TestTrue(TEXT("First keeps the seat"), Station->GetOccupant() == First);
	TestNull(TEXT("Second is refused"), Second->GetSeatedStation());
	TestTrue(TEXT("Refused pawn is not frozen"), Second->GetCharacterMovement()->MovementMode == MOVE_Walking);
	First->ServerStandUp();
	Second->SetActorLocation(Near);
	Second->ServerSitAt(Station);
	TestTrue(TEXT("Seat passes to the next crew member"), Station->GetOccupant() == Second && Second->IsSeated());
	Second->Destroy();
	TestTrue(TEXT("Destroying the occupant frees the seat"), Station->GetOccupant() == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlBoardRowsTest, "Airlock.Ship.ControlBoardListsEveryModuleAndLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FControlBoardRowsTest::RunTest(const FString& Parameters)
{
	FStationTestWorld Ship;
	Ship.Mode->ShipDamageModule(TEXT("LifeSupport"), 90.f);
	Ship.Mode->ShipDamageModule(TEXT("Engines"), 50.f);
	Ship.Mode->ShipSetLineFault(TEXT("Coolant_2"), EShipLineFault::Leak);
	const TArray<FControlBoardRow> Rows = UControlBoardWidget::BuildRows(Ship.State->ShipModules, Ship.State->ShipLines);
	// Two headers + 7 modules + 4 lines.
	TestEqual(TEXT("One row per module and line plus two headers"), Rows.Num(), 2 + 7 + 4);
	int32 Headers = 0, Failed = 0, Warnings = 0;
	bool bLifeSupportFailed = false, bEnginesDegraded = false, bLeakShown = false, bHealthyLineOk = false;
	for (const FControlBoardRow& Row : Rows)
	{
		const FString Text = Row.Text.ToString();
		if (Row.Tone == EControlBoardTone::Header) { ++Headers; }
		if (Row.Tone == EControlBoardTone::Failed) { ++Failed; }
		if (Row.Tone == EControlBoardTone::Warning) { ++Warnings; }
		if (Text.StartsWith(TEXT("LifeSupport")) && Text.Contains(TEXT("Failed")) && Text.Contains(TEXT("10%"))) { bLifeSupportFailed = Row.Tone == EControlBoardTone::Failed; }
		if (Text.StartsWith(TEXT("Engines")) && Text.Contains(TEXT("Degraded")) && Text.Contains(TEXT("50%"))) { bEnginesDegraded = Row.Tone == EControlBoardTone::Warning; }
		if (Text.StartsWith(TEXT("Coolant_2")) && Text.Contains(TEXT("Leak"))) { bLeakShown = Row.Tone == EControlBoardTone::Warning; }
		if (Text.StartsWith(TEXT("Coolant_1")) && Text.Contains(TEXT("OK"))) { bHealthyLineOk = Row.Tone == EControlBoardTone::Normal; }
	}
	TestEqual(TEXT("Modules and lines headers"), Headers, 2);
	TestEqual(TEXT("Exactly one failed row"), Failed, 1);
	TestEqual(TEXT("Degraded module and leaking line are warnings"), Warnings, 2);
	TestTrue(TEXT("Failed life support row reads condition and tone"), bLifeSupportFailed);
	TestTrue(TEXT("Degraded engines row"), bEnginesDegraded);
	TestTrue(TEXT("Leaking line row"), bLeakShown);
	TestTrue(TEXT("Healthy line reads OK"), bHealthyLineOk);
	const TArray<FControlBoardRow> Empty = UControlBoardWidget::BuildRows({}, {});
	TestTrue(TEXT("Empty graph explains itself"), Empty.Last().Text.ToString().Contains(TEXT("No ship systems")));
	return true;
}
#endif
