// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Ship/ShipAtmosphere.h"
#include "UI/TaskBoardWidget.h"
#include "Ship/ShipLayouts.h"
#include "Ship/CargoLayouts.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTaskBoardRowsTest, "Airlock.Incidents.TaskBoardPriorityRepairAndOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTaskBoardRowsTest::RunTest(const FString& Parameters)
{
	FShipGraph Graph;
	Graph.Build(*ShipLayouts::Find(TEXT("Hauler_Mk1")));
	auto Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines());
	TestEqual(TEXT("Empty graph tasks has one nominal row"), Rows.Num(), 1);
	TestEqual(TEXT("Nominal text"), Rows[0].Text.ToString(), FString(TEXT("All systems nominal")));
	Graph.DamageModule(TEXT("PowerCore"), 58.f);
	Graph.DamageModule(TEXT("Engines"), 90.f);
	Graph.SetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
	Graph.SetLineFault(TEXT("Power_2"), EShipLineFault::Short);
	Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines());
	if (!TestEqual(TEXT("One row per open task"), Rows.Num(), 4)) { return false; }
	TestEqual(TEXT("Failed before faults"), Rows[0].Text.ToString(), FString(TEXT("Engines - Failed 10%")));
	TestTrue(TEXT("Failed row marked red"), Rows[0].bFailed);
	TestEqual(TEXT("Leak row"), Rows[1].Text.ToString(), FString(TEXT("Tape leak - Coolant_1")));
	TestEqual(TEXT("Short row"), Rows[2].Text.ToString(), FString(TEXT("Patch short - Power_2")));
	TestEqual(TEXT("Degraded last"), Rows[3].Text.ToString(), FString(TEXT("PowerCore - Degraded 42%")));
	Graph.SetLineFault(TEXT("Coolant_1"), EShipLineFault::None);
	Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines());
	TestEqual(TEXT("Repair removes one row"), Rows.Num(), 3);
	TestFalse(TEXT("Repaired leak absent"), Rows.ContainsByPredicate([](const FTaskBoardRow& R) { return R.Text.ToString().Contains(TEXT("Coolant_1")); }));
	for (const FShipModuleSpec& Module : ShipLayouts::Find(TEXT("Hauler_Mk1"))->Modules) { Graph.DamageModule(Module.Id, 100.f); }
	for (const FShipLineSpec& Line : ShipLayouts::Find(TEXT("Hauler_Mk1"))->Lines)
	{
		Graph.SetLineFault(Line.Id, Line.Kind == EShipLineKind::Coolant ? EShipLineFault::Leak : EShipLineFault::Short);
	}
	Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines());
	TestEqual(TEXT("Eight tasks plus overflow row"), Rows.Num(), UTaskBoardWidget::MaxTaskRows + 1);
	TestEqual(TEXT("Overflow counts hidden tasks"), Rows.Last().Text.ToString(), FString(TEXT("+3 more")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoTaskRowsTest, "Airlock.Cargo.TaskBoardHeatingPriorityAndLooseRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoTaskRowsTest::RunTest(const FString& Parameters)
{
	FCargoModel Cargo;
	Cargo.Build(*CargoLayouts::Find(TEXT("Hauler_Mk1")));
	FShipGraph Graph;
	Graph.Build(*ShipLayouts::Find(TEXT("Hauler_Mk1")));
	Graph.DamageModule(TEXT("PowerCore"), 58);
	Graph.DamageModule(TEXT("Engines"), 90);
	Graph.SetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
	Cargo.SetTemperature(TEXT("C2"), 71.9f);
	Cargo.SetTemperature(TEXT("C4"), 80);
	auto Rows = UTaskBoardWidget::BuildRows(Graph.Modules(), Graph.Lines(), Cargo.Crates());
	TestEqual(TEXT("One row per cargo or ship task"), Rows.Num(), 7);
	TestEqual(TEXT("Hottest first"), Rows[0].Text.ToString(), FString(TEXT("Cool crate C4 - 80C")));
	TestTrue(TEXT("80 marked red"), Rows[0].bFailed);
	TestEqual(TEXT("Next hottest floored"), Rows[1].Text.ToString(), FString(TEXT("Cool crate C2 - 71C")));
	TestFalse(TEXT("Below 80 not red"), Rows[1].bFailed);
	TestEqual(TEXT("Heating before Failed"), Rows[2].Text.ToString(), FString(TEXT("Engines - Failed 10%")));
	TestEqual(TEXT("Secure tasks last"), Rows.Last().Text.ToString(), FString(TEXT("Secure crate C3")));
	TestFalse(TEXT("Hot loose crate has no duplicate secure row"), Rows.ContainsByPredicate([](const auto& R) { return R.Text.ToString() == TEXT("Secure crate C2"); }));
	Cargo.SetTemperature(TEXT("C4"), 100); Cargo.Tick(.25f, 0);
	Cargo.PickUp(TEXT("C2")); Cargo.Place(TEXT("C2"), TEXT("ColdRack"));
	Cargo.PickUp(TEXT("C1")); Cargo.Place(TEXT("C1"), TEXT("Clamp_1"));
	Cargo.PickUp(TEXT("C3"));
	Rows = UTaskBoardWidget::BuildRows({}, {}, Cargo.Crates());
	TestEqual(TEXT("Vented, racked, secured and cool carried crates have no rows"), Rows.Num(), 1);
	TestEqual(TEXT("Nominal when no tasks"), Rows[0].Text.ToString(), FString(TEXT("All systems nominal")));
	Cargo.PickUp(TEXT("C2"));
	Rows = UTaskBoardWidget::BuildRows({}, {}, Cargo.Crates());
	TestTrue(TEXT("Hot carried remains a cooling task"), Rows[0].Text.ToString().StartsWith(TEXT("Cool crate C2")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirTaskRowsTest, "Airlock.Atmosphere.TaskBoardAirPriorityWordsAndCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirTaskRowsTest::RunTest(const FString& Parameters)
{
	FAtmosphereState Air;
	Air.Ppm = 19;
	TestEqual(TEXT("Below warning no row"), UTaskBoardWidget::BuildRows({}, {}, {}, &Air)[0].Text.ToString(), FString(TEXT("All systems nominal")));
	Air.Ppm = 20;
	TestEqual(TEXT("Warning starts exactly at 20"), UTaskBoardWidget::BuildRows({}, {}, {}, &Air)[0].Text.ToString(), FString(TEXT("Hold air 20 ppm - scrub")));
	Air.Ppm = 32.9f;
	for (const EScrubMode Mode : { EScrubMode::Off, EScrubMode::Normal, EScrubMode::Hazard })
	{
		Air.ScrubMode = Mode;
		const auto Row = UTaskBoardWidget::BuildRows({}, {}, {}, &Air)[0];
		TestEqual(TEXT("Scrub wording and whole ppm"), Row.Text.ToString(), FString(Mode == EScrubMode::Hazard ? TEXT("Hold air 32 ppm - scrubbing") : TEXT("Hold air 32 ppm - scrub")));
		TestFalse(TEXT("Below danger not red"), Row.bFailed);
	}
	Air.bScrubDisabled = true;
	TestEqual(TEXT("Disabled wording wins over mode"), UTaskBoardWidget::BuildRows({}, {}, {}, &Air)[0].Text.ToString(), FString(TEXT("Hold air 32 ppm - repair life support")));
	Air.Ppm = 50;
	TestTrue(TEXT("Red exactly at danger"), UTaskBoardWidget::BuildRows({}, {}, {}, &Air)[0].bFailed);
	TArray<FCargoCrateState> Cargo;
	for (int32 I = 0; I < 10; ++I) { FCargoCrateState C; C.Id = *FString::Printf(TEXT("C%d"), I); C.bVolatile = true; C.Temperature = 80; Cargo.Add(C); }
	const auto Rows = UTaskBoardWidget::BuildRows({}, {}, Cargo, &Air);
	TestTrue(TEXT("Air precedes hottest cargo"), Rows[0].Text.ToString().StartsWith(TEXT("Hold air")));
	TestTrue(TEXT("Heating order still follows"), Rows[1].Text.ToString().StartsWith(TEXT("Cool crate C0")));
	TestEqual(TEXT("Eight plus overflow"), Rows.Num(), UTaskBoardWidget::MaxTaskRows + 1);
	TestEqual(TEXT("Extra air row counted in overflow"), Rows.Last().Text.ToString(), FString(TEXT("+3 more")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelTaskRowsTest, "Airlock.Fuel.TaskRowsPriorityWordsThresholdsAndCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFuelTaskRowsTest::RunTest(const FString& Parameters)
{
	FFuelState Fuel; Fuel.bFeedLeak = true; Fuel.bInjectorsFouled = true;
	FAtmosphereState Air; Air.Ppm = 30;
	FCargoCrateState C; C.Id = TEXT("C2"); C.bVolatile = true; C.Temperature = 45;
	auto Rows = UTaskBoardWidget::BuildRows({}, {}, {C}, &Air, &Fuel);
	TestEqual(TEXT("Leak first"), Rows[0].Text.ToString(), FString(TEXT("Fuel leak - shut the feed valve")));
	TestTrue(TEXT("Leak red"), Rows[0].bFailed); TestTrue(TEXT("Air second"), Rows[1].Text.ToString().StartsWith(TEXT("Hold air")));
	TestEqual(TEXT("Injectors third"), Rows[2].Text.ToString(), FString(TEXT("Clear the injectors")));
	TestTrue(TEXT("Heat follows"), Rows[3].Text.ToString().StartsWith(TEXT("Cool crate")));
	Fuel.bFeedLeak = false; Fuel.bInjectorsFouled = false; Fuel.Units = 18;
	Rows = UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, &Fuel);
	TestEqual(TEXT("Closed wording"), Rows[0].Text.ToString(), FString(TEXT("Fuel 18% - open the bypass and feed casks")));
	TestFalse(TEXT("Low not red"), Rows[0].bFailed); Fuel.bBypassOpen = true;
	Rows = UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, &Fuel);
	TestEqual(TEXT("Open wording"), Rows[0].Text.ToString(), FString(TEXT("Fuel 18% - feed casks to the intake")));
	Fuel.Units = 15; TestFalse(TEXT("15 not critical"), UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, &Fuel)[0].bFailed);
	Fuel.Units = 14; TestTrue(TEXT("Below 15 critical"), UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, &Fuel)[0].bFailed);
	Fuel.BurningCrate = TEXT("C1");
	TestEqual(TEXT("Burn hides low row"), UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, &Fuel)[0].Text.ToString(), FString(TEXT("All systems nominal")));
	Fuel.BurningCrate = NAME_None; Fuel.Units = 30;
	TestEqual(TEXT("30 has no row"), UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, &Fuel)[0].Text.ToString(), FString(TEXT("All systems nominal")));
	C.State = ECargoState::Burned;
	TestEqual(TEXT("Burned no heat row"), UTaskBoardWidget::BuildRows({}, {}, {C})[0].Text.ToString(), FString(TEXT("All systems nominal")));
	TArray<FCargoCrateState> Cargo; C.State = ECargoState::Loose; for (int32 I=0; I<10; ++I) { Cargo.Add(C); }
	Fuel.bFeedLeak = true; Fuel.bInjectorsFouled = true;
	Rows = UTaskBoardWidget::BuildRows({}, {}, Cargo, &Air, &Fuel);
	TestEqual(TEXT("Cap still eight plus overflow"), Rows.Num(), 9); TestEqual(TEXT("Overflow count"), Rows.Last().Text.ToString(), FString(TEXT("+5 more")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFloorHazardTaskRowsTest, "Airlock.Floor.HazardInteraction.TaskBoardMedicalStillSpillPriorityAndOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFloorHazardTaskRowsTest::RunTest(const FString& Parameters)
{
	FRefineryState Refinery;
	FStillState Still; Still.Id = TEXT("Still_1"); Still.Status = EStillStatus::Popped; Refinery.Stills.Add(Still);
	FFloorHazardState Hazards;
	FFloorSpillState Spill; Spill.Id = TEXT("S1"); Spill.StillId = TEXT("Still_2"); Spill.Litres = 200; Hazards.Spills.Add(Spill);
	FAtmosphereState Air; Air.Ppm = 55;
	FFuelState Fuel; Fuel.bFeedLeak = true;
	auto Rows = UTaskBoardWidget::BuildRows({}, {}, {}, &Air, &Fuel, false, &Refinery, &Hazards, 1);
	if (!TestEqual(TEXT("Five priority rows"), Rows.Num(), 5)) { return false; }
	TestEqual(TEXT("Medical total first"), Rows[0].Text.ToString(), FString(TEXT("1 critical - 200 cr medical")));
	TestTrue(TEXT("Medical red"), Rows[0].bFailed);
	TestEqual(TEXT("Urgent still next"), Rows[1].Text.ToString(), FString(TEXT("Still_1 popped - reset the trap")));
	TestEqual(TEXT("Spill before fuel and air"), Rows[2].Text.ToString(), FString(TEXT("Spill by Still_2 - squeegee")));
	TestFalse(TEXT("Spill white"), Rows[2].bFailed);
	TestTrue(TEXT("Fuel follows spills"), Rows[3].Text.ToString().StartsWith(TEXT("Fuel leak")));
	TestTrue(TEXT("Air follows fuel"), Rows[4].Text.ToString().StartsWith(TEXT("Hold air 55")));
	Hazards.Spills.Reset(); Refinery.Stills.Reset();
	Rows = UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, nullptr, false, &Refinery, &Hazards, 2);
	TestEqual(TEXT("Clean floor retains medical total"), Rows[0].Text.ToString(), FString(TEXT("2 critical - 400 cr medical")));
	TestEqual(TEXT("No medical debt is nominal"), UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, nullptr, false, &Refinery, &Hazards)[0].Text.ToString(), FString(TEXT("All systems nominal")));
	for (int32 I = 1; I <= 10; ++I) { Spill.Id = FName(*FString::Printf(TEXT("S%d"), I)); Hazards.Spills.Add(Spill); }
	Rows = UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, nullptr, false, &Refinery, &Hazards, 1);
	TestEqual(TEXT("Eight rows plus overflow"), Rows.Num(), 9);
	TestEqual(TEXT("Medical priority survives overflow"), Rows[0].Text.ToString(), FString(TEXT("1 critical - 200 cr medical")));
	TestEqual(TEXT("Each puddle counted"), Rows.Last().Text.ToString(), FString(TEXT("+3 more")));
	return true;
}
#endif
