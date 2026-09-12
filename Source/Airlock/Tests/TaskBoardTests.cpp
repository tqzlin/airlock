// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "UI/TaskBoardWidget.h"
#include "Ship/ShipLayouts.h"

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
#endif
