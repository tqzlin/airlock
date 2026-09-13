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
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"
#include "Ship/ShipLine.h"
#include "Ship/ShipModule.h"
#include "Ship/ControlStation.h"
#include "TimerManager.h"
#include "UI/VoyageBoardWidget.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// FCargoTestWorld's transient world harness, with independent names and an active-clock helper.
class FAirFrameCommand : public IAutomationLatentCommand
{
	TFunction<bool()> Step;
public:
	explicit FAirFrameCommand(TFunction<bool()> InStep) : Step(MoveTemp(InStep)) {}
	virtual bool Update() override { return Step(); }
};
struct FAirTestWorld
{
	UWorld* World;
	AAirlockGameMode* Mode;
	AAirlockGameState* State;
	explicit FAirTestWorld(FName Section = TEXT("Hauler_Mk1"))
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World));
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>(); State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = Section; World->BeginPlay();
	}
	~FAirTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
	template<class T> T* Find(FName Id) const
	{
		for (TActorIterator<T> It(World); It; ++It) { if (It->GetFName() == Id) { return *It; } }
		return nullptr;
	}
	AAirlockCharacter* CrewAt(FVector Location = FVector::ZeroVector) const
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Crew = World->SpawnActor<AAirlockCharacter>(Params);
		Crew->SetActorLocation(Location); Crew->GetCharacterMovement()->SetMovementMode(MOVE_Walking); return Crew;
	}
	void Activate() const { State->bVoyageActive = true; State->VoyageEndServerTime = State->GetServerWorldTimeSeconds() + 1200; }
	void AddFloor() const
	{
		auto* Floor = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(1000, 1000, 10));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent(); Floor->SetActorLocation(FVector(0, 0, -10));
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirPublicationTest, "Airlock.Atmosphere.PublicationAndActiveVoyageGating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirPublicationTest::RunTest(const FString& Parameters)
{
	const auto Keep = MakeShared<FAirTestWorld>();
	auto& Ship = *Keep;
	TestTrue(TEXT("Cargo section has atmosphere on both sides"), Ship.Mode->AirHasAtmosphere() && Ship.State->AirHasAtmosphere());
	TestTrue(TEXT("Initial state equals reset"), Ship.State->Atmosphere == FAtmosphereState());
	int32 Notifications = 0;
	const auto Handle = Ship.State->OnShipGraphChanged.AddLambda([&Notifications] { ++Notifications; });
	Ship.State->PublishAtmosphere(FAtmosphereState(), {}); TestEqual(TEXT("Default publish silent"), Notifications, 0);
	FAtmosphereState Air; Air.Ppm = 12;
	Ship.State->PublishAtmosphere(Air, {}); TestEqual(TEXT("Air change once"), Notifications, 1);
	Ship.State->PublishAtmosphere(Air, { TEXT("C2") }); TestEqual(TEXT("Only FX ids change once"), Notifications, 2);
	Ship.State->PublishAtmosphere(Air, { TEXT("C2") }); TestEqual(TEXT("Unchanged silent"), Notifications, 2);
	Ship.State->OnShipGraphChanged.Remove(Handle);
	Ship.Mode->AirSetPpm(0.5f); TestEqual(TEXT("Publication floors ppm"), Ship.State->Atmosphere.Ppm, 0.f);
	Ship.Mode->CargoSetTemperature(TEXT("C2"), 45);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAirFrameCommand>([this, Keep, Step = 0]() mutable
	{
		auto& Ship = *Keep;
		Ship.World->GetTimerManager().Tick(1);
		if (Step == 1)
		{
			TestTrue(TEXT("Unbooked section never starts puff"), Ship.State->PuffingCrates.IsEmpty());
			Ship.Activate();
		}
		if (Step == 2)
		{
			TestTrue(TEXT("Active timer uses live cargo"), Ship.State->PuffingCrates.Contains(TEXT("C2")));
			TestEqual(TEXT("One source"), Ship.State->Atmosphere.ActiveSources, 1);
			Ship.Mode->CargoSetTemperature(TEXT("C2"), 100);
		}
		if (Step == 3)
		{
			TestTrue(TEXT("Cargo vent happens before atmosphere step"), Ship.State->Cargo[1].State == ECargoState::Vented);
			TestEqual(TEXT("Rupture starts in same tick"), Ship.State->Atmosphere.ActiveSources, 1);
			return true;
		}
		++Step; return false;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrewExposureSpeedTest, "Airlock.Atmosphere.CrewExposureCarrySpeedAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCrewExposureSpeedTest::RunTest(const FString& Parameters)
{
	FAirTestWorld Ship; auto* Crew = Ship.CrewAt(); const float Base = Crew->GetCharacterMovement()->MaxWalkSpeed;
	Ship.Mode->AirSetPpm(20); Crew->Tick(10); TestEqual(TEXT("No exposure without active voyage"), Crew->Exposure, 0.f);
	Ship.Activate(); Crew->Tick(10); TestEqual(TEXT("Active exposure rises"), Crew->Exposure, 10.f);
	Crew->SetExposureServer(60); TestEqual(TEXT("Slow at 60"), Crew->GetCharacterMovement()->MaxWalkSpeed, Base * FAtmosphereRates().SlowFactor);
	auto* Crate = Ship.Find<ACargoCrate>(TEXT("C1"));
	Crew->SetActorLocation(Crate->GetActorLocation() + FVector(-150, 0, 96)); Crew->ServerPickUp(Crate);
	TestEqual(TEXT("Carry and exposure combine"), Crew->GetCharacterMovement()->MaxWalkSpeed, Base * Crew->CarrySpeedFactor * FAtmosphereRates().SlowFactor);
	for (int32 I = 0; I < 8; ++I) { Crew->OnRep_CarriedCrate(); Crew->OnRep_Exposure(); }
	TestEqual(TEXT("Repeated RepNotifies never multiply speed"), Crew->GetCharacterMovement()->MaxWalkSpeed, Base * Crew->CarrySpeedFactor * FAtmosphereRates().SlowFactor);
	Ship.Mode->AirSetPpm(0); Crew->Tick(1);
	TestEqual(TEXT("Recovery below threshold restores carry speed"), Crew->GetCharacterMovement()->MaxWalkSpeed, Base * Crew->CarrySpeedFactor);
	Crew->ServerPutDown(); TestEqual(TEXT("Empty hands restore base"), Crew->GetCharacterMovement()->MaxWalkSpeed, Base);
	TestFalse(TEXT("NaN exposure rejected"), Crew->SetExposureServer(std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("Negative exposure rejected"), Crew->SetExposureServer(-1)); TestFalse(TEXT("Over 100 rejected"), Crew->SetExposureServer(101));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrewBlackoutTest, "Airlock.Atmosphere.BlackoutDropsCargoRefusesActionsAndRecovers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCrewBlackoutTest::RunTest(const FString& Parameters)
{
	const auto Keep = MakeShared<FAirTestWorld>();
	auto& Ship = *Keep; Ship.AddFloor(); Ship.Activate();
	auto* Crew = Ship.CrewAt(FVector(0, 0, 96)); auto* Crate = Ship.Find<ACargoCrate>(TEXT("C1"));
	Crate->SetActorLocation(FVector(150, 0, 0)); Crew->ServerPickUp(Crate);
	Crew->SetExposureServer(100); TestFalse(TEXT("Injection waits for tick"), Crew->bBlackedOut);
	Crew->Tick(.25f);
	TestTrue(TEXT("100 blacks out even in clean air"), Crew->bBlackedOut);
	TestNull(TEXT("Blackout frees hands"), Crew->CarriedCrate.Get());
	TestTrue(TEXT("Cargo dropped loose"), Crate->GetCargoState()->State == ECargoState::Loose);
	TestTrue(TEXT("Drop uses forward put-down floor spot"), Crate->GetActorLocation().Equals(FVector(120, 0, 0), 1));
	TestTrue(TEXT("Blackout stops movement"), Crew->GetCharacterMovement()->MovementMode == MOVE_None);
	TestEqual(TEXT("Counted once"), Ship.Mode->GetBlackoutCount(), 1);
	Ship.Mode->NotifyBlackout(Crew); Crew->Tick(1); TestEqual(TEXT("Repeated notification not counted"), Ship.Mode->GetBlackoutCount(), 1);
	Crew->ServerPickUp(Crate); TestNull(TEXT("Pickup refused"), Crew->CarriedCrate.Get());
	Crew->ServerPlace(Ship.Find<ACargoSlot>(TEXT("ColdRack"))); TestTrue(TEXT("Place refused"), Crate->GetCargoState()->State == ECargoState::Loose);
	auto* Line = Ship.Find<AShipLine>(TEXT("Coolant_1")); Ship.Mode->ShipSetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
	Crew->SetActorLocation(Line->GetActorLocation()); Crew->ServerBeginHold(Line); TestNull(TEXT("Hold refused"), Crew->GetHoldTarget());
	Ship.Mode->ShipRepairLine(TEXT("Coolant_1"));
	auto* Seat = Ship.Find<AControlStation>(TEXT("Station_Bridge")); Crew->SetActorLocation(Seat->GetActorLocation()); Crew->ServerSitAt(Seat);
	TestNull(TEXT("Sit refused"), Crew->GetSeatedStation());
	TestFalse(TEXT("Console scrub refused too"), Crew->SetScrubModeServer(EScrubMode::Hazard, false));
	Crew->ServerSetScrubMode(EScrubMode::Hazard); TestTrue(TEXT("RPC scrub mode unchanged"), Ship.Mode->AirGetScrubMode() == EScrubMode::Normal);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FAirFrameCommand>([this, Keep, Crew, Frame = 0]() mutable
	{
		auto& Ship = *Keep;
		Ship.World->Tick(LEVELTICK_All, .25f);
		++Frame;
		if (Frame == 119) { TestTrue(TEXT("Still out before deadline"), Crew->bBlackedOut); }
		if (Frame < 120) { return false; }
		// Explicitly tick the pawn too: a transient world has no player controller driving its actors.
		if (Crew->bBlackedOut) { Crew->Tick(.25f); }
		TestFalse(TEXT("Recovered at 30 seconds"), Crew->bBlackedOut);
		TestEqual(TEXT("Recovery sets exposure to 50"), Crew->Exposure, FAtmosphereRates().ExposureAfterBlackout);
		TestTrue(TEXT("Recovery restores walking"), Crew->GetCharacterMovement()->MovementMode == MOVE_Walking);
		Crew->SetExposureServer(100); Crew->Tick(.25f); TestEqual(TEXT("Next blackout counted separately"), Ship.Mode->GetBlackoutCount(), 2);
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrewBlackoutSeatTest, "Airlock.Atmosphere.BlackoutSeatHoldAndMovementPredicate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCrewBlackoutSeatTest::RunTest(const FString& Parameters)
{
	FAirTestWorld Ship; Ship.Activate(); auto* Seat = Ship.Find<AControlStation>(TEXT("Station_Bridge"));
	auto* Crew = Ship.CrewAt(Seat->GetActorLocation() + FVector(-150, 0, 96));
	Crew->ApplyLocalMovement(); TestTrue(TEXT("Neither walks"), Crew->GetCharacterMovement()->MovementMode == MOVE_Walking);
	Crew->ServerSitAt(Seat); Crew->ApplyLocalMovement(); TestTrue(TEXT("Seated stops"), Crew->GetCharacterMovement()->MovementMode == MOVE_None);
	Crew->bBlackedOut = true; Crew->ApplyLocalMovement(); TestTrue(TEXT("Both stop"), Crew->GetCharacterMovement()->MovementMode == MOVE_None);
	Crew->bBlackedOut = false; const FVector Before = Crew->GetActorLocation();
	Crew->SetExposureServer(100); Crew->Tick(.25f);
	TestNull(TEXT("Blackout releases seat"), Seat->GetOccupant()); TestNull(TEXT("Pawn seat cleared"), Crew->GetSeatedStation());
	TestTrue(TEXT("No exit teleport"), Crew->GetActorLocation().Equals(Before, .1));
	Crew->ApplyLocalMovement(); TestTrue(TEXT("Only blackout stops"), Crew->GetCharacterMovement()->MovementMode == MOVE_None);
	auto* Repairer = Ship.CrewAt(); auto* Line = Ship.Find<AShipLine>(TEXT("Coolant_1"));
	Ship.Mode->ShipSetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak); Repairer->SetActorLocation(Line->GetActorLocation());
	Repairer->ServerBeginHold(Line); TestNotNull(TEXT("Hold began"), Repairer->GetHoldTarget());
	Repairer->SetExposureServer(100); Repairer->Tick(.25f); TestNull(TEXT("Blackout cancels hold"), Repairer->GetHoldTarget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirScrubControlTest, "Airlock.Atmosphere.LifeSupportPanelRangeModesAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirScrubControlTest::RunTest(const FString& Parameters)
{
	FAirTestWorld Ship; auto* Life = Ship.Find<AShipModule>(TEXT("LifeSupport")); auto* Crew = Ship.CrewAt();
	TestEqual(TEXT("Life id"), Life->GetModuleId(), FName(TEXT("LifeSupport")));
	TestEqual(TEXT("Press duration zero"), IInteractable::Execute_GetInteractHoldSeconds(Life), 0.f);
	TestEqual(TEXT("Normal prompt"), IInteractable::Execute_GetInteractPrompt(Life).ToString(), FString(TEXT("Scrubbers: Normal - E for hazard mode")));
	Crew->ServerSetScrubMode(EScrubMode::Hazard); TestTrue(TEXT("Distant RPC refused"), Ship.Mode->AirGetScrubMode() == EScrubMode::Normal);
	TestTrue(TEXT("Console bypasses reach"), Crew->SetScrubModeServer(EScrubMode::Hazard, false));
	TestEqual(TEXT("Hazard prompt"), IInteractable::Execute_GetInteractPrompt(Life).ToString(), FString(TEXT("Scrubbers: Hazard - E for normal mode")));
	Crew->SetActorLocation(Life->GetActorLocation() + FVector(-150, 0, 0)); IInteractable::Execute_Interact(Life, Crew);
	TestTrue(TEXT("Hazard press selects normal"), Ship.Mode->AirGetScrubMode() == EScrubMode::Normal);
	IInteractable::Execute_Interact(Life, Crew); TestTrue(TEXT("Normal press selects hazard"), Ship.Mode->AirGetScrubMode() == EScrubMode::Hazard);
	Ship.Mode->AirSetScrubMode(EScrubMode::Off);
	TestEqual(TEXT("Off prompt"), IInteractable::Execute_GetInteractPrompt(Life).ToString(), FString(TEXT("Scrubbers: Off - E to start")));
	IInteractable::Execute_Interact(Life, Crew); TestTrue(TEXT("Off press starts normal"), Ship.Mode->AirGetScrubMode() == EScrubMode::Normal);
	TestTrue(TEXT("Label mirrors mode"), Life->Label->Text.ToString().Contains(TEXT("Scrubbers Normal")));
	auto* Core = Ship.Find<AShipModule>(TEXT("PowerCore"));
	TestTrue(TEXT("Other modules have no prompt"), IInteractable::Execute_GetInteractPrompt(Core).IsEmpty());
	IInteractable::Execute_Interact(Core, Crew); TestTrue(TEXT("Other module cannot switch"), Ship.Mode->AirGetScrubMode() == EScrubMode::Normal);
	Ship.Mode->ShipDamageModule(TEXT("LifeSupport"), 100);
	TestTrue(TEXT("Failure disables immediately"), Ship.State->Atmosphere.bScrubDisabled);
	TestTrue(TEXT("Failed prompt empty"), IInteractable::Execute_GetInteractPrompt(Life).IsEmpty());
	TestFalse(TEXT("Failed panel rejected server-side"), Crew->SetScrubModeServer(EScrubMode::Hazard, true));
	Ship.Mode->AirSetPpm(100); Ship.Activate(); Ship.World->GetTimerManager().Tick(1);
	TestEqual(TEXT("Failed removes no ppm"), Ship.State->Atmosphere.Ppm, 100.f);
	Ship.Mode->ShipRepairModule(TEXT("LifeSupport"), 100);
	TestFalse(TEXT("Repair restores scrubbing"), Ship.State->Atmosphere.bScrubDisabled);
	TestTrue(TEXT("Prior selected mode restored"), Ship.Mode->AirGetScrubMode() == EScrubMode::Normal);
	int32 Notifications = 0; const auto H = Ship.State->OnShipGraphChanged.AddLambda([&Notifications] { ++Notifications; });
	TestTrue(TEXT("Same mode accepted"), Crew->SetScrubModeServer(EScrubMode::Normal, false));
	TestEqual(TEXT("Same mode publishes nothing"), Notifications, 0); Ship.State->OnShipGraphChanged.Remove(H);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirHubTest, "Airlock.Atmosphere.HubHasNoAirOrExposureTick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirHubTest::RunTest(const FString& Parameters)
{
	FAirTestWorld Hub(TEXT("StationHub")); auto* Crew = Hub.CrewAt();
	TestFalse(TEXT("Hub lacks atmosphere server"), Hub.Mode->AirHasAtmosphere()); TestFalse(TEXT("Hub lacks atmosphere client predicate"), Hub.State->AirHasAtmosphere());
	TestEqual(TEXT("Hub summary"), Hub.Mode->AirSummary(), FString(TEXT("No atmosphere on this section")));
	TestFalse(TEXT("Hub ppm rejected"), Hub.Mode->AirSetPpm(100)); TestFalse(TEXT("Hub mode rejected"), Hub.Mode->AirSetScrubMode(EScrubMode::Hazard));
	TestFalse(TEXT("Hub scrub rejected"), Crew->SetScrubModeServer(EScrubMode::Hazard, false));
	TestTrue(TEXT("Exposure number can be set on hub"), Crew->SetExposureServer(100)); Hub.Activate(); Crew->Tick(1);
	TestEqual(TEXT("No exposure step on hub"), Crew->Exposure, 100.f); TestFalse(TEXT("Hub never blacks out"), Crew->bBlackedOut);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirResultTextTest, "Airlock.Atmosphere.HubBoardAirResultText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirResultTextTest::RunTest(const FString& Parameters)
{
	FVoyageResult R; R.CratesIntact = 1; R.CratesTotal = 4; R.ShipCondition = 100; R.Pay = 400;
	const FString Base = TEXT("Last run: 1/4 crates intact - hull 100% - pay 400 cr");
	TestEqual(TEXT("No air preserves old text"), UVoyageBoardWidget::DescribeResult(R).ToString(), Base);
	R.DockPpm = 4; R.bCleanAir = true;
	TestEqual(TEXT("Clean air suffix"), UVoyageBoardWidget::DescribeResult(R).ToString(), Base + TEXT(" - air clean"));
	R.DockPpm = 30.9f; R.bCleanAir = false;
	TestEqual(TEXT("Dirty air suffix floors ppm"), UVoyageBoardWidget::DescribeResult(R).ToString(), Base + TEXT(" - air 30 ppm"));
	return true;
}
#endif
