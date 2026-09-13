// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Core/AirlockSaveGame.h"
#include "Ship/CargoLayouts.h"
#include "Ship/CargoModel.h"
#include "Ship/ShipGraph.h"
#include "Ship/ShipLayouts.h"
#include "Station/StationSign.h"
#include "UI/VoyageBoardWidget.h"
#include "Voyage/VoyageSchedule.h"
#include "Voyage/VoyageSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryTiersTest, "Airlock.Refinery.TiersAndManifests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryTiersTest::RunTest(const FString& Parameters)
{
	const int32 Casks[] = { 4, 8, 12 }, Volatile[] = { 2, 4, 6 }, Crew[] = { 2, 4, 6 };
	const EVoyageType Tiers[] = { EVoyageType::MilkRun, EVoyageType::Standard, EVoyageType::Priority };
	for (int32 I = 0; I < 3; ++I)
	{
		const FVoyageSpec Spec = UVoyageSubsystem::MakeVoyageSpec(Tiers[I], 7);
		TestEqual(TEXT("Casks per tier"), Spec.CaskCount, Casks[I]);
		TestEqual(TEXT("Volatile per tier"), Spec.VolatileCount, Volatile[I]);
		TestEqual(TEXT("Crew per tier"), Spec.CrewCapacity, Crew[I]);
		TestEqual(TEXT("One ship class"), Spec.ShipClassId, FName(TEXT("Hauler_Mk1")));
		TestTrue(TEXT("Deterministic captain"), Spec.Captain == UVoyageSubsystem::MakeVoyageSpec(Tiers[I], 7).Captain && !Spec.Captain.IsNone());
		const FCargoLayout Layout = CargoLayouts::Build(TEXT("Hauler_Mk1"), Spec.CaskCount, Spec.VolatileCount);
		TestEqual(TEXT("Crate count"), Layout.Crates.Num(), Casks[I]);
		TestEqual(TEXT("Slot count = clamps + rack"), Layout.Slots.Num(), Casks[I] * 3 / 4 + 1);
		int32 Vol = 0, Racks = 0;
		TSet<FName> Ids; TSet<FVector> Spots;
		for (const auto& C : Layout.Crates) { Vol += C.bVolatile; Ids.Add(C.Id); Spots.Add(C.Transform.GetLocation()); }
		for (const auto& S : Layout.Slots) { Racks += S.bRack; Ids.Add(S.Id); Spots.Add(S.Transform.GetLocation()); }
		TestEqual(TEXT("Volatile count"), Vol, Volatile[I]);
		TestEqual(TEXT("One rack"), Racks, 1);
		TestEqual(TEXT("Unique ids"), Ids.Num(), Layout.Crates.Num() + Layout.Slots.Num());
		TestEqual(TEXT("Distinct spots"), Spots.Num(), Layout.Crates.Num() + Layout.Slots.Num());
		TestTrue(TEXT("Second cask volatile"), Layout.Crates[1].bVolatile && !Layout.Crates[0].bVolatile);
	}
	// The small tier is exactly today's layout, so every existing proof that books tier 0 sees what it saw.
	const FCargoLayout Small = CargoLayouts::Build(TEXT("Hauler_Mk1"), 4, 2);
	const FCargoLayout* Today = CargoLayouts::Find(TEXT("Hauler_Mk1"));
	if (TestNotNull(TEXT("Find"), Today))
	{
		TestEqual(TEXT("Find crate count"), Today->Crates.Num(), 4);
		for (int32 I = 0; I < 4; ++I)
		{
			TestEqual(TEXT("Same crate id"), Small.Crates[I].Id, Today->Crates[I].Id);
			TestTrue(TEXT("Same crate spot"), Small.Crates[I].Transform.GetLocation().Equals(Today->Crates[I].Transform.GetLocation()));
			TestEqual(TEXT("Same slot id"), Small.Slots[I].Id, Today->Slots[I].Id);
		}
	}
	TestTrue(TEXT("Unknown section builds nothing"), CargoLayouts::Build(TEXT("StationHub"), 4, 2).Crates.IsEmpty());
	const auto Lines = FVoyageSchedule::Build(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Priority, 3));
	TestTrue(TEXT("Priority mid-voyage line"), Lines[1].Line.ToString().Contains(TEXT("Priority manifest")));
	TestTrue(TEXT("Departure names the casks"), Lines[0].Line.ToString().Contains(TEXT("Sludge casks")));
	TestTrue(TEXT("Docking names the refinery"), Lines[3].Line.ToString().Contains(TEXT("refinery")));
	TestEqual(TEXT("Hub has two signs"), StationSigns::Find(TEXT("StationHub")).Num(), 2);
	TestTrue(TEXT("Ship has no signs"), StationSigns::Find(TEXT("Hauler_Mk1")).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryTallyTest, "Airlock.Refinery.TallyFuelAndBoardText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryTallyTest::RunTest(const FString& Parameters)
{
	FShipGraph Ship; Ship.Build(*ShipLayouts::Find(TEXT("Hauler_Mk1")));
	FCargoModel M; M.Build(*CargoLayouts::Find(TEXT("Hauler_Mk1")));
	for (const TCHAR* Id : { TEXT("C1"), TEXT("C2"), TEXT("C3") }) { M.PickUp(Id); }
	M.Place(TEXT("C1"), TEXT("Clamp_1")); M.Place(TEXT("C2"), TEXT("Clamp_2")); M.Place(TEXT("C3"), TEXT("ColdRack"));
	FVoyageSpec Spec;
	const FVoyageResult R = FCargoModel::Tally(M, Ship, Spec);
	TestEqual(TEXT("Fuel credit"), R.FuelCredit, 3 * FCargoModel::FuelCreditPerIntactCask);
	TestEqual(TEXT("Fuel surplus"), R.FuelSurplus, 3 * FCargoModel::SurplusPerIntactCask);
	TestTrue(TEXT("Summary is the M3-5 text (the fuel rides on the result log line)"), R.Summary().EndsWith(TEXT("pay 500")));
	TestTrue(TEXT("M3-4 fields unchanged"), R.Summary().StartsWith(TEXT("3/4 crates intact, 0 vented, 1 loose, hull 100.00%, pay 500")));
	TestEqual(TEXT("Base line"), UVoyageBoardWidget::DescribeBase(220, 1, 20, true).ToString(), FString(TEXT("Base fuel 220 t  -  runs 1  (+20 this run)")));
	TestEqual(TEXT("Base line, no result"), UVoyageBoardWidget::DescribeBase(200, 0, 0, false).ToString(), FString(TEXT("Base fuel 200 t  -  runs 0")));
	FVoyageSpec Offer = UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Priority, 7);
	TestTrue(TEXT("Offer names the tier and casks"), UVoyageBoardWidget::DescribeOffer(Offer).ToString().StartsWith(TEXT("Priority run  -  12 casks  -  Capt. ")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefinerySaveTest, "Airlock.Refinery.SaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefinerySaveTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("AirlockTest");
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	UGameInstance* Outer = NewObject<UGameInstance>(GEngine);
	UVoyageSubsystem* A = NewObject<UVoyageSubsystem>(Outer);
	A->SaveSlotName = Slot;
	A->ReloadSave();
	TestEqual(TEXT("Fresh slot defaults"), A->GetBaseFuelTonnes(), UAirlockSaveGame::DefaultBaseFuelTonnes);
	TestEqual(TEXT("Fresh slot runs"), A->GetRunsCompleted(), 0);
	FVoyageResult Result; Result.CratesTotal = 4; Result.CratesIntact = 3; Result.FuelSurplus = 30;
	A->SetLastVoyageResult(Result);
	TestEqual(TEXT("Surplus added"), A->GetBaseFuelTonnes(), UAirlockSaveGame::DefaultBaseFuelTonnes + 30);
	TestEqual(TEXT("Run counted"), A->GetRunsCompleted(), 1);
	UVoyageSubsystem* B = NewObject<UVoyageSubsystem>(Outer);
	B->SaveSlotName = Slot;
	B->ReloadSave();
	TestEqual(TEXT("Persisted fuel"), B->GetBaseFuelTonnes(), UAirlockSaveGame::DefaultBaseFuelTonnes + 30);
	TestEqual(TEXT("Persisted runs"), B->GetRunsCompleted(), 1);
	FVoyageResult Nothing; Nothing.CratesTotal = 4;
	B->SetLastVoyageResult(Nothing);
	TestEqual(TEXT("Stock never falls"), B->GetBaseFuelTonnes(), UAirlockSaveGame::DefaultBaseFuelTonnes + 30);
	TestEqual(TEXT("Second run counted"), B->GetRunsCompleted(), 2);
	B->SetBaseFuelTonnes(999);
	TestEqual(TEXT("Console set"), B->GetBaseFuelTonnes(), 999);
	TestTrue(TEXT("Reset"), B->ResetSave());
	TestEqual(TEXT("Reset defaults"), B->GetBaseFuelTonnes(), UAirlockSaveGame::DefaultBaseFuelTonnes);
	TestFalse(TEXT("Slot deleted"), UGameplayStatics::DoesSaveGameExist(Slot, 0));
	return true;
}
#endif
