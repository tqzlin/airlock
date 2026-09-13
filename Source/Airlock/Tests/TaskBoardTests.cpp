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
#endif
