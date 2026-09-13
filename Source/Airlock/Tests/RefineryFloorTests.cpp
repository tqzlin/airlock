// Copyright Airlock project. All Rights Reserved.
// M5-2: the refinery floor section - layouts, Okafor's schedule, the ship result hand-over, the combined tally.
// M5-3a: the refinery model on the floor - the mirror, the verbs, a batch by world ticks, the result, the board and the labels.
#include "Misc/AutomationTest.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Ship/CargoLayouts.h"
#include "Ship/CargoModel.h"
#include "Ship/CargoCrate.h"
#include "Ship/FloorFixture.h"
#include "Ship/RefineryLayouts.h"
#include "Ship/RefineryModel.h"
#include "UI/TaskBoardWidget.h"
#include "Ship/ShipLayouts.h"
#include "Ship/ShipModule.h"
#include "Station/StationSign.h"
#include "UI/VoyageBoardWidget.h"
#include "Voyage/FloorSchedule.h"
#include "Voyage/VoyageSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
/** A section world with a pending voyage; the game instance is created first so a ship result can be stored on it. */
struct FFloorTestWorld
{
	UWorld* World; AAirlockGameMode* Mode; AAirlockGameState* State; UVoyageSubsystem* Voyages;
	explicit FFloorTestWorld(FName Section, const FVoyageResult* ShipResult)
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World)); World->GetGameInstance()->Init();
		Voyages = World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>();
		FVoyageSpec Spec = UVoyageSubsystem::MakeVoyageSpec(EVoyageType::MilkRun, 1); FGuid Id;
		Voyages->RequestVoyage(Spec, Id);
		if (ShipResult) { Voyages->SetShipResult(*ShipResult); }
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>(); State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = Section; World->BeginPlay();
	}
	~FFloorTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->GetGameInstance()->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
	template<class T> int32 Count() const { int32 N = 0; for (TActorIterator<T> It(World); It; ++It) { ++N; } return N; }
	template<class T> T* Find(FName Name) const { for (TActorIterator<T> It(World); It; ++It) { if (It->GetFName() == Name) { return *It; } } return nullptr; }
	/** Runs the world in exact quarter-second frames; the frame counter advances so the timer manager ticks each one. */
	void Tick(float Seconds) const { for (int32 I = 0, N = FMath::RoundToInt(Seconds * 4.f); I < N; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .25f); } }
};
FVoyageResult ThreeCaskShipResult()
{
	FVoyageResult R; R.CratesTotal = 4; R.CratesIntact = 3; R.CratesLoose = 1; R.Pay = 450; R.FuelSurplus = 30;
	R.IntactCasks = { TEXT("C1"), TEXT("C2"), TEXT("C4") }; R.IntactLightCasks = { TEXT("C2"), TEXT("C4") };
	return R;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorLayoutsTest, "Airlock.Floor.LayoutsAndSigns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFloorLayoutsTest::RunTest(const FString& Parameters)
{
	const FName Floor = RefineryLayouts::FloorSectionId();
	TestEqual(TEXT("Floor section id"), Floor, FName(TEXT("RefineryFloor")));
	// Staging spots: twelve at most, two rows of six, 160 cm apart, the light casks volatile, labels facing the walkway, no slots.
	TArray<FName> Casks; for (int32 I = 1; I <= 14; ++I) { Casks.Add(FName(*FString::Printf(TEXT("C%d"), I))); }
	const FCargoLayout Twelve = CargoLayouts::BuildFloor(Casks, { TEXT("C2"), TEXT("C13") });
	TestEqual(TEXT("Twelve spots at most"), Twelve.Crates.Num(), 12);
	TestEqual(TEXT("No slots on the floor"), Twelve.Slots.Num(), 0);
	TestEqual(TEXT("Floor section"), Twelve.SectionId, Floor);
	TestTrue(TEXT("C2 is light"), Twelve.Crates[1].bVolatile); TestFalse(TEXT("C1 is heavy"), Twelve.Crates[0].bVolatile);
	TestEqual(TEXT("Row pitch"), Twelve.Crates[1].Transform.GetLocation().X - Twelve.Crates[0].Transform.GetLocation().X, 160.0, 0.1);
	TestEqual(TEXT("Second row"), Twelve.Crates[6].Transform.GetLocation().Y, 1250.0, 0.1);
	TestEqual(TEXT("Labels face the walkway"), Twelve.Crates[0].LabelYaw, 90.f);
	const FCargoLayout Three = CargoLayouts::BuildFloor({ TEXT("C1"), TEXT("C2"), TEXT("C4") }, { TEXT("C2"), TEXT("C4") });
	TestEqual(TEXT("Three casks"), Three.Crates.Num(), 3); TestEqual(TEXT("Ids kept"), Three.Crates[2].Id, FName(TEXT("C4")));
	const FCargoLayout Empty = CargoLayouts::BuildFloor({}, {});
	TestEqual(TEXT("Empty floor"), Empty.Crates.Num(), 0);
	TestNotNull(TEXT("Find knows the floor (air and a cargo model)"), CargoLayouts::Find(Floor));
	TestNotNull(TEXT("Find still knows the ship"), CargoLayouts::Find(TEXT("Hauler_Mk1")));
	// The scrubber panel is the floor's only ship module.
	const FShipLayout* Ship = ShipLayouts::Find(Floor);
	TestNotNull(TEXT("Floor ship layout"), Ship); if (!Ship) { return false; }
	TestEqual(TEXT("One module"), Ship->Modules.Num(), 1); TestEqual(TEXT("No lines"), Ship->Lines.Num(), 0); TestEqual(TEXT("No stations"), Ship->Stations.Num(), 0);
	TestEqual(TEXT("It is life support"), Ship->Modules[0].Type, EShipModuleType::LifeSupport);
	// Fixtures and signs.
	const FRefineryLayout* Fixtures = RefineryLayouts::Find(Floor);
	TestNotNull(TEXT("Fixture table"), Fixtures); if (!Fixtures) { return false; }
	int32 Stills = 0; for (const auto& F : Fixtures->Fixtures) { Stills += F.Kind == EFloorFixtureKind::Still; }
	TestEqual(TEXT("Three stills"), Stills, 3); TestEqual(TEXT("Six fixtures"), Fixtures->Fixtures.Num(), 6);
	TestNull(TEXT("No fixtures on the ship"), RefineryLayouts::Find(TEXT("Hauler_Mk1")));
	TestEqual(TEXT("Three signs"), StationSigns::Find(Floor).Num(), 3);
	TestEqual(TEXT("Hub signs unchanged"), StationSigns::Find(TEXT("StationHub")).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorScheduleTest, "Airlock.Floor.OkaforSchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFloorScheduleTest::RunTest(const FString& Parameters)
{
	const TArray<FVoyageAnnouncement> Lines = FFloorSchedule::Build();
	TestEqual(TEXT("Slot length"), FFloorSchedule::SlotSeconds, 600.f);
	TestEqual(TEXT("Four lines"), Lines.Num(), 4);
	if (Lines.Num() != 4) { return false; }
	TestEqual(TEXT("At 0"), Lines[0].AtSeconds, 0.f); TestEqual(TEXT("At 300"), Lines[1].AtSeconds, 300.f);
	TestEqual(TEXT("At 540"), Lines[2].AtSeconds, 540.f); TestEqual(TEXT("At 600"), Lines[3].AtSeconds, 600.f);
	TestTrue(TEXT("Okafor speaks first"), Lines[0].Line.ToString().StartsWith(TEXT("Okafor")));
	TestTrue(TEXT("The horn ends it"), Lines[3].Line.ToString().StartsWith(TEXT("Horn")));
	TestEqual(TEXT("Speaker"), FString(FFloorSchedule::Speaker()), FString(TEXT("Okafor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorResultTest, "Airlock.Floor.ShipResultAndCombinedTally",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFloorResultTest::RunTest(const FString& Parameters)
{
	// The tally lists the intact casks in manifest order, light ones separately.
	FCargoModel Cargo; Cargo.Build(CargoLayouts::Build(TEXT("Hauler_Mk1"), 4, 2));
	Cargo.PickUp(TEXT("C1")); Cargo.Place(TEXT("C1"), TEXT("Clamp_1"));
	Cargo.PickUp(TEXT("C2")); Cargo.Place(TEXT("C2"), TEXT("ColdRack"));
	FShipGraph Ship; Ship.Build(*ShipLayouts::Find(TEXT("Hauler_Mk1")));
	const FVoyageResult Tally = FCargoModel::Tally(Cargo, Ship, UVoyageSubsystem::MakeVoyageSpec(EVoyageType::MilkRun, 1));
	TestEqual(TEXT("Two intact"), Tally.IntactCasks.Num(), 2);
	if (Tally.IntactCasks.Num() == 2) { TestEqual(TEXT("C1 first"), Tally.IntactCasks[0], FName(TEXT("C1"))); TestEqual(TEXT("C2 second"), Tally.IntactCasks[1], FName(TEXT("C2"))); }
	TestEqual(TEXT("One light cask"), Tally.IntactLightCasks.Num(), 1);
	if (Tally.IntactLightCasks.Num() == 1) { TestEqual(TEXT("C2 is the light one"), Tally.IntactLightCasks[0], FName(TEXT("C2"))); }
	TestFalse(TEXT("No floor yet"), Tally.bHasFloor);
	TestFalse(TEXT("Summary unchanged without a floor"), Tally.Summary().Contains(TEXT("floor")));

	// The combined summary and the board line.
	FVoyageResult Combined = ThreeCaskShipResult();
	Combined.bHasFloor = true; Combined.FloorCasksTotal = 3; Combined.FloorCasksRefined = 0; Combined.FloorCasksUnrefined = 3; Combined.FloorPay = 120; Combined.Pay += 120;
	TestTrue(TEXT("Summary carries the floor"), Combined.Summary().EndsWith(TEXT(" | floor: 0/3 casks refined, 0 L diesel, pay 120")));
	TestTrue(TEXT("Board line carries the floor"), UVoyageBoardWidget::DescribeResult(Combined).ToString().EndsWith(TEXT(" - floor: 0/3 refined")));

	// The ship result round trip on the subsystem: kept, read back, cleared; never saved by itself.
	UGameInstance* Outer = NewObject<UGameInstance>(GEngine);
	UVoyageSubsystem* Voyages = NewObject<UVoyageSubsystem>(Outer);
	FVoyageResult Read;
	TestFalse(TEXT("Nothing before"), Voyages->GetShipResult(Read));
	Voyages->SetShipResult(ThreeCaskShipResult());
	TestTrue(TEXT("Read back"), Voyages->GetShipResult(Read)); TestEqual(TEXT("Three intact"), Read.IntactCasks.Num(), 3);
	TestFalse(TEXT("Not the last result"), Voyages->GetLastVoyageResult(Read));
	Voyages->ClearShipResult();
	TestFalse(TEXT("Cleared"), Voyages->GetShipResult(Read));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorSectionTest, "Airlock.Floor.SectionBeginsPlayFromTheShipResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFloorSectionTest::RunTest(const FString& Parameters)
{
	const FVoyageResult Ship = ThreeCaskShipResult();
	FFloorTestWorld S(RefineryLayouts::FloorSectionId(), &Ship);
	TestTrue(TEXT("Slot clock running"), S.State->bVoyageActive);
	TestEqual(TEXT("Ten minutes"), S.State->GetVoyageSecondsRemaining(), 600.0, 1.0);
	TestNull(TEXT("No incident director on the floor"), S.Mode->GetIncidentDirector());
	TestTrue(TEXT("The floor has air"), S.Mode->AirHasAtmosphere());
	TestFalse(TEXT("No fuel system"), S.Mode->FuelHasSystem());
	TestEqual(TEXT("Three casks on the pallets"), S.Count<ACargoCrate>(), 3);
	TestEqual(TEXT("Six fixtures"), S.Count<AFloorFixture>(), 6);
	TestEqual(TEXT("One module (the scrubber panel)"), S.Count<AShipModule>(), 1);
	TestEqual(TEXT("Three signs"), S.Count<AStationSign>(), 3);
	int32 Light = 0; for (const auto& Crate : S.State->Cargo) { Light += Crate.bVolatile; }
	TestEqual(TEXT("Two light casks"), Light, 2);
	TestTrue(TEXT("Cargo summary lists C4"), S.Mode->CargoSummary().Contains(TEXT("Cargo C4 Loose")));
	// The slot's end (the combined record and the card home) is timer-driven and proven in PIE by verify-floor.ps1.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorEmptyTest, "Airlock.Floor.EmptyFloorWithoutAShipResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFloorEmptyTest::RunTest(const FString& Parameters)
{
	FFloorTestWorld S(RefineryLayouts::FloorSectionId(), nullptr);
	TestTrue(TEXT("Slot clock running"), S.State->bVoyageActive);
	TestEqual(TEXT("No casks"), S.Count<ACargoCrate>(), 0);
	TestEqual(TEXT("Fixtures still spawn"), S.Count<AFloorFixture>(), 6);
	TestTrue(TEXT("Air present"), S.Mode->AirHasAtmosphere());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipUnchangedTest, "Airlock.Floor.ShipSectionUnchanged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipUnchangedTest::RunTest(const FString& Parameters)
{
	FFloorTestWorld S(TEXT("Hauler_Mk1"), nullptr);
	TestNotNull(TEXT("Director on the ship"), S.Mode->GetIncidentDirector());
	TestEqual(TEXT("Twenty minutes"), S.State->GetVoyageSecondsRemaining(), 1200.0, 1.0);
	TestEqual(TEXT("No floor fixtures on the ship"), S.Count<AFloorFixture>(), 0);
	TestEqual(TEXT("Four casks"), S.Count<ACargoCrate>(), 4);
	TestFalse(TEXT("No stills on the ship"), S.Mode->RefineryHasFloor());
	TestEqual(TEXT("Empty refinery mirror"), S.State->Refinery.Stills.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorStillsTest, "Airlock.Floor.StillsLoadKnobsLeversAndABatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFloorStillsTest::RunTest(const FString& Parameters)
{
	const FVoyageResult Ship = ThreeCaskShipResult();
	FFloorTestWorld S(RefineryLayouts::FloorSectionId(), &Ship);
	const FName Still1(TEXT("Still_1"));
	auto MirrorStill = [&S](FName Id) { return S.State->Refinery.Stills.FindByPredicate([Id](const FStillState& St) { return St.Id == Id; }); };
	auto MirrorCrate = [&S](FName Id) { return S.State->Cargo.FindByPredicate([Id](const FCargoCrateState& C) { return C.Id == Id; }); };
	// Built from the delivered casks (light = the ship's volatile) and mirrored on the game state; the fixtures read it.
	TestTrue(TEXT("The floor has stills"), S.Mode->RefineryHasFloor());
	TestEqual(TEXT("Three stills"), S.State->Refinery.Stills.Num(), 3);
	TestEqual(TEXT("Three casks waiting"), S.State->Refinery.Waiting.Num(), 3);
	TestEqual(TEXT("Delivered"), S.State->Refinery.CasksDelivered, 3);
	if (S.State->Refinery.Waiting.Num() == 3)
	{
		TestFalse(TEXT("C1 is heavy"), S.State->Refinery.Waiting[0].bLight);
		TestTrue(TEXT("C2 is light"), S.State->Refinery.Waiting[1].bLight);
	}
	if (!MirrorStill(Still1)) { AddError(TEXT("Still_1 missing from the mirror")); return false; }
	AFloorFixture* StillFixture = S.Find<AFloorFixture>(Still1); AFloorFixture* TankFixture = S.Find<AFloorFixture>(TEXT("Tank"));
	if (!StillFixture || !TankFixture) { AddError(TEXT("Fixtures missing")); return false; }
	// M5-5: the still's name stands alone on its base label; the state is on the gauge stack (StillTests reads it).
	TestEqual(TEXT("Still label is its name"), StillFixture->CurrentLabel(), FString(TEXT("STILL 1")));
	TestTrue(TEXT("Still state line still describes the mirror"), AFloorFixture::StateLine(StillFixture->GetSpec(), S.State->Refinery).Contains(TEXT("idle 92K")));
	TestTrue(TEXT("Tank label empty"), TankFixture->CurrentLabel().EndsWith(TEXT("0 L")));
	// Knobs: set, step, clamp, an unknown still; the state mirrors them.
	TestTrue(TEXT("Heat set"), S.Mode->RefinerySetKnob(Still1, ERefineryKnob::Heat, 8));
	TestTrue(TEXT("Feed stepped"), S.Mode->RefineryStepKnob(Still1, ERefineryKnob::Feed, 25));
	TestFalse(TEXT("Unknown still knob"), S.Mode->RefinerySetKnob(TEXT("Still_9"), ERefineryKnob::Heat, 8));
	TestEqual(TEXT("Heat mirrored"), MirrorStill(Still1)->HeatSteps, 8);
	TestEqual(TEXT("Feed clamped"), MirrorStill(Still1)->FeedSteps, 20);
	TestTrue(TEXT("Feed set"), S.Mode->RefinerySetKnob(Still1, ERefineryKnob::Feed, 10));
	// Levers on an idle still: reset, purge and repair refuse; calibrate and scrape go through.
	TestFalse(TEXT("Reset refused"), S.Mode->RefineryLever(Still1, ERefineryLever::Reset));
	TestFalse(TEXT("Purge refused"), S.Mode->RefineryLever(Still1, ERefineryLever::Purge));
	TestFalse(TEXT("Repair refused"), S.Mode->RefineryLever(Still1, ERefineryLever::Repair));
	TestTrue(TEXT("Calibrate"), S.Mode->RefineryLever(Still1, ERefineryLever::Calibrate));
	TestTrue(TEXT("Scrape"), S.Mode->RefineryLever(Still1, ERefineryLever::Scrape));
	// Loading refuses what is not on the floor; the batch follows M5-1's recipe: 240 s of preheat at heat 40 %, then C1 at feed 50 %.
	TestFalse(TEXT("Unknown cask"), S.Mode->RefineryLoadHopper(Still1, TEXT("C9")));
	TestFalse(TEXT("Unknown still load"), S.Mode->RefineryLoadHopper(TEXT("Still_9"), TEXT("C1")));
	S.Tick(240.f);
	TestEqual(TEXT("Preheated"), MirrorStill(Still1)->TempK, 195.f, 1.5f);
	TestTrue(TEXT("C1 loaded"), S.Mode->RefineryLoadHopper(Still1, TEXT("C1")));
	TestFalse(TEXT("C1 cannot load twice"), S.Mode->RefineryLoadHopper(TEXT("Still_2"), TEXT("C1")));
	TestTrue(TEXT("C1 is Loaded"), MirrorCrate(TEXT("C1")) && MirrorCrate(TEXT("C1"))->State == ECargoState::Loaded);
	ACargoCrate* Crate = S.Find<ACargoCrate>(TEXT("C1"));
	TestTrue(TEXT("C1's crate hides"), Crate && Crate->IsHidden());
	TestEqual(TEXT("Two waiting"), S.State->Refinery.Waiting.Num(), 2);
	TestEqual(TEXT("Hopper holds C1"), MirrorStill(Still1)->Hopper.Num(), 1);
	S.Tick(1.f);
	TestEqual(TEXT("Running"), MirrorStill(Still1)->Status, EStillStatus::Running);
	TestEqual(TEXT("Current cask"), MirrorStill(Still1)->CurrentCask, FName(TEXT("C1")));
	TestTrue(TEXT("Still state line runs"), AFloorFixture::StateLine(StillFixture->GetSpec(), S.State->Refinery).Contains(TEXT("C1 ")));
	S.Tick(101.f);
	TestEqual(TEXT("C1 refined"), S.State->Refinery.CasksRefined, 1);
	TestEqual(TEXT("Idle again"), MirrorStill(Still1)->Status, EStillStatus::Idle);
	TestEqual(TEXT("M5-1's measured yield"), S.State->Refinery.TankDieselL, 747.f, 2.f);
	TestTrue(TEXT("Tank label"), TankFixture->CurrentLabel().Contains(FString::Printf(TEXT("%d L"), FMath::FloorToInt(S.State->Refinery.TankDieselL))));
	// The result: the floor's fields and the base fuel come from the model's tally, added to the ship's pay.
	const FFloorResult Expected = S.Mode->RefineryTally();
	TestEqual(TEXT("Tally refined"), Expected.CasksRefined, 1);
	TestEqual(TEXT("Tally unrefined"), Expected.CasksUnrefined, 2);
	TestEqual(TEXT("Surplus tonnes"), Expected.BaseFuelTonnes, 7);
	const int32 BaseBefore = S.Voyages->GetBaseFuelTonnes();
	S.Mode->RecordVoyageResult();
	FVoyageResult Last;
	TestTrue(TEXT("Recorded"), S.Voyages->GetLastVoyageResult(Last));
	TestTrue(TEXT("Has floor"), Last.bHasFloor);
	TestEqual(TEXT("Floor total"), Last.FloorCasksTotal, 3);
	TestEqual(TEXT("Floor refined"), Last.FloorCasksRefined, 1);
	TestEqual(TEXT("Floor unrefined"), Last.FloorCasksUnrefined, 2);
	TestEqual(TEXT("Floor diesel"), Last.FloorDieselL, FMath::FloorToInt(Expected.DieselL));
	TestEqual(TEXT("Floor pay"), Last.FloorPay, Expected.Pay);
	TestEqual(TEXT("Combined pay"), Last.Pay, Ship.Pay + Expected.Pay);
	TestEqual(TEXT("Base fuel is the surplus"), Last.FuelSurplus, Expected.BaseFuelTonnes);
	TestEqual(TEXT("Banked at the Rig"), S.Voyages->GetBaseFuelTonnes(), BaseBefore + Expected.BaseFuelTonnes);
	TestTrue(TEXT("Summary"), Last.Summary().Contains(TEXT("| floor: 1/3 casks refined, ")));
	S.Voyages->SetBaseFuelTonnes(BaseBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorTaskRowsTest, "Airlock.Floor.TaskBoardStillRowsFixtureLinesAndTheHorn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFloorTaskRowsTest::RunTest(const FString& Parameters)
{
	auto Cask = [](const TCHAR* Id, bool bLight) { FRefineryCask C; C.Id = Id; C.bLight = bLight; return C; };
	auto Has = [](const TArray<FTaskBoardRow>& Rows, const TCHAR* Prefix)
	{ return Rows.ContainsByPredicate([Prefix](const FTaskBoardRow& Row) { return Row.Text.ToString().StartsWith(Prefix); }); };
	FRefineryModel M; M.Build(3, { Cask(TEXT("C1"), false), Cask(TEXT("C2"), true), Cask(TEXT("C3"), false) });
	FRefineryState R = M.State();
	R.Stills[0].Status = EStillStatus::Popped; R.Stills[0].PressureBar = 3.f;
	R.Stills[1].Status = EStillStatus::Running; R.Stills[1].CurrentCask = TEXT("C3"); R.Stills[1].PressureBar = 2.7f; R.Stills[1].bFouled = true;
	R.Stills[2].Calibration = 0.7f;
	R.Waiting = { Cask(TEXT("C1"), false), Cask(TEXT("C2"), true) };
	FCargoModel Cargo; Cargo.Build(CargoLayouts::BuildFloor({ TEXT("C1"), TEXT("C2") }, { TEXT("C2") }));
	Cargo.SetTemperature(TEXT("C2"), 60.f);
	FShipGraph Graph; Graph.Build(*ShipLayouts::Find(RefineryLayouts::FloorSectionId()));
	auto Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines(), Cargo.Crates(), nullptr, nullptr, true, &R);
	if (!TestEqual(TEXT("Six floor rows"), Rows.Num(), 6)) { for (const auto& Row : Rows) { AddInfo(Row.Text.ToString()); } return false; }
	TestEqual(TEXT("Popped first"), Rows[0].Text.ToString(), FString(TEXT("Still_1 popped - reset the trap"))); TestTrue(TEXT("Popped red"), Rows[0].bFailed);
	TestEqual(TEXT("Pressure second"), Rows[1].Text.ToString(), FString(TEXT("Still_2 2.7 bar - open the vent"))); TestTrue(TEXT("Pressure red"), Rows[1].bFailed);
	TestEqual(TEXT("Fouled"), Rows[2].Text.ToString(), FString(TEXT("Still_2 fouled - scrape when empty"))); TestFalse(TEXT("Fouled white"), Rows[2].bFailed);
	TestEqual(TEXT("Drifted"), Rows[3].Text.ToString(), FString(TEXT("Still_3 drifted 70% - calibrate")));
	TestEqual(TEXT("Load a still"), Rows[4].Text.ToString(), FString(TEXT("Load a still - 2 casks waiting")));
	TestEqual(TEXT("A warm cask goes into a still"), Rows[5].Text.ToString(), FString(TEXT("Load crate C2 - 60C")));
	TestFalse(TEXT("No secure rows on the floor"), Has(Rows, TEXT("Secure")));
	R.Stills[0].Status = EStillStatus::Failed; R.Stills[1].Status = EStillStatus::Flash; R.Stills[1].PressureBar = 1.f;
	R.Waiting.SetNum(1); R.Stills[2].Hopper.Add(TEXT("C3"));
	Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines(), {}, nullptr, nullptr, true, &R);
	if (!TestTrue(TEXT("Rows"), Rows.Num() >= 2)) { return false; }
	TestEqual(TEXT("Failed row"), Rows[0].Text.ToString(), FString(TEXT("Still_1 failed - repair"))); TestTrue(TEXT("Failed red"), Rows[0].bFailed);
	TestEqual(TEXT("Flash row"), Rows[1].Text.ToString(), FString(TEXT("Still_2 flash - purge"))); TestTrue(TEXT("Flash red"), Rows[1].bFailed);
	TestFalse(TEXT("No idle empty still, no load row"), Has(Rows, TEXT("Load a still")));
	R.Stills[2].Hopper.Empty();
	Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines(), {}, nullptr, nullptr, true, &R);
	TestTrue(TEXT("One cask waiting"), Has(Rows, TEXT("Load a still - 1 cask waiting")));
	// Off the floor nothing changes: the ship's board keeps its secure rows and counts to the dock.
	Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines(), Cargo.Crates(), nullptr, nullptr, true);
	TestTrue(TEXT("Secure rows without a refinery"), Has(Rows, TEXT("Secure crate C1")));
	TestTrue(TEXT("Cool rows without a refinery"), Has(Rows, TEXT("Cool crate C2")));
	TestEqual(TEXT("To the horn"), UTaskBoardWidget::BuildReadout(nullptr, nullptr, 65, true).ToString(), FString(TEXT("01:05 to horn")));
	TestEqual(TEXT("To the dock"), UTaskBoardWidget::BuildReadout(nullptr, nullptr, 65).ToString(), FString(TEXT("01:05 to dock")));
	// The fixture's second line from the same state.
	const FRefineryLayout* Layout = RefineryLayouts::Find(RefineryLayouts::FloorSectionId());
	const FFloorFixtureSpec* StillSpec = Layout->Fixtures.FindByPredicate([](const FFloorFixtureSpec& F) { return F.Id == TEXT("Still_2"); });
	const FFloorFixtureSpec* TankSpec = Layout->Fixtures.FindByPredicate([](const FFloorFixtureSpec& F) { return F.Kind == EFloorFixtureKind::Tank; });
	const FFloorFixtureSpec* DrainSpec = Layout->Fixtures.FindByPredicate([](const FFloorFixtureSpec& F) { return F.Kind == EFloorFixtureKind::Drain; });
	if (!StillSpec || !TankSpec || !DrainSpec) { AddError(TEXT("Fixture specs missing")); return false; }
	R.Stills[1].Status = EStillStatus::Running; R.Stills[1].ChargeL = 580.f; R.Stills[1].TempK = 184.3f; R.Stills[1].PressureBar = 1.4f;
	R.TankDieselL = 747.4f;
	TestEqual(TEXT("Still line"), AFloorFixture::StateLine(*StillSpec, R), FString(TEXT("C3 42% 184K 1.4bar fouled")));
	TestEqual(TEXT("Tank line"), AFloorFixture::StateLine(*TankSpec, R), FString(TEXT("747 L")));
	TestEqual(TEXT("Drain has no line"), AFloorFixture::StateLine(*DrainSpec, R), FString());
	R.Stills[1].Status = EStillStatus::Popped;
	TestEqual(TEXT("Popped line"), AFloorFixture::StateLine(*StillSpec, R), FString(TEXT("POPPED fouled")));
	return true;
}
#endif
