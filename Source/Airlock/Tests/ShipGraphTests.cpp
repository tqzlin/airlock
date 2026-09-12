// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Ship/ShipGraph.h"
#include "Ship/ShipLayouts.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FShipGraph HaulerGraph()
{
    FShipGraph Graph;
    Graph.Build(*ShipLayouts::Find(TEXT("Hauler_Mk1")));
    return Graph;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphBuildHaulerTest, "Airlock.ShipGraph.BuildHauler",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphBuildHaulerTest::RunTest(const FString& Parameters)
{

    const FShipLayout* Layout = ShipLayouts::Find(TEXT("Hauler_Mk1"));
    if (!TestNotNull(TEXT("Hauler layout"), Layout)) { return false; }
    TestNull(TEXT("Hub has no graph"), ShipLayouts::Find(TEXT("StationHub")));
    TestNull(TEXT("Unknown section"), ShipLayouts::Find(TEXT("Missing")));
    FShipGraph G = HaulerGraph();
    TestEqual(TEXT("Seven modules"), G.Modules().Num(), 7);
    TestEqual(TEXT("Four lines"), G.Lines().Num(), 4);
    for (const auto& M : G.Modules())
    {
        TestEqual(TEXT("Full condition"), M.Condition, 100.f);
        TestTrue(TEXT("Nominal"), M.Status == EShipModuleStatus::Nominal);
    }
    for (const auto& L : G.Lines())
    {
        TestTrue(TEXT("No faults"), L.Fault == EShipLineFault::None);
        TestNotNull(TEXT("From exists"), G.FindModule(L.FromModule));
        TestNotNull(TEXT("To exists"), G.FindModule(L.ToModule));
    }
    G.DamageModule(TEXT("Galley"), 90);
    G.SetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
    G.Build(*Layout);
    TestTrue(TEXT("Rebuild restores full state"), G.Modules() == HaulerGraph().Modules() && G.Lines() == HaulerGraph().Lines());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphLeakDrainTest, "Airlock.ShipGraph.LeakDrain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphLeakDrainTest::RunTest(const FString& Parameters)
{

    FShipGraph G = HaulerGraph();
    G.SetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
    G.Tick(10);
    for (const auto& M : G.Modules()) { TestEqual(TEXT("Only coolant drains"), M.Condition, M.Id == TEXT("CoolantLoop") ? 90.f : 100.f); }
    G.SetLineFault(TEXT("Coolant_2"), EShipLineFault::Leak);
    G.Tick(5);
    TestEqual(TEXT("Two leaks add"), G.FindModule(TEXT("CoolantLoop"))->Condition, 80.f);
    G.Tick(10);
    TestTrue(TEXT("60 is still nominal"), G.FindModule(TEXT("CoolantLoop"))->Status == EShipModuleStatus::Nominal);
    G.Tick(0.25f);
    TestEqual(TEXT("Core waits until the next tick after threshold"), G.FindModule(TEXT("PowerCore"))->Condition, 100.f);
    G.Tick(2);
    TestEqual(TEXT("Degraded coolant now drifts core"), G.FindModule(TEXT("PowerCore"))->Condition, 99.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphCoolantPropagationTest, "Airlock.ShipGraph.CoolantPropagation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphCoolantPropagationTest::RunTest(const FString& Parameters)
{

    FShipGraph G = HaulerGraph();
    G.DamageModule(TEXT("CoolantLoop"), 50);
    G.Tick(10);
    TestEqual(TEXT("Degraded drift"), G.FindModule(TEXT("PowerCore"))->Condition, 95.f);
    G.DamageModule(TEXT("CoolantLoop"), 31);
    G.Tick(10);
    TestEqual(TEXT("Failed drift faster"), G.FindModule(TEXT("PowerCore"))->Condition, 80.f);
    TestEqual(TEXT("No self recovery"), G.FindModule(TEXT("CoolantLoop"))->Condition, 19.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphBrownoutTest, "Airlock.ShipGraph.Brownout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphBrownoutTest::RunTest(const FString& Parameters)
{

    for (const bool Failed : { false, true })
    {
        FShipGraph G = HaulerGraph();
        G.DamageModule(TEXT("PowerCore"), Failed ? 90 : 50);
        const auto Before = G.Modules();
        G.Tick(8);
        for (int32 I = 0; I < Before.Num(); ++I)
        {
            const bool Affected = Before[I].Type == EShipModuleType::LifeSupport || Before[I].Type == EShipModuleType::CargoClamps;
            TestEqual(TEXT("Brownout only hits life support and clamps"), G.Modules()[I].Condition,
                Before[I].Condition - (Affected ? (Failed ? 6.f : 2.f) : 0.f));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphShortDrainTest, "Airlock.ShipGraph.ShortDrain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphShortDrainTest::RunTest(const FString& Parameters)
{

    for (const FName Id : { FName(TEXT("Power_1")), FName(TEXT("Power_2")) })
    {
        FShipGraph G = HaulerGraph();
        G.SetLineFault(Id, EShipLineFault::Short);
        const FName Target = G.FindLine(Id)->ToModule;
        G.Tick(10);
        for (const auto& M : G.Modules()) { TestEqual(TEXT("Short only drains destination"), M.Condition, M.Id == Target ? 90.f : 100.f); }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphClearFaultStopsDrainTest, "Airlock.ShipGraph.ClearFaultStopsDrain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphClearFaultStopsDrainTest::RunTest(const FString& Parameters)
{

    for (const bool Leak : { false, true })
    {
        FShipGraph G = HaulerGraph();
        const FName Id = Leak ? TEXT("Coolant_1") : TEXT("Power_1");
        G.SetLineFault(Id, Leak ? EShipLineFault::Leak : EShipLineFault::Short);
        G.Tick(10);
        G.SetLineFault(Id, EShipLineFault::None);
        const auto Before = G.Modules();
        G.Tick(10);
        TestTrue(TEXT("Clearing stops direct drain on next tick, no recovery"), G.Modules() == Before);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphClampAndStatusThresholdsTest, "Airlock.ShipGraph.ClampAndStatusThresholds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphClampAndStatusThresholdsTest::RunTest(const FString& Parameters)
{

    FShipGraph G = HaulerGraph();
    G.DamageModule(TEXT("Nav"), 40);
    TestTrue(TEXT("60 nominal"), G.FindModule(TEXT("Nav"))->Status == EShipModuleStatus::Nominal);
    G.DamageModule(TEXT("Nav"), 0.01f);
    TestTrue(TEXT("Below 60 degraded"), G.FindModule(TEXT("Nav"))->Status == EShipModuleStatus::Degraded);
    G.DamageModule(TEXT("Galley"), 80);
    TestTrue(TEXT("20 degraded"), G.FindModule(TEXT("Galley"))->Status == EShipModuleStatus::Degraded);
    G.DamageModule(TEXT("Galley"), 0.01f);
    TestTrue(TEXT("Below 20 failed"), G.FindModule(TEXT("Galley"))->Status == EShipModuleStatus::Failed);
    G.DamageModule(TEXT("Nav"), 999);
    TestEqual(TEXT("Damage clamps zero"), G.FindModule(TEXT("Nav"))->Condition, 0.f);
    G.RepairModule(TEXT("Nav"), 999);
    TestEqual(TEXT("Repair clamps 100"), G.FindModule(TEXT("Nav"))->Condition, 100.f);
    TestTrue(TEXT("Repair recomputes status"), G.FindModule(TEXT("Nav"))->Status == EShipModuleStatus::Nominal);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphStatusChangesInLayoutOrderTest, "Airlock.ShipGraph.StatusChangesInLayoutOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphStatusChangesInLayoutOrderTest::RunTest(const FString& Parameters)
{

    FShipGraph G = HaulerGraph();
    G.DamageModule(TEXT("CoolantLoop"), 80);
    G.DamageModule(TEXT("PowerCore"), 40);
    G.SetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
    const auto Changed = G.Tick(1);
    const TArray<FName> Expected = { TEXT("PowerCore"), TEXT("CoolantLoop") };
    TestTrue(TEXT("Exactly changed modules in layout order"), Changed == Expected);
    TestEqual(TEXT("Stable statuses give empty result"), G.Tick(0.25f).Num(), 0);
    G.Tick(100000);
    for (const auto& M : G.Modules()) { TestTrue(TEXT("Tick clamps"), M.Condition >= 0 && M.Condition <= 100); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphDeterminismTest, "Airlock.ShipGraph.Determinism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphDeterminismTest::RunTest(const FString& Parameters)
{

    FShipGraph A = HaulerGraph(), B = HaulerGraph();
    A.SetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
    B.SetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
    A.SetLineFault(TEXT("Power_2"), EShipLineFault::Short);
    B.SetLineFault(TEXT("Power_2"), EShipLineFault::Short);
    for (int32 I = 0; I < 1600; ++I)
    {
        const float Delta = I % 2 ? 0.25f : 0.1f;
        TestTrue(TEXT("Identical transitions"), A.Tick(Delta) == B.Tick(Delta));
        TestTrue(TEXT("Identical state"), A.Modules() == B.Modules() && A.Lines() == B.Lines());
    }
    TestEqual(TEXT("Identical summary"), A.Summary(), B.Summary());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphUnknownIdsAndInvalidInputsTest, "Airlock.ShipGraph.UnknownIdsAndInvalidInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphUnknownIdsAndInvalidInputsTest::RunTest(const FString& Parameters)
{

    FShipGraph G = HaulerGraph();
    TestFalse(TEXT("Unknown fault"), G.SetLineFault(TEXT("Missing"), EShipLineFault::Leak));
    TestFalse(TEXT("Unknown damage"), G.DamageModule(TEXT("Missing"), 5));
    TestFalse(TEXT("Unknown repair"), G.RepairModule(TEXT("Missing"), 5));
    TestNull(TEXT("Unknown module"), G.FindModule(TEXT("Missing")));
    TestNull(TEXT("Unknown line"), G.FindLine(TEXT("Missing")));
    TestFalse(TEXT("Invalid enum"), G.SetLineFault(TEXT("Coolant_1"), static_cast<EShipLineFault>(255)));
    for (const float Bad : { -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
    {
        TestFalse(TEXT("Invalid damage"), G.DamageModule(TEXT("Nav"), Bad));
        TestFalse(TEXT("Invalid repair"), G.RepairModule(TEXT("Nav"), Bad));
    }
    TestTrue(TEXT("Invalid commands leave state intact"), G.Modules() == HaulerGraph().Modules());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipGraphInvalidDeltaNoOpTest, "Airlock.ShipGraph.InvalidDeltaNoOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShipGraphInvalidDeltaNoOpTest::RunTest(const FString& Parameters)
{

    FShipGraph G = HaulerGraph();
    G.SetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
    const auto Before = G.Modules();
    for (const float Bad : { 0.f, -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() })
    {
        TestEqual(TEXT("No transitions"), G.Tick(Bad).Num(), 0);
        TestTrue(TEXT("No state mutation"), G.Modules() == Before);
    }
    return true;
}

#endif
