// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Ship/FuelSystem.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelRatesTest, "Airlock.Fuel.ResetNominalLeakAndReserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFuelRatesTest::RunTest(const FString& Parameters)
{
	FFuelSystem M; const FFuelRates R;
	TestTrue(TEXT("Defaults equal reset"), M.State() == FFuelState());
	TestEqual(TEXT("22.5 minute reserve"), M.SecondsOfFuelLeft(), 1350.f, 0.01f);
	TestFalse(TEXT("Enough for voyage"), M.IsShort(1200)); TestTrue(TEXT("Short for longer trip"), M.IsShort(1400));
	M.Tick(60); TestEqual(TEXT("90 -> 86 in a minute"), M.State().Units, R.StartFuel - R.NominalBurnPerMinute, 0.01f);
	M.Reset(); M.OpenLeak(); M.Tick(60);
	TestEqual(TEXT("Leak loses 40 extra"), M.State().Units, 46.f, 0.01f);
	TestTrue(TEXT("Shut leak"), M.CloseLeak()); TestFalse(TEXT("Shut once"), M.CloseLeak());
	M.Tick(60); TestEqual(TEXT("Only nominal after shut"), M.State().Units, 42.f, 0.01f);
	M.Reset(); for (int32 I = 0; I < 4800; ++I) { M.Tick(0.25f); }
	TestEqual(TEXT("Clean run retains ten units"), M.State().Units, 10.f, 0.01f);
	TestFalse(TEXT("Clean run never exhausts"), M.State().bExhausted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelBurnTest, "Airlock.Fuel.BypassBurnSootAndFouling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFuelBurnTest::RunTest(const FString& Parameters)
{
	FFuelSystem M; const FFuelRates R; M.SetUnits(20);
	TestFalse(TEXT("Closed refuses"), M.FeedCask(TEXT("C1")));
	TestTrue(TEXT("Opens"), M.OpenBypass()); TestFalse(TEXT("Opens once"), M.OpenBypass());
	TestFalse(TEXT("None refused"), M.FeedCask(NAME_None));
	for (int32 I = 1; I <= 3; ++I)
	{
		TestEqual(TEXT("Feed costs 10, 10, 20"), M.FoulingOnFeed(), I == 3 ? R.FoulingPerCaskFouled : R.FoulingPerCask);
		const float Before = M.State().Units;
		TestTrue(TEXT("Feed starts"), M.FeedCask(FName(*FString::Printf(TEXT("C%d"), I))));
		TestFalse(TEXT("Busy refuses"), M.FeedCask(TEXT("Other")));
		TestEqual(TEXT("Soot evenly over burn"), M.SootPerSecond(), R.SootPpmPerCask / R.CaskBurnSeconds);
		M.Tick(19.75f); TestEqual(TEXT("No early credit"), M.State().CasksBurned, I - 1);
		TestEqual(TEXT("Ceil countdown"), M.State().BurnSecondsLeft, 1);
		M.Tick(0.25f); TestEqual(TEXT("Completed count"), M.State().CasksBurned, I);
		TestEqual(TEXT("Exactly 15 credited after 20 s"), M.State().Units, Before + R.CaskFuel - R.NominalBurnPerMinute / 3.f, 0.01f);
		TestEqual(TEXT("Fouled from second completion"), M.State().bInjectorsFouled, I >= 2);
		TestEqual(TEXT("No soot afterwards"), M.SootPerSecond(), 0.f);
		TestTrue(TEXT("No burning crate"), M.State().BurningCrate.IsNone());
		TestEqual(TEXT("No countdown"), M.State().BurnSecondsLeft, 0);
	}
	TestTrue(TEXT("Clear"), M.ClearInjectors()); TestFalse(TEXT("Clear once"), M.ClearInjectors());
	TestEqual(TEXT("Cost restored"), M.FoulingOnFeed(), R.FoulingPerCask);
	M.FeedCask(TEXT("C4")); M.Tick(21); TestTrue(TEXT("Fourth completion fouls again"), M.State().bInjectorsFouled);
	M.Reset(); TestTrue(TEXT("Reset clears everything"), M.State() == FFuelState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelValidationTest, "Airlock.Fuel.ExhaustionClampInvalidAndPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFuelValidationTest::RunTest(const FString& Parameters)
{
	FFuelSystem M;
	for (float D : { 0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{ TestFalse(TEXT("Invalid delta no-op"), M.Tick(D)); TestTrue(TEXT("State unchanged"), M.State() == FFuelState()); }
	TestFalse(TEXT("NaN setter refused"), M.SetUnits(std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("Infinity setter refused"), M.SetUnits(std::numeric_limits<float>::infinity()));
	M.SetUnits(1000); TestEqual(TEXT("Upper clamp"), M.State().Units, 100.f);
	M.OpenBypass(); M.FeedCask(TEXT("C1")); M.Tick(20); TestEqual(TEXT("Credit clamps"), M.State().Units, 100.f);
	M.SetUnits(-10); TestEqual(TEXT("Lower clamp"), M.State().Units, 0.f);
	TestFalse(TEXT("Setter waits for exhaustion tick"), M.State().bExhausted);
	TestTrue(TEXT("Flag change publishes"), M.Tick(0.25f)); TestTrue(TEXT("Exhausted"), M.State().bExhausted);
	TestFalse(TEXT("Empty tick unchanged"), M.Tick(1)); M.FeedCask(TEXT("C2")); M.Tick(20);
	TestEqual(TEXT("Refills after exhaustion"), M.State().Units, 15.f, 0.01f); TestTrue(TEXT("Latched"), M.State().bExhausted);
	M.Reset(); TestTrue(TEXT("First fractional drain crosses floor"), M.Tick(0.25f));
	TestFalse(TEXT("Same whole unit"), M.Tick(0.25f)); M.OpenBypass(); M.FeedCask(TEXT("C3"));
	TestFalse(TEXT("Setters aren't tick changes"), M.Tick(0.25f));
	TestTrue(TEXT("Whole second publishes"), M.Tick(0.75f));
	TestEqual(TEXT("Summary format"), FFuelSystem().Summary(), FString(TEXT("Fuel 90% leak=0 bypass=closed burning=none burned=0 fouled=0 exhausted=0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelDeterminismTest, "Airlock.Fuel.DeterminismAndSplitTicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFuelDeterminismTest::RunTest(const FString& Parameters)
{
	FFuelSystem A, B;
	for (int32 I = 0; I < 5000; ++I)
	{
		if (I == 100) { A.OpenLeak(); B.OpenLeak(); }
		if (I == 200) { A.CloseLeak(); B.CloseLeak(); A.OpenBypass(); B.OpenBypass(); }
		if (I % 100 == 0) { A.FeedCask(TEXT("C1")); B.FeedCask(TEXT("C1")); }
		TestEqual(TEXT("Same changes"), A.Tick(0.25f), B.Tick(0.25f)); TestTrue(TEXT("Same state"), A.State() == B.State());
	}
	A.Reset(); B.Reset(); A.SetUnits(1); B.SetUnits(1); A.OpenBypass(); B.OpenBypass(); A.FeedCask(TEXT("C2")); B.FeedCask(TEXT("C2"));
	A.Tick(30); for (int32 I = 0; I < 120; ++I) { B.Tick(0.25f); }
	TestEqual(TEXT("Burn completes within long tick"), A.State().Units, B.State().Units, 0.01f);
	TestTrue(TEXT("Exhaustion before credit remains latched"), A.State().bExhausted && B.State().bExhausted);
	return true;
}
#endif
