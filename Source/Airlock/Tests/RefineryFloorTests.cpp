// Copyright Airlock project. All Rights Reserved.
// M5-2: the refinery floor section - layouts, Okafor's schedule, the ship result hand-over, the combined tally.
#include "Misc/AutomationTest.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Ship/CargoLayouts.h"
#include "Ship/CargoModel.h"
#include "Ship/CargoCrate.h"
#include "Ship/FloorFixture.h"
#include "Ship/RefineryLayouts.h"
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
	return true;
}
#endif
