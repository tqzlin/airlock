// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Ship/CargoModel.h"
#include "Ship/CargoLayouts.h"
#include "Ship/ShipGraph.h"
#include "Ship/ShipLayouts.h"
#include "Voyage/IncidentSchedule.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FCargoModel CargoTestManifest()
{
	FCargoModel Model;
	Model.Build(*CargoLayouts::Find(TEXT("Hauler_Mk1")));
	return Model;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoBuildTest, "Airlock.Cargo.BuildManifestAndSlotOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoBuildTest::RunTest(const FString& Parameters)
{
	FCargoModel M = CargoTestManifest();
	TestNull(TEXT("No hub layout"), CargoLayouts::Find(TEXT("StationHub")));
	TestEqual(TEXT("Four crates"), M.Crates().Num(), 4);
	TestEqual(TEXT("Four slots"), M.Slots().Num(), 4);
	int32 Volatile = 0, Racks = 0;
	for (const auto& C : M.Crates()) { TestTrue(TEXT("Loose at zero without a slot"), C.State == ECargoState::Loose && C.Temperature == 0 && C.SlotId.IsNone()); Volatile += C.bVolatile; }
	for (const auto& S : M.Slots()) { TestTrue(TEXT("Empty"), S.CrateId.IsNone()); Racks += S.bRack; }
	TestEqual(TEXT("Two volatile"), Volatile, 2);
	TestEqual(TEXT("One rack"), Racks, 1);
	TestFalse(TEXT("Empty clamp release"), M.Release(TEXT("Clamp_1")));
	TestFalse(TEXT("Unknown crate"), M.PickUp(TEXT("Missing")));
	TestFalse(TEXT("Loose cannot be placed"), M.Place(TEXT("C1"), TEXT("Clamp_1")));
	TestTrue(TEXT("Pick loose"), M.PickUp(TEXT("C1")));
	TestFalse(TEXT("Already carried"), M.PickUp(TEXT("C1")));
	TestFalse(TEXT("Unknown slot"), M.Place(TEXT("C1"), TEXT("Missing")));
	TestTrue(TEXT("Place clamp"), M.Place(TEXT("C1"), TEXT("Clamp_1")));
	TestTrue(TEXT("Pick second"), M.PickUp(TEXT("C2")));
	TestFalse(TEXT("Occupied"), M.Place(TEXT("C2"), TEXT("Clamp_1")));
	TestTrue(TEXT("Release filled clamp"), M.Release(TEXT("Clamp_1")));
	TestTrue(TEXT("Release clears both sides"), M.Slots()[0].CrateId.IsNone() && M.FindCrate(TEXT("C1"))->SlotId.IsNone() && M.FindCrate(TEXT("C1"))->State == ECargoState::Loose);
	TestTrue(TEXT("Place rack"), M.Place(TEXT("C2"), TEXT("ColdRack")));
	TestFalse(TEXT("Rack never shifts"), M.Release(TEXT("ColdRack")));
	TestTrue(TEXT("Pick racked"), M.PickUp(TEXT("C2")));
	TestTrue(TEXT("Rack now empty"), M.Slots()[3].CrateId.IsNone());
	TestTrue(TEXT("Put down"), M.PutDown(TEXT("C2")));
	TestFalse(TEXT("Already loose"), M.PutDown(TEXT("C2")));
	M.PickUp(TEXT("C1")); M.Place(TEXT("C1"), TEXT("Clamp_1"));
	TestTrue(TEXT("Pick secured"), M.PickUp(TEXT("C1")));
	TestTrue(TEXT("Clamp freed"), M.Slots()[0].CrateId.IsNone());
	M.Build(*CargoLayouts::Find(TEXT("Hauler_Mk1")));
	TestTrue(TEXT("Rebuild resets"), M.Crates() == CargoTestManifest().Crates() && M.Slots() == CargoTestManifest().Slots());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoHeatTest, "Airlock.Cargo.CalmHeatingCoolingAndWholeDegrees",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoHeatTest::RunTest(const FString& Parameters)
{
	FCargoModel M = CargoTestManifest();
	const FCargoRates R;
	TestFalse(TEXT("Calm no heat"), M.Tick(10, FIncidentSchedule::CalmSeconds - 1));
	TestFalse(TEXT("Fraction accumulates without publish"), M.Tick(0.25f, FIncidentSchedule::CalmSeconds));
	TestTrue(TEXT("Whole degree publish"), M.Tick(0.75f, FIncidentSchedule::CalmSeconds));
	TestEqual(TEXT("Loose rate"), M.FindCrate(TEXT("C2"))->Temperature, R.LooseHeat);
	M.PickUp(TEXT("C2")); M.Tick(10, 160);
	TestEqual(TEXT("Carried rate"), M.FindCrate(TEXT("C2"))->Temperature, R.LooseHeat + 10 * R.CarriedHeat);
	M.Place(TEXT("C2"), TEXT("Clamp_1")); M.Tick(10, 170);
	TestEqual(TEXT("Secured unchanged"), M.FindCrate(TEXT("C2"))->Temperature, 6.f);
	M.PickUp(TEXT("C2")); M.Place(TEXT("C2"), TEXT("ColdRack")); M.Tick(1, 0);
	TestEqual(TEXT("Rack cools during calm"), M.FindCrate(TEXT("C2"))->Temperature, 6.f - R.RackCool);
	M.Tick(10, 0);
	TestEqual(TEXT("Cooling clamps zero"), M.FindCrate(TEXT("C2"))->Temperature, 0.f);
	M.Tick(1000, 150);
	TestEqual(TEXT("Nonvolatile loose never heats"), M.FindCrate(TEXT("C1"))->Temperature, 0.f);
	M.PickUp(TEXT("C3")); M.Tick(1000, 150);
	TestEqual(TEXT("Nonvolatile carried never heats"), M.FindCrate(TEXT("C3"))->Temperature, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoVentTest, "Airlock.Cargo.VentingIsInertAndRetainsSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoVentTest::RunTest(const FString& Parameters)
{
	for (const FName Slot : { FName(NAME_None), FName(TEXT("Clamp_1")), FName(TEXT("ColdRack")) })
	{
		FCargoModel M = CargoTestManifest();
		if (!Slot.IsNone()) { M.PickUp(TEXT("C2")); M.Place(TEXT("C2"), Slot); }
		TestTrue(TEXT("100 accepted"), M.SetTemperature(TEXT("C2"), 100));
		TestTrue(TEXT("Not vented until tick"), M.FindCrate(TEXT("C2"))->State != ECargoState::Vented);
		TestTrue(TEXT("Tick vents even during calm or on rack"), M.Tick(0.25f, 0));
		const auto Before = M.Crates();
		TestTrue(TEXT("Vented"), M.FindCrate(TEXT("C2"))->State == ECargoState::Vented);
		TestEqual(TEXT("Slot retained"), M.FindCrate(TEXT("C2"))->SlotId, Slot);
		TestFalse(TEXT("No pickup"), M.PickUp(TEXT("C2")));
		TestFalse(TEXT("No putdown"), M.PutDown(TEXT("C2")));
		TestFalse(TEXT("No place"), M.Place(TEXT("C2"), TEXT("Clamp_2")));
		TestFalse(TEXT("No temperature edit"), M.SetTemperature(TEXT("C2"), 0));
		TestFalse(TEXT("No release"), M.Release(Slot));
		M.Tick(10, 150);
		TestTrue(TEXT("Vented stays inert"), *M.FindCrate(TEXT("C2")) == Before[1]);
		if (!Slot.IsNone())
		{
			M.PickUp(TEXT("C1"));
			TestFalse(TEXT("Vented slot still occupied"), M.Place(TEXT("C1"), Slot));
		}
	}
	FCargoModel M = CargoTestManifest();
	M.SetTemperature(TEXT("C4"), 99); M.Tick(1, 150);
	TestTrue(TEXT("Heating reaches vent threshold"), M.FindCrate(TEXT("C4"))->State == ECargoState::Vented);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoInvalidTest, "Airlock.Cargo.InvalidInputsAreNoOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoInvalidTest::RunTest(const FString& Parameters)
{
	FCargoModel M = CargoTestManifest();
	TestFalse(TEXT("Nonvolatile temperature rejected"), M.SetTemperature(TEXT("C1"), 50));
	TestFalse(TEXT("Missing temperature rejected"), M.SetTemperature(TEXT("Missing"), 50));
	for (const float Bad : { -1.f, 101.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
	{
		TestFalse(TEXT("Bad heat rejected"), M.SetTemperature(TEXT("C2"), Bad));
	}
	M.SetTemperature(TEXT("C2"), 100);
	const auto Before = M.Crates();
	for (const float Bad : { 0.f, -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() })
	{
		TestFalse(TEXT("Invalid tick"), M.Tick(Bad, 150));
		TestTrue(TEXT("No changes, even pending vent"), M.Crates() == Before);
	}
	TestTrue(TEXT("Zero heat accepted"), M.SetTemperature(TEXT("C2"), 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoDeterminismTest, "Airlock.Cargo.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoDeterminismTest::RunTest(const FString& Parameters)
{
	FCargoModel A = CargoTestManifest(), B = CargoTestManifest();
	for (int32 I = 0; I < 1600; ++I)
	{
		if (I == 800) { A.PickUp(TEXT("C2")); B.PickUp(TEXT("C2")); }
		if (I == 820) { A.Place(TEXT("C2"), TEXT("ColdRack")); B.Place(TEXT("C2"), TEXT("ColdRack")); }
		TestEqual(TEXT("Same tick change"), A.Tick(0.25f, I * 0.25), B.Tick(0.25f, I * 0.25));
		TestTrue(TEXT("Same state"), A.Crates() == B.Crates() && A.Slots() == B.Slots());
	}
	TestEqual(TEXT("Same summary"), A.Summary(), B.Summary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoTallyTest, "Airlock.Cargo.TallyPayExamplesAndCarriedLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoTallyTest::RunTest(const FString& Parameters)
{
	FShipGraph Ship;
	Ship.Build(*ShipLayouts::Find(TEXT("Hauler_Mk1")));
	FVoyageSpec Spec; Spec.Seed = 123;
	FCargoModel M = CargoTestManifest();
	for (int32 I = 0; I < 4; ++I) { const FName Id = M.Crates()[I].Id; M.PickUp(Id); M.Place(Id, M.Slots()[I].Id); }
	auto Result = FCargoModel::Tally(M, Ship, Spec);
	TestEqual(TEXT("Clean manifest pay"), Result.Pay, 750);
	TestEqual(TEXT("All intact"), Result.CratesIntact, 4);
	TestEqual(TEXT("Seed recorded in value"), Result.Seed, Spec.Seed);
	M.SetTemperature(TEXT("C4"), 100); M.Tick(0.25f, 0);
	for (const auto& Module : Ship.Modules()) { Ship.DamageModule(Module.Id, 50); }
	Result = FCargoModel::Tally(M, Ship, Spec);
	TestEqual(TEXT("Three intact, one vented, half hull pay"), Result.Pay, 400);
	TestEqual(TEXT("Vented count"), Result.CratesVented, 1);
	TestEqual(TEXT("Hull mean"), Result.ShipCondition, 50.f);
	M = CargoTestManifest(); Ship.Build(*ShipLayouts::Find(TEXT("Hauler_Mk1")));
	M.PickUp(TEXT("C1")); M.Place(TEXT("C1"), TEXT("Clamp_1"));
	M.PickUp(TEXT("C2")); M.Place(TEXT("C2"), TEXT("Clamp_2"));
	TestEqual(TEXT("Two intact two loose pay"), FCargoModel::Tally(M, Ship, Spec).Pay, 400);
	M.PickUp(TEXT("C3"));
	Result = FCargoModel::Tally(M, Ship, Spec);
	TestEqual(TEXT("Carried is lost"), Result.CratesLoose, 2);
	TestEqual(TEXT("Carried does not change pay"), Result.Pay, 400);
	Ship.DamageModule(TEXT("Nav"), 1);
	TestTrue(TEXT("Hull is unrounded mean"), FMath::IsNearlyEqual(FCargoModel::Tally(M, Ship, Spec).ShipCondition, 699.f / 7.f));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoRackFactorTest, "Airlock.Cargo.RackCoolFactorClampsAndResets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoRackFactorTest::RunTest(const FString& Parameters)
{
	FCargoModel M = CargoTestManifest();
	M.PickUp(TEXT("C2")); M.Place(TEXT("C2"), TEXT("ColdRack")); M.SetTemperature(TEXT("C2"), 80);
	M.SetRackCoolFactor(.5f); M.Tick(1, 0);
	TestEqual(TEXT("Half power cools 2 degrees per second"), M.FindCrate(TEXT("C2"))->Temperature, 78.f);
	M.SetRackCoolFactor(-1); M.Tick(1, 0);
	TestEqual(TEXT("Lower clamp stops cooling"), M.FindCrate(TEXT("C2"))->Temperature, 78.f);
	M.SetRackCoolFactor(2); M.Tick(1, 0);
	TestEqual(TEXT("Upper clamp full cooling"), M.FindCrate(TEXT("C2"))->Temperature, 74.f);
	M.SetRackCoolFactor(std::numeric_limits<float>::quiet_NaN()); M.Tick(1, 0);
	TestEqual(TEXT("Invalid factor ignored"), M.FindCrate(TEXT("C2"))->Temperature, 70.f);
	M.SetRackCoolFactor(0); M.Build(*CargoLayouts::Find(TEXT("Hauler_Mk1")));
	M.PickUp(TEXT("C2")); M.Place(TEXT("C2"), TEXT("ColdRack")); M.SetTemperature(TEXT("C2"), 80); M.Tick(1, 0);
	TestEqual(TEXT("Build restores full cooling"), M.FindCrate(TEXT("C2"))->Temperature, 76.f);
	return true;
}
#endif
