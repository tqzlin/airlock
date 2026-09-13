// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Ship/ShipVisuals.h"
#include "Ship/ShipModule.h"
#include "Ship/CargoLayouts.h"
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"
#include "Ship/ShipLine.h"
#include "Ship/FuelValve.h"
#include "Ship/BypassValve.h"
#include "Ship/FuelIntake.h"
#include "Ship/ControlStation.h"
#include "Station/StationSign.h"
#include "UI/TaskBoardWidget.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystemComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FLegibilityTestWorld
{
	UWorld* World;
	AAirlockGameMode* Mode;
	AAirlockGameState* State;
	explicit FLegibilityTestWorld(FName Section = TEXT("Hauler_Mk1"))
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World));
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>();
		State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = Section;
		World->BeginPlay();
	}
	~FLegibilityTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	template<class T> T* Find(FName Id) const
	{
		for (TActorIterator<T> It(World); It; ++It) { if (It->GetFName() == Id) { return *It; } }
		return nullptr;
	}
	AAirlockCharacter* CrewAt(FVector Location) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Crew = World->SpawnActor<AAirlockCharacter>(Params);
		Crew->SetActorLocation(Location);
		Crew->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		return Crew;
	}
	void AddFloor() const
	{
		auto* Floor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(1000, 1000, 10));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0, 0, -10));
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigureLabelTest, "Airlock.Legibility.ConfigureLabel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FConfigureLabelTest::RunTest(const FString& Parameters)
{
    FLegibilityTestWorld Env(TEXT("StationHub"));
    auto* Actor = Env.World->SpawnActor<AActor>();
    auto* Root = NewObject<USceneComponent>(Actor);
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();
    Actor->SetActorRotation(FRotator(0, 35, 0));
    auto* Label = NewObject<UTextRenderComponent>(Actor);
    Label->SetupAttachment(Root);
    Label->RegisterComponent();
    const FVector Offset(4, 5, 210);
    const FLinearColor Colour(1.f, .6f, .1f);
    ShipVisuals::ConfigureLabel(Label, 22.f, -90.f, Colour, Offset);
    TestEqual(TEXT("World size"), Label->WorldSize, 22.f);
    TestTrue(TEXT("Horizontal centre"), Label->HorizontalAlignment == EHTA_Center);
    TestTrue(TEXT("Vertical text centre"), Label->VerticalAlignment == EVRTA_TextCenter);
    TestEqual(TEXT("Offset unchanged"), Label->GetRelativeLocation(), Offset);
    TestTrue(TEXT("Yaw relative to current owner"), FMath::IsNearlyEqual(Label->GetRelativeRotation().Yaw, -125.0));
    TestTrue(TEXT("World facing"), FMath::IsNearlyEqual(Label->GetComponentRotation().Yaw, -90.0));
    TestEqual(TEXT("Linear colour converted to sRGB"), Label->TextRenderColor, Colour.ToFColor(true));
    TestTrue(TEXT("No collision"), Label->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Unlit text material (M5-5)"), Label->TextMaterial && Label->TextMaterial->GetName() == TEXT("UnlitText"));
    Actor->SetActorRotation(FRotator(0, -50, 0));
    ShipVisuals::ConfigureLabel(Label, 22.f, -90.f, Colour, Offset);
    TestTrue(TEXT("Reapply uses new owner yaw"), FMath::IsNearlyEqual(Label->GetRelativeRotation().Yaw, -40.0));
    // The world-centimetre variant on a scaled owner (M5-5): the text and its offset measure what they say; the offset
    // still turns with the owner (the actor is at yaw -50 here), like every other label's.
    Actor->SetActorScale3D(FVector(.8f));
    ShipVisuals::ConfigureLabelUnscaled(Label, 18.f, -90.f, Colour, FVector(0, -125, 185));
    TestEqual(TEXT("Unscaled keeps the world size"), Label->WorldSize, 18.f);
    TestTrue(TEXT("Unscaled offset in world centimetres"), (Label->GetComponentLocation() - Actor->GetActorLocation())
        .Equals(Actor->GetActorRotation().RotateVector(FVector(0, -125, 185)), .5f));
    TestTrue(TEXT("Unscaled text renders at one"), Label->GetComponentScale().Equals(FVector::OneVector, .001f));
    Actor->SetActorScale3D(FVector(.3f, .3f, .15f));
    ShipVisuals::ConfigureLabelUnscaled(Label, 8.f, -90.f, Colour, FVector(0, -12, 98));
    TestTrue(TEXT("Per-axis owner scale compensated"), (Label->GetComponentLocation() - Actor->GetActorLocation())
        .Equals(Actor->GetActorRotation().RotateVector(FVector(0, -12, 98)), .5f));
    TestTrue(TEXT("Per-axis text scale compensated"), Label->GetComponentScale().Equals(FVector::OneVector, .001f));
    TestTrue(TEXT("Still facing the world yaw"), FMath::IsNearlyEqual(Label->GetComponentRotation().Yaw, -90.0, .1));
    auto* Ownerless = NewObject<UTextRenderComponent>();
    ShipVisuals::ConfigureLabel(Ownerless, 34.f, 90.f, FLinearColor::White, FVector::ZeroVector);
    TestTrue(TEXT("Ownerless yaw"), FMath::IsNearlyEqual(Ownerless->GetRelativeRotation().Yaw, 90.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegibilityLifeSupportTest, "Airlock.Legibility.LifeSupportKeepsScrubberLine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLegibilityLifeSupportTest::RunTest(const FString& Parameters)
{
    FLegibilityTestWorld Env;
    auto* Life = Env.Find<AShipModule>(TEXT("LifeSupport"));
    if (!TestNotNull(TEXT("Life support spawned"), Life)) { return false; }
    TestEqual(TEXT("Id, state and scrubber on separate lines"), Life->Label->Text.ToString(), FString(TEXT("LifeSupport\nNominal  100%\nScrubbers Normal")));
    Env.Mode->AirSetScrubMode(EScrubMode::Hazard);
    TestEqual(TEXT("Hazard mode still updates third line"), Life->Label->Text.ToString(), FString(TEXT("LifeSupport\nNominal  100%\nScrubbers Hazard")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoLabelYawTest, "Airlock.Legibility.CargoLabelsFaceWalkway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoLabelYawTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("North faces south"), CargoLayouts::LabelYawFor(1560.001), -90.f);
	TestEqual(TEXT("Centre faces north"), CargoLayouts::LabelYawFor(1560), 90.f);
	TestEqual(TEXT("South faces north"), CargoLayouts::LabelYawFor(1559.999), 90.f);
	TestEqual(TEXT("Hand-built crate default"), FCargoCrateSpec().LabelYaw, 180.f);
	TestEqual(TEXT("Hand-built slot default"), FCargoSlotSpec().LabelYaw, 180.f);
	const auto Layout = CargoLayouts::Build(TEXT("Hauler_Mk1"), 12, 6);
	TestEqual(TEXT("All twelve crate spots"), Layout.Crates.Num(), 12);
	TestEqual(TEXT("All ten slot spots"), Layout.Slots.Num(), 10);
	for (const auto& Crate : Layout.Crates)
	{
		TestEqual(*Crate.Id.ToString(), Crate.LabelYaw, Crate.Transform.GetLocation().Y > 1560 ? -90.f : 90.f);
	}
	for (const auto& Slot : Layout.Slots)
	{
		TestEqual(*Slot.Id.ToString(), Slot.LabelYaw, Slot.Transform.GetLocation().Y > 1560 ? -90.f : 90.f);
		if (Slot.bRack) { TestEqual(TEXT("Rack faces north"), Slot.LabelYaw, 90.f); }
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrateLabelRulesTest, "Airlock.Legibility.CarriedDroppedRackedAndTerminalLabels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCrateLabelRulesTest::RunTest(const FString& Parameters)
{
	FLegibilityTestWorld Env;
	auto* Crate = Env.Find<ACargoCrate>(TEXT("C2"));
	auto* Rack = Env.Find<ACargoSlot>(TEXT("ColdRack"));
	if (!TestNotNull(TEXT("Cask spawned"), Crate) || !TestNotNull(TEXT("Rack spawned"), Rack)) { return false; }
	TestEqual(TEXT("White id on first line"), Crate->Label->Text.ToString(), FString(TEXT("C2")));
	TestEqual(TEXT("Volatile state on second line"), Crate->StateLabel->Text.ToString(), FString(TEXT("Loose  0C")));
	TestEqual(TEXT("Id height retained"), Crate->Label->GetRelativeLocation().Z, 95.0);
	for (float Temperature : { 39.f, 40.f, 79.f, 80.f })
	{
		Env.Mode->CargoSetTemperature(TEXT("C2"), Temperature);
		const FLinearColor Colour = Temperature >= 80 ? FLinearColor(1.f, .15f, .1f)
			: Temperature >= 40 ? FLinearColor(1.f, .6f, .1f) : FLinearColor::White;
		TestEqual(TEXT("Heat colour on state only"), Crate->StateLabel->TextRenderColor, Colour.ToFColor(true));
		TestEqual(TEXT("Id stays white"), Crate->Label->TextRenderColor, FColor::White);
	}
	auto* Crew = Env.CrewAt(Crate->GetActorLocation() + FVector(-150, 0, 96));
	Crew->SetActorRotation(FRotator(0, 37, 0));
	Crew->ServerPickUp(Crate);
	TestTrue(TEXT("Picked up through server verb"), Crew->CarriedCrate == Crate);
	TestFalse(TEXT("Carried id hidden"), Crate->Label->IsVisible());
	TestFalse(TEXT("Carried state hidden"), Crate->StateLabel->IsVisible());
	Crew->ServerPutDown();
	TestTrue(TEXT("Drop restores both lines"), Crate->Label->IsVisible() && Crate->StateLabel->IsVisible());
	TestTrue(TEXT("Drop uses pawn yaw"), FMath::IsNearlyEqual(Crate->GetActorRotation().Yaw, 37.0));
	TestTrue(TEXT("Drop retains world label facing"), FMath::IsNearlyEqual(Crate->Label->GetComponentRotation().Yaw, -90.0));
	Crew->ServerPickUp(Crate);
	Crew->SetActorLocation(Rack->GetActorLocation() + FVector(0, 150, 96));
	Crew->ServerPlace(Rack);
	TestTrue(TEXT("Placed through server verb"), Crate->GetCargoState()->State == ECargoState::Racked);
	TestTrue(TEXT("Rack restores both lines"), Crate->Label->IsVisible() && Crate->StateLabel->IsVisible());
	TestTrue(TEXT("Rack takes rack yaw"), FMath::IsNearlyEqual(Crate->GetActorRotation().Yaw, 90.0));
	// M5-5: a cask faces the walkway from where it stands - the rack is south of it, so the racked cask turns to face north.
	TestTrue(TEXT("Rack turns the id to face the walkway"), FMath::IsNearlyEqual(Crate->Label->GetComponentRotation().Yaw, 90.0));
	TestTrue(TEXT("Rack turns the state to face the walkway"), FMath::IsNearlyEqual(Crate->StateLabel->GetComponentRotation().Yaw, 90.0));
	// Replicated terminal snapshots exercise visuals without changing any model or gameplay helper.
	auto Cargo = Env.State->Cargo;
	auto* State = Cargo.FindByPredicate([](const auto& C) { return C.Id == TEXT("C2"); });
	State->State = ECargoState::Vented; State->Temperature = 100;
	Env.State->PublishCargo(Cargo);
	TestTrue(TEXT("Vented label remains"), Crate->Label->IsVisible() && Crate->StateLabel->IsVisible());
	TestEqual(TEXT("Vented state red"), Crate->StateLabel->TextRenderColor, FLinearColor(1.f, .15f, .1f).ToFColor(true));
	State->State = ECargoState::Burned;
	Env.State->PublishCargo(Cargo);
	TestTrue(TEXT("Burned actor hidden"), Crate->IsHidden());
	TestFalse(TEXT("Burned id hidden"), Crate->Label->IsVisible());
	TestFalse(TEXT("Burned state hidden"), Crate->StateLabel->IsVisible());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLooseCargoSummaryTest, "Airlock.Legibility.LooseSummaryThresholdAndLegacyRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLooseCargoSummaryTest::RunTest(const FString& Parameters)
{
	TArray<FCargoCrateState> Cargo;
	for (int32 I = 1; I <= 5; ++I)
	{
		FCargoCrateState Crate; Crate.Id = *FString::Printf(TEXT("C%d"), I); Cargo.Add(Crate);
		const auto Rows = UTaskBoardWidget::BuildRows({}, {}, Cargo, nullptr, nullptr, true);
		const auto Legacy = UTaskBoardWidget::BuildRows({}, {}, Cargo);
		TestEqual(TEXT("Default still has one row per loose cask"), Legacy.Num(), I);
		if (I <= 4)
		{
			TestEqual(TEXT("Four or fewer keep individual rows"), Rows.Num(), I);
			for (int32 R = 0; R < I; ++R) { TestEqual(TEXT("Individual words unchanged"), Rows[R].Text.ToString(), Legacy[R].Text.ToString()); }
		}
		else
		{
			TestEqual(TEXT("Five become one row"), Rows.Num(), 1);
			TestEqual(TEXT("Loose summary wording"), Rows[0].Text.ToString(), FString(TEXT("Secure 5 loose casks")));
			TestFalse(TEXT("Loose summary not red"), Rows[0].bFailed);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeatingCargoSummaryTest, "Airlock.Legibility.HeatingSummaryThresholdOrderAndTerminalExclusions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHeatingCargoSummaryTest::RunTest(const FString& Parameters)
{
	FCargoModel Model; Model.Build(CargoLayouts::Build(TEXT("Hauler_Mk1"), 12, 6));
	auto Cargo = Model.Crates();
	Cargo[1].Temperature = 70; Cargo[3].Temperature = 71.9f;
	auto Rows = UTaskBoardWidget::BuildRows({}, {}, Cargo, nullptr, nullptr, true);
	TestEqual(TEXT("Two hot casks stay individual"), Rows[0].Text.ToString(), FString(TEXT("Cool crate C4 - 71C")));
	TestEqual(TEXT("Second hot cask"), Rows[1].Text.ToString(), FString(TEXT("Cool crate C2 - 70C")));
	TestEqual(TEXT("Ten eligible loose casks"), Rows[2].Text.ToString(), FString(TEXT("Secure 10 loose casks")));
	Cargo[5].Temperature = 60;
	Rows = UTaskBoardWidget::BuildRows({}, {}, Cargo, nullptr, nullptr, true);
	TestEqual(TEXT("Three hot casks summarized and floored"), Rows[0].Text.ToString(), FString(TEXT("Cool 3 casks - hottest C4 71C")));
	TestFalse(TEXT("Below 80 not red"), Rows[0].bFailed);
	TestEqual(TEXT("No duplicate loose tasks for hot casks"), Rows[1].Text.ToString(), FString(TEXT("Secure 9 loose casks")));
	const auto Legacy = UTaskBoardWidget::BuildRows({}, {}, Cargo);
	TestEqual(TEXT("Default retains old overflow"), Legacy.Last().Text.ToString(), FString(TEXT("+4 more")));
	TestEqual(TEXT("Default hottest"), Legacy[0].Text.ToString(), FString(TEXT("Cool crate C4 - 71C")));
	TestEqual(TEXT("Default second"), Legacy[1].Text.ToString(), FString(TEXT("Cool crate C2 - 70C")));
	TestEqual(TEXT("Default third"), Legacy[2].Text.ToString(), FString(TEXT("Cool crate C6 - 60C")));
	Cargo[3].Temperature = 80;
	TestTrue(TEXT("Summary red exactly at 80"), UTaskBoardWidget::BuildRows({}, {}, Cargo, nullptr, nullptr, true)[0].bFailed);
	Cargo[1].Temperature = 80;
	TestEqual(TEXT("Tied hottest keeps manifest order"), UTaskBoardWidget::BuildRows({}, {}, Cargo, nullptr, nullptr, true)[0].Text.ToString(), FString(TEXT("Cool 3 casks - hottest C2 80C")));
	Cargo[1].State = ECargoState::Carried; Cargo[3].State = ECargoState::Secured;
	for (const auto State : { ECargoState::Vented, ECargoState::Burned, ECargoState::Racked })
	{
		FCargoCrateState Extra; Extra.Id = TEXT("Excluded"); Extra.State = State; Extra.bVolatile = true; Extra.Temperature = 100; Cargo.Add(Extra);
	}
	FCargoCrateState Carried; Carried.Id = TEXT("CoolCarried"); Carried.State = ECargoState::Carried; Cargo.Add(Carried);
	Rows = UTaskBoardWidget::BuildRows({}, {}, Cargo, nullptr, nullptr, true);
	TestEqual(TEXT("Hot carried and secured still count; terminals and rack do not"), Rows[0].Text.ToString(), FString(TEXT("Cool 3 casks - hottest C2 80C")));
	TestEqual(TEXT("Excluded states never inflate loose summary"), Rows[1].Text.ToString(), FString(TEXT("Secure 9 loose casks")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSummaryPriorityTest, "Airlock.Legibility.SummariesKeepHazardPriorityAndOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSummaryPriorityTest::RunTest(const FString& Parameters)
{
	FCargoModel Cargo; Cargo.Build(CargoLayouts::Build(TEXT("Hauler_Mk1"), 12, 6));
	Cargo.SetTemperature(TEXT("C2"), 70); Cargo.SetTemperature(TEXT("C4"), 85); Cargo.SetTemperature(TEXT("C6"), 60);
	FFuelState Fuel; Fuel.bFeedLeak = true; Fuel.bInjectorsFouled = true;
	FAtmosphereState Air; Air.Ppm = 22;
	FShipModuleState Failed; Failed.Id = TEXT("Engines"); Failed.Condition = 10; Failed.Status = EShipModuleStatus::Failed;
	FShipModuleState Degraded; Degraded.Id = TEXT("PowerCore"); Degraded.Condition = 42; Degraded.Status = EShipModuleStatus::Degraded;
	FShipLineState Line; Line.Id = TEXT("Coolant_1"); Line.Fault = EShipLineFault::Leak;
	auto Rows = UTaskBoardWidget::BuildRows({ Failed, Degraded }, { Line }, Cargo.Crates(), &Air, &Fuel, true);
	const TArray<FString> Expected = { TEXT("Fuel leak - shut the feed valve"), TEXT("Hold air 22 ppm - scrub"),
		TEXT("Clear the injectors"), TEXT("Cool 3 casks - hottest C4 85C"), TEXT("Engines - Failed 10%"),
		TEXT("Tape leak - Coolant_1"), TEXT("PowerCore - Degraded 42%"), TEXT("Secure 9 loose casks") };
	if (!TestEqual(TEXT("Eight distinct tasks"), Rows.Num(), Expected.Num())) { return false; }
	for (int32 I = 0; I < Expected.Num(); ++I) { TestEqual(TEXT("Priority"), Rows[I].Text.ToString(), Expected[I]); }
	Rows = UTaskBoardWidget::BuildRows({ Failed, Failed, Failed, Degraded }, { Line }, Cargo.Crates(), &Air, &Fuel, true);
	TestEqual(TEXT("Summary path still caps at eight plus overflow"), Rows.Num(), 9);
	TestEqual(TEXT("Overflow counts display rows"), Rows.Last().Text.ToString(), FString(TEXT("+2 more")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskReadoutTest, "Airlock.Legibility.ReadoutPartsFlooringAndClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTaskReadoutTest::RunTest(const FString& Parameters)
{
	FAtmosphereState Air; Air.Ppm = 22.9f;
	FFuelState Fuel; Fuel.Units = 80.9f;
	TestEqual(TEXT("Full field readout"), UTaskBoardWidget::BuildReadout(&Air, &Fuel, 1169).ToString(), FString(TEXT("AIR 22 ppm  |  FUEL 80%  |  19:29 to dock")));
	TestEqual(TEXT("Air only"), UTaskBoardWidget::BuildReadout(&Air, nullptr, -1).ToString(), FString(TEXT("AIR 22 ppm")));
	TestEqual(TEXT("Fuel only"), UTaskBoardWidget::BuildReadout(nullptr, &Fuel, -1).ToString(), FString(TEXT("FUEL 80%")));
	TestEqual(TEXT("Clock only at dock"), UTaskBoardWidget::BuildReadout(nullptr, nullptr, 0).ToString(), FString(TEXT("00:00 to dock")));
	TestEqual(TEXT("Air and fuel without clock"), UTaskBoardWidget::BuildReadout(&Air, &Fuel, -10).ToString(), FString(TEXT("AIR 22 ppm  |  FUEL 80%")));
	TestEqual(TEXT("Air and clock"), UTaskBoardWidget::BuildReadout(&Air, nullptr, 9).ToString(), FString(TEXT("AIR 22 ppm  |  00:09 to dock")));
	TestEqual(TEXT("Fuel and clock"), UTaskBoardWidget::BuildReadout(nullptr, &Fuel, 60).ToString(), FString(TEXT("FUEL 80%  |  01:00 to dock")));
	TestTrue(TEXT("Nothing gives empty text"), UTaskBoardWidget::BuildReadout(nullptr, nullptr, -1).IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegibilityFixtureTest, "Airlock.Legibility.FixtureLabelsAndHazardVisuals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLegibilityFixtureTest::RunTest(const FString& Parameters)
{
	FLegibilityTestWorld Env;
	auto* Crate = Env.Find<ACargoCrate>(TEXT("C1"));
	auto* Line = Env.Find<AShipLine>(TEXT("Coolant_1"));
	auto* Feed = Env.Find<AFuelValve>(TEXT("FeedValve"));
	auto* Bypass = Env.Find<ABypassValve>(TEXT("BypassValve"));
	auto* Intake = Env.Find<AFuelIntake>(TEXT("FuelIntake"));
	auto* Core = Env.Find<AShipModule>(TEXT("PowerCore"));
	if (!TestNotNull(TEXT("Crate"), Crate) || !TestNotNull(TEXT("Line"), Line) || !TestNotNull(TEXT("Feed"), Feed)
		|| !TestNotNull(TEXT("Bypass"), Bypass) || !TestNotNull(TEXT("Intake"), Intake) || !TestNotNull(TEXT("Core"), Core)) { return false; }
	TestEqual(TEXT("Nonvolatile second line"), Crate->StateLabel->Text.ToString(), FString(TEXT("Loose")));
	TestEqual(TEXT("Line words on two lines"), Line->Label->Text.ToString(), FString(TEXT("Coolant_1\nNone")));
	TestEqual(TEXT("Core words on two lines"), Core->Label->Text.ToString(), FString(TEXT("PowerCore\nNominal  100%")));
	TestEqual(TEXT("Feed words on two lines"), Feed->Label->Text.ToString(), FString(TEXT("FEED VALVE\nshut")));
	TestEqual(TEXT("Bypass words on two lines"), Bypass->Label->Text.ToString(), FString(TEXT("BYPASS\nclosed")));
	TestEqual(TEXT("Intake words on two lines"), Intake->Label->Text.ToString(), FString(TEXT("INTAKE\nbypass closed")));
	Env.State->Fuel.bFeedLeak = true; Env.State->Fuel.bBypassOpen = true;
	Env.State->Fuel.BurningCrate = TEXT("C3"); Env.State->Fuel.BurnSecondsLeft = 12;
	Env.State->OnShipGraphChanged.Broadcast();
	TestEqual(TEXT("Leaking feed"), Feed->Label->Text.ToString(), FString(TEXT("FEED VALVE\nleak")));
	TestEqual(TEXT("Open bypass"), Bypass->Label->Text.ToString(), FString(TEXT("BYPASS\nOPEN")));
	TestEqual(TEXT("Burn countdown"), Intake->Label->Text.ToString(), FString(TEXT("INTAKE\nC3 12 s")));
	Env.State->Fuel.BurningCrate = NAME_None; Env.State->OnShipGraphChanged.Broadcast();
	TestEqual(TEXT("Ready intake"), Intake->Label->Text.ToString(), FString(TEXT("INTAKE\nready")));
	TestEqual(TEXT("Crate light stays local"), Crate->HeatLight->AttenuationRadius, 120.f);
	TestEqual(TEXT("Module light stays local"), Core->StatusLight->AttenuationRadius, 100.f);
	TestEqual(TEXT("Module light intensity"), Core->StatusLight->Intensity, 12.f);
	TestEqual(TEXT("Valve light unchanged"), Feed->LeakLight->Intensity, 80.f);
	TestEqual(TEXT("Valve light radius unchanged"), Feed->LeakLight->AttenuationRadius, 140.f);
	TestEqual(TEXT("Crate steam raised"), Crate->VentFX->GetRelativeLocation().Z, 80.0);
	TestEqual(TEXT("Line steam raised"), Line->LeakFX->GetRelativeLocation().Z, 150.0);
	TestEqual(TEXT("Feed steam raised"), Feed->LeakFX->GetRelativeLocation().Z, 130.0);
	for (auto* Steam : { Crate->VentFX.Get(), Line->LeakFX.Get(), Feed->LeakFX.Get() })
	{
		TestEqual(TEXT("Steam scaled on component"), Steam->GetRelativeScale3D(), FVector(2.5));
	}
	for (auto* Text : { Core->Label.Get(), Line->Label.Get(), Crate->Label.Get(), Crate->StateLabel.Get() })
	{
		TestEqual(TEXT("Labels use 22 cm"), Text->WorldSize, 22.f);
		TestTrue(TEXT("Labels vertically centred"), Text->VerticalAlignment == EVRTA_TextCenter);
	}
	// Reviewer tuning (2026-09-12): the fixtures are read from the engineering walkway 130-250 cm away, where 22 cm filled the frame.
	for (auto* Text : { Feed->Label.Get(), Bypass->Label.Get(), Intake->Label.Get() })
	{
		TestEqual(TEXT("Fixture labels use 14 cm"), Text->WorldSize, 14.f);
		TestTrue(TEXT("Fixture labels vertically centred"), Text->VerticalAlignment == EVRTA_TextCenter);
	}
	for (TActorIterator<ACargoSlot> It(Env.World); It; ++It)
	{
		TestEqual(TEXT("Slot id and empty state"), It->Label->Text.ToString(), It->GetSlotId().ToString() + TEXT("\nempty"));
	}
	return true;
}
#endif
