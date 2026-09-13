// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Ship/RefineryModel.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace RefineryTests
{
	const FName StillId(TEXT("Still_1"));
	TArray<FRefineryCask> Manifest(int32 Count)
	{
		TArray<FRefineryCask> Casks;
		for (int32 I = 1; I <= Count; ++I)
		{
			FRefineryCask C;
			C.Id = FName(*FString::Printf(TEXT("C%d"), I)); C.bLight = I % 2 == 0;
			Casks.Add(C);
		}
		return Casks;
	}
	struct FRun
	{
		float Ppm = 0.f;
		TArray<FRefineryEvent> Events;
		void Advance(FRefineryModel& M, int32 Ticks, float RoomPpm = 0.f)
		{
			for (int32 I = 0; I < Ticks; ++I)
			{
				float Added = 0.f; M.Tick(0.25f, RoomPpm, Added, Events); Ppm += Added;
			}
		}
		int32 Count(ERefineryEvent Type) const
		{
			return Events.FilterByPredicate([Type](const FRefineryEvent& E) { return E.Type == Type; }).Num();
		}
	};
	void Recipe(FRefineryModel& M, int32 Heat = 8, int32 Feed = 10, int32 Vent = 11)
	{
		M.SetKnob(StillId, ERefineryKnob::Heat, Heat);
		M.SetKnob(StillId, ERefineryKnob::Feed, Feed);
		M.SetKnob(StillId, ERefineryKnob::Vent, Vent);
		FRun Preheat; Preheat.Advance(M, 240);
	}
	// Construct isolated report/boundary fixtures without exposing a mutable gameplay API.
	FRefineryState& Fixture(FRefineryModel& M) { return const_cast<FRefineryState&>(M.State()); }
	bool SameEvent(const FRefineryEvent& A, const FRefineryEvent& B)
	{
		return A.Type == B.Type && A.StillId == B.StillId && A.CaskId == B.CaskId && A.Litres == B.Litres
			&& A.DieselL == B.DieselL && A.GasL == B.GasL && A.TarL == B.TarL;
	}
}
using namespace RefineryTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryBuildTest, "Airlock.Refinery.Model.BuildOrderAndDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryBuildTest::RunTest(const FString& Parameters)
{
	FRefineryModel M;
	TestTrue(TEXT("Empty before build"), M.State() == FRefineryState());
	const auto Casks = Manifest(4); M.Build(3, Casks);
	TestEqual(TEXT("Delivered"), M.State().CasksDelivered, 4);
	TestTrue(TEXT("Waiting preserves order and kind"), M.State().Waiting == Casks);
	TestEqual(TEXT("Three stills"), M.State().Stills.Num(), 3);
	for (int32 I = 0; I < 3; ++I)
	{
		FStillState Expected; Expected.Id = FName(*FString::Printf(TEXT("Still_%d"), I + 1));
		TestTrue(TEXT("Initial still"), M.State().Stills[I] == Expected);
		TestEqual(TEXT("Empty progress"), M.BatchProgress(Expected.Id), 0.f);
	}
	TestNull(TEXT("Unknown still"), M.FindStill(TEXT("Missing")));
	TestEqual(TEXT("Unknown progress"), M.BatchProgress(TEXT("Missing")), 0.f);
	FRefineryState Expected; Expected.Stills = M.State().Stills; Expected.Waiting = Casks; Expected.CasksDelivered = 4;
	TestTrue(TEXT("All totals zero"), M.State() == Expected);
	M.Rebuild(); TestTrue(TEXT("Rebuild resets identically"), M.State() == Expected);
	M.Build(-1, {}); TestTrue(TEXT("Negative still count makes an empty floor"), M.State() == FRefineryState());
	M.Rates.HeatBaseK = 100.f; M.Rates.PressureFloorBar = 1.2f; M.Build(1, Casks);
	TestEqual(TEXT("Build reads rates"), M.FindStill(StillId)->TempK, 100.f);
	TestEqual(TEXT("Build pressure reads rates"), M.FindStill(StillId)->PressureBar, 1.2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryKnobsTest, "Airlock.Refinery.Model.KnobStepsClampsAndRejections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryKnobsTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, {});
	TestFalse(TEXT("Unknown set"), M.SetKnob(TEXT("Missing"), ERefineryKnob::Heat, 1));
	TestFalse(TEXT("Unknown step"), M.StepKnob(TEXT("Missing"), ERefineryKnob::Heat, 1));
	TestFalse(TEXT("Zero steps"), M.StepKnob(StillId, ERefineryKnob::Heat, 0));
	TestFalse(TEXT("Unknown knob"), M.SetKnob(StillId, static_cast<ERefineryKnob>(255), 1));
	TestTrue(TEXT("Heat advances"), M.StepKnob(StillId, ERefineryKnob::Heat, 8));
	TestEqual(TEXT("Eight exact steps"), M.FindStill(StillId)->HeatSteps, 8);
	M.StepKnob(StillId, ERefineryKnob::Heat, MAX_int32);
	TestEqual(TEXT("Extreme addition saturates"), M.FindStill(StillId)->HeatSteps, 20);
	M.StepKnob(StillId, ERefineryKnob::Heat, MIN_int32);
	TestEqual(TEXT("Extreme subtraction saturates"), M.FindStill(StillId)->HeatSteps, 0);
	M.SetKnob(StillId, ERefineryKnob::Feed, 90); M.SetKnob(StillId, ERefineryKnob::Vent, -2);
	TestEqual(TEXT("Feed upper clamp"), M.FindStill(StillId)->FeedSteps, 20);
	TestEqual(TEXT("Vent lower clamp"), M.FindStill(StillId)->VentSteps, 0);
	TestTrue(TEXT("Setting same value is accepted"), M.SetKnob(StillId, ERefineryKnob::Feed, 20));
	TestTrue(TEXT("Step at limit is accepted"), M.StepKnob(StillId, ERefineryKnob::Feed, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryHopperTest, "Airlock.Refinery.Model.HopperCapacityAndRebuildOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryHopperTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; const auto Casks = Manifest(5); M.Build(2, Casks);
	const FRefineryState Initial = M.State();
	TestFalse(TEXT("Unknown still"), M.LoadHopper(TEXT("Missing"), TEXT("C1")));
	TestFalse(TEXT("Unknown cask"), M.LoadHopper(StillId, TEXT("Missing")));
	TestTrue(TEXT("Load in chosen order"), M.LoadHopper(StillId, TEXT("C3")));
	TestFalse(TEXT("No double load"), M.LoadHopper(StillId, TEXT("C3")));
	M.LoadHopper(StillId, TEXT("C1")); M.LoadHopper(StillId, TEXT("C2"));
	TestFalse(TEXT("Fourth hopper cask refused"), M.LoadHopper(StillId, TEXT("C4")));
	FRun Run; Run.Advance(M, 1);
	TestEqual(TEXT("First chosen cask goes to separator"), M.FindStill(StillId)->CurrentCask, FName(TEXT("C3")));
	TestEqual(TEXT("Loaded charge"), M.FindStill(StillId)->ChargeL, 1000.f);
	TestTrue(TEXT("Fourth still cask fits once separator occupied"), M.LoadHopper(StillId, TEXT("C4")));
	M.LoadHopper(TEXT("Still_2"), TEXT("C5"));
	M.Rebuild(); TestTrue(TEXT("All casks return in original build order"), M.State() == Initial);
	TestTrue(TEXT("Refused hopper overflow never spills"), Run.Events.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryHeavyTest, "Airlock.Refinery.Model.HeavyRecipeActualYieldAndHundredSeconds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryHeavyTest::RunTest(const FString& Parameters)
{
	for (const bool NoDrift : { false, true })
	{
		FRefineryModel M; M.Build(1, Manifest(1)); if (NoDrift) { M.Rates.CalDriftPerS = 0.f; }
		Recipe(M); M.LoadHopper(StillId, TEXT("C1")); FRun Run; Run.Advance(M, 399);
		TestEqual(TEXT("Not early"), M.State().CasksRefined, 0);
		TestEqual(TEXT("Last 2.5 litres"), M.FindStill(StillId)->ChargeL, 2.5f);
		TestEqual(TEXT("Progress"), M.BatchProgress(StillId), 0.9975f, 0.00001f);
		Run.Advance(M, 1);
		TestEqual(TEXT("Done at 100 s"), M.State().CasksRefined, 1);
		TestEqual(TEXT("One batch event"), Run.Count(ERefineryEvent::BatchDone), 1);
		TestEqual(TEXT("Exactly empty"), M.FindStill(StillId)->ChargeL, 0.f);
		TestEqual(TEXT("Idle without another cask"), M.FindStill(StillId)->Status, EStillStatus::Idle);
		TestEqual(TEXT("No loaded progress"), M.BatchProgress(StillId), 0.f);
		TestEqual(TEXT("Every litre accounted"), M.State().TankDieselL + M.State().TotalGasL + M.State().TotalTarL, 1000.f, 0.5f);
		TestEqual(TEXT("Normal gas fumes"), Run.Ppm, 2.5f, 0.01f);
		if (NoDrift) { TestTrue(TEXT("No drift exceeds 83 percent"), M.State().TankDieselL >= 830.f); }
		else
		{
			// Brief contradiction: the specified 60 s preheat and drift yield 74.740%, below 75%.
			TestEqual(TEXT("Actual specified recipe diesel"), M.State().TankDieselL, 747.40f, 0.5f);
			TestTrue(TEXT("Falls short of the requested 750 L"), M.State().TankDieselL < 750.f);
			AddInfo(FString::Printf(TEXT("Heavy recipe: %.3f L diesel at 100 s (brief asks >=750 L)"), M.State().TankDieselL));
		}
		const FRefineryEvent& E = Run.Events.Last();
		TestEqual(TEXT("Batch identity"), E.CaskId, FName(TEXT("C1")));
		TestEqual(TEXT("Batch still"), E.StillId, StillId);
		TestEqual(TEXT("Processed event litres"), E.Litres, 1000.f, 0.5f);
		TestEqual(TEXT("Event matches continuous tank"), E.DieselL, M.State().TankDieselL);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryLightTest, "Airlock.Refinery.Model.LightRecipeBeatsHeavyHeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryLightTest::RunTest(const FString& Parameters)
{
	float Yields[2];
	for (int32 I = 0; I < 2; ++I)
	{
		FRefineryModel M; M.Build(1, Manifest(2)); Recipe(M, I == 0 ? 6 : 8);
		M.LoadHopper(StillId, TEXT("C2")); FRun Run; Run.Advance(M, 400);
		Yields[I] = M.State().TankDieselL;
		TestEqual(TEXT("Light completes"), M.State().CasksRefined, 1);
		TestTrue(TEXT("Light flag preserved"), M.FindStill(StillId)->bCurrentLight);
	}
	TestTrue(TEXT("30 percent heat outperforms 40 for light crude"), Yields[0] > Yields[1]);
	TestTrue(TEXT("Light recipe above 74 percent"), Yields[0] > 740.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryBathTest, "Airlock.Refinery.Model.BathLagCoolingAndFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryBathTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(1)); Recipe(M);
	TestEqual(TEXT("Empty preheat approaches 196 K"), M.FindStill(StillId)->TempK, 196.f, 0.3f);
	M.LoadHopper(StillId, TEXT("C1")); FRun Run; Run.Advance(M, 400);
	TestEqual(TEXT("Feed pulls bath 12 K below target"), M.FindStill(StillId)->TempK, 184.f, 1.f);
	M.SetKnob(StillId, ERefineryKnob::Heat, 0); Run.Advance(M, 240);
	TestEqual(TEXT("Heat zero cools through lag"), M.FindStill(StillId)->TempK, 92.f, 0.3f);
	M.Rebuild(); M.SetKnob(StillId, ERefineryKnob::Feed, 18); M.LoadHopper(StillId, TEXT("C1"));
	FRun Cold; Cold.Advance(M, 40);
	TestEqual(TEXT("Two cold floods"), Cold.Count(ERefineryEvent::Spill), 2);
	TestEqual(TEXT("Never colder than crude"), M.FindStill(StillId)->TempK, M.Rates.HeatBaseK);
	TestEqual(TEXT("Cold pressure stays at floor"), M.FindStill(StillId)->PressureBar, M.Rates.PressureFloorBar);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryCalibrationTest, "Airlock.Refinery.Model.CalibrationOnlyDriftsWhileFeeding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryCalibrationTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(1)); Recipe(M, 8, 0);
	M.LoadHopper(StillId, TEXT("C1")); FRun Run; Run.Advance(M, 240);
	TestEqual(TEXT("Loaded preheat does not drift"), M.FindStill(StillId)->Calibration, 1.f);
	M.SetKnob(StillId, ERefineryKnob::Feed, 10); Run.Advance(M, 200);
	TestEqual(TEXT("50 s drift"), M.FindStill(StillId)->Calibration, 0.8f, 0.0001f);
	float Before = M.State().TankDieselL; Run.Advance(M, 1);
	const float DriftedYield = M.State().TankDieselL - Before;
	TestTrue(TEXT("Calibrate accepted"), M.Calibrate(StillId));
	TestEqual(TEXT("Trap restored"), M.FindStill(StillId)->Calibration, 1.f);
	Before = M.State().TankDieselL; Run.Advance(M, 1);
	TestTrue(TEXT("Restored trap raises next slice yield"), M.State().TankDieselL - Before > DriftedYield);
	M.SetKnob(StillId, ERefineryKnob::Feed, 0);
	const float Paused = M.FindStill(StillId)->Calibration; Run.Advance(M, 4);
	TestEqual(TEXT("Paused feed preserves trap"), M.FindStill(StillId)->Calibration, Paused);
	TestFalse(TEXT("Unknown calibration"), M.Calibrate(TEXT("Missing")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryFloodTest, "Airlock.Refinery.Model.FloodTimingCoolingResetAndConservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryFloodTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(1)); Recipe(M, 8, 18);
	M.LoadHopper(StillId, TEXT("C1")); FRun Run; Run.Advance(M, 19);
	TestEqual(TEXT("No early flood"), M.State().Spills, 0);
	const float Before = M.FindStill(StillId)->TempK;
	const float WithoutFlood = Before + (196.f - Before) * 0.025f - 4.5f * 0.12f;
	Run.Advance(M, 1);
	TestEqual(TEXT("One flood at five seconds"), Run.Count(ERefineryEvent::Spill), 1);
	TestEqual(TEXT("Exactly 200 L spill"), Run.Events[0].Litres, 200.f);
	TestEqual(TEXT("Charge subtracts feed and spill"), M.FindStill(StillId)->ChargeL, 710.f);
	TestEqual(TEXT("Flood drops bath 30 K"), M.FindStill(StillId)->TempK, WithoutFlood - 30.f, 0.001f);
	TestEqual(TEXT("Flood clock reset"), M.FindStill(StillId)->FloodSeconds, 0.f);
	Run.Advance(M, 3); M.SetKnob(StillId, ERefineryKnob::Feed, 16); Run.Advance(M, 1);
	TestEqual(TEXT("Exactly 80 percent resets"), M.FindStill(StillId)->FloodSeconds, 0.f);
	const auto& S = M.State();
	TestEqual(TEXT("Live conservation with spilled and loaded litres"), S.TankDieselL + S.TotalGasL + S.TotalTarL + S.SpilledL + S.Stills[0].ChargeL, 1000.f, 0.5f);
	Run.Advance(M, 200);
	TestEqual(TEXT("Batch completes after flooding"), M.State().CasksRefined, 1);
	TestEqual(TEXT("Flood clock clear on leaving Running"), M.FindStill(StillId)->FloodSeconds, 0.f);
	TestEqual(TEXT("Finished conservation"), M.State().TankDieselL + M.State().TotalGasL + M.State().TotalTarL + M.State().SpilledL, 1000.f, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryQueueTest, "Airlock.Refinery.Model.FourBatchesWithoutDrySlice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryQueueTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(4)); Recipe(M);
	M.LoadHopper(StillId, TEXT("C1")); M.LoadHopper(StillId, TEXT("C2")); M.LoadHopper(StillId, TEXT("C3"));
	FRun Run; Run.Advance(M, 1); M.LoadHopper(StillId, TEXT("C4"));
	Run.Advance(M, 399);
	TestEqual(TEXT("Running across batch boundary"), M.FindStill(StillId)->Status, EStillStatus::Running);
	Run.Advance(M, 1);
	TestEqual(TEXT("Next slice feeds next cask"), M.FindStill(StillId)->ChargeL, 997.5f);
	TestEqual(TEXT("Next cask"), M.FindStill(StillId)->CurrentCask, FName(TEXT("C2")));
	Run.Advance(M, 1199);
	TestEqual(TEXT("Four casks done in 400 s"), M.State().CasksRefined, 4);
	TestEqual(TEXT("Four completion events"), Run.Count(ERefineryEvent::BatchDone), 4);
	TestEqual(TEXT("Per still batches"), M.FindStill(StillId)->BatchesDone, 4);
	TestEqual(TEXT("Calibration floored at zero"), M.FindStill(StillId)->Calibration, 0.f);
	TestEqual(TEXT("Four casks conserved"), M.State().TankDieselL + M.State().TotalGasL + M.State().TotalTarL, 4000.f, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryTickTest, "Airlock.Refinery.Model.InvalidDeltaPublicationAndSubsteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryTickTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(1)); const auto Before = M.State();
	TArray<FRefineryEvent> Events; Events.AddDefaulted();
	for (float Dt : { 0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{
		float Ppm = 123.f; TestFalse(TEXT("Bad delta ignored"), M.Tick(Dt, 40.f, Ppm, Events));
		TestEqual(TEXT("Output ppm reset"), Ppm, 0.f);
		TestEqual(TEXT("Existing events preserved"), Events.Num(), 1);
		TestTrue(TEXT("Bad delta preserves state"), M.State() == Before);
	}
	float Ppm = 0.f; TestFalse(TEXT("Idle tick unchanged"), M.Tick(0.25f, 0.f, Ppm, Events));
	M.SetKnob(StillId, ERefineryKnob::Heat, 1);
	TestFalse(TEXT("Fractional heat alone does not publish"), M.Tick(0.25f, 0.f, Ppm, Events));
	TestTrue(TEXT("Precise temperature kept"), M.FindStill(StillId)->TempK > 92.f);
	FRefineryModel A; A.Build(1, Manifest(1)); Recipe(A, 8, 18); A.LoadHopper(StillId, TEXT("C1"));
	FRun Warm; Warm.Advance(A, 16); FRefineryModel B = A;
	TArray<FRefineryEvent> LargeEvents; float LargePpm = 0.f; A.Tick(1.f, 0.f, LargePpm, LargeEvents);
	FRun Small; Small.Advance(B, 4);
	TestTrue(TEXT("1 s equals four 0.25 s in every state field"), A.State() == B.State());
	TestEqual(TEXT("Single flood across large tick"), LargeEvents.Num(), 1);
	TestEqual(TEXT("Substepped flood still exactly 200 L"), LargeEvents[0].Litres, 200.f);
	TestEqual(TEXT("Identical ppm accumulation"), LargePpm, Small.Ppm);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryPressureTest, "Airlock.Refinery.Model.PressureActualPeaksWithDepletingCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryPressureTest::RunTest(const FString& Parameters)
{
	for (int32 Vent : { 11, 4 })
	{
		FRefineryModel M; M.Build(1, Manifest(1)); Recipe(M, 8, 10, Vent); M.LoadHopper(StillId, TEXT("C1"));
		FRun Run; float Peak = 1.f;
		for (int32 I = 0; I < 400; ++I)
		{
			Run.Advance(M, 1); Peak = FMath::Max(Peak, M.FindStill(StillId)->PressureBar);
			TestTrue(TEXT("Pressure never below floor"), M.FindStill(StillId)->PressureBar >= 1.f);
		}
		TestEqual(TEXT("No pop"), Run.Count(ERefineryEvent::Pop), 0);
		TestEqual(TEXT("Recipe completes"), M.State().CasksRefined, 1);
		if (Vent == 11) { TestEqual(TEXT("Recipe peak about 1.6 bar"), Peak, 1.56f, 0.01f); }
		else
		{
			// Brief's 2.8 bar equilibrium assumes constant full charge. This batch empties as it feeds.
			TestEqual(TEXT("Actual vent 20 peak"), Peak, 2.253f, 0.01f);
			TestTrue(TEXT("Never reaches the requested warning band"), Peak < M.Rates.WarnBar);
			AddInfo(FString::Printf(TEXT("Vent 20%% peaks at %.4f bar, below WarnBar 2.5 (depleting charge)"), Peak));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryVentTest, "Airlock.Refinery.Model.VentExcessNeedsBoilingAndScales",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryVentTest::RunTest(const FString& Parameters)
{
	float Ppm[3]; const int32 Vents[] = { 10, 13, 18 };
	for (int32 I = 0; I < 3; ++I)
	{
		FRefineryModel M; M.Build(1, Manifest(1)); Recipe(M, 8, 0, Vents[I]);
		FRun Empty; Empty.Advance(M, 4);
		TestEqual(TEXT("Empty hot still does not vent gas"), Empty.Ppm, 0.f);
		M.LoadHopper(StillId, TEXT("C1")); FRun Boil; Boil.Advance(M, 4); Ppm[I] = Boil.Ppm;
		TestEqual(TEXT("No feed gas cut"), M.State().TotalGasL, 0.f);
		M.Rebuild(); M.SetKnob(StillId, ERefineryKnob::Vent, Vents[I]); M.LoadHopper(StillId, TEXT("C1"));
		FRun Cold; Cold.Advance(M, 4);
		TestEqual(TEXT("Cold charge does not vent excess"), Cold.Ppm, 0.f);
	}
	TestEqual(TEXT("50 percent adds no vent ppm"), Ppm[0], 0.f);
	TestTrue(TEXT("65 percent fumes"), Ppm[1] > 0.f);
	TestTrue(TEXT("90 percent fumes more"), Ppm[2] > Ppm[1]);
	TestEqual(TEXT("Six times the excess"), Ppm[2], Ppm[1] * 6.f, 0.001f);
	TestEqual(TEXT("Boiling-scaled vent 90"), Ppm[2], 7.90f, 0.03f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryPopTest, "Airlock.Refinery.Model.PopOnceSpillRetainedProductAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryPopTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(3)); Recipe(M, 10, 10, 2);
	TestFalse(TEXT("Unknown reset"), M.ClearPop(TEXT("Missing")));
	TestFalse(TEXT("Idle reset refused"), M.ClearPop(StillId));
	M.LoadHopper(StillId, TEXT("C1")); FRun Run; Run.Advance(M, 1);
	TestFalse(TEXT("Running reset refused"), M.ClearPop(StillId));
	float RedSeconds = 0.f; int32 Ticks = 1;
	for (; Ticks < 400 && M.State().Pops == 0; ++Ticks)
	{
		const float BeforePpm = Run.Ppm, BeforeGas = M.State().TotalGasL;
		Run.Advance(M, 1);
		if (M.FindStill(StillId)->PressureBar >= M.Rates.WarnBar) { RedSeconds += 0.25f; }
		if (M.State().Pops > 0)
		{
			TestEqual(TEXT("Pop adds exactly 60 beyond slice gas"), Run.Ppm - BeforePpm
				- (M.State().TotalGasL - BeforeGas) * M.Rates.RoomPpmPerGasL, 60.f, 0.001f);
		}
	}
	TestEqual(TEXT("Pop at 16.5 s"), Ticks, 66);
	TestEqual(TEXT("One pop event"), Run.Count(ERefineryEvent::Pop), 1);
	TestEqual(TEXT("One spill event"), Run.Count(ERefineryEvent::Spill), 1);
	TestEqual(TEXT("Pop first"), Run.Events[0].Type, ERefineryEvent::Pop);
	TestEqual(TEXT("Spill follows"), Run.Events[1].Type, ERefineryEvent::Spill);
	TestEqual(TEXT("Pop spill identity"), Run.Events[1].CaskId, FName(TEXT("C1")));
	TestEqual(TEXT("300 L spill"), Run.Events[1].Litres, 300.f);
	TestEqual(TEXT("Lost cask once"), M.State().CasksLost, 1);
	TestEqual(TEXT("Unfed charge cleared"), M.FindStill(StillId)->ChargeL, 0.f);
	TestTrue(TEXT("Cask cleared"), M.FindStill(StillId)->CurrentCask.IsNone());
	TestEqual(TEXT("Popped"), M.FindStill(StillId)->Status, EStillStatus::Popped);
	TestEqual(TEXT("Pressure reset"), M.FindStill(StillId)->PressureBar, 1.f);
	TestEqual(TEXT("Flood timer reset"), M.FindStill(StillId)->FloodSeconds, 0.f);
	TestEqual(TEXT("Warning only 5.5 s before pop"), RedSeconds, 5.5f);
	AddInfo(TEXT("Heat 50% / vent 10% pops at 16.5 s; only 5.5 s observed in warning band, not tens of seconds."));
	const float Tank = M.State().TankDieselL, Gas = M.State().TotalGasL, Ppm = Run.Ppm;
	M.SetKnob(StillId, ERefineryKnob::Feed, 18);
	TestTrue(TEXT("Queue while popped"), M.LoadHopper(StillId, TEXT("C2")));
	Run.Advance(M, 400);
	TestEqual(TEXT("Popped still never feeds or emits again"), Run.Events.Num(), 2);
	TestEqual(TEXT("Popped ppm stops"), Run.Ppm, Ppm);
	TestEqual(TEXT("Tank survives pop"), M.State().TankDieselL, Tank);
	TestEqual(TEXT("Processed gas survives pop"), M.State().TotalGasL, Gas);
	const FStillState Popped = *M.FindStill(StillId);
	TestTrue(TEXT("Clear pop accepted"), M.ClearPop(StillId));
	FStillState Expected = Popped; Expected.Status = EStillStatus::Idle; Expected.PressureBar = 1.f;
	TestTrue(TEXT("Reset changes only status and pressure"), *M.FindStill(StillId) == Expected);
	M.SetKnob(StillId, ERefineryKnob::Feed, 10); M.SetKnob(StillId, ERefineryKnob::Vent, 11);
	Run.Advance(M, 1);
	TestEqual(TEXT("Queued cask resumes"), M.FindStill(StillId)->CurrentCask, FName(TEXT("C2")));
	TestEqual(TEXT("Resumed charge"), M.FindStill(StillId)->ChargeL, 997.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryHotTest, "Airlock.Refinery.Model.HotBatchGasTarAndActualCoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryHotTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(1)); Recipe(M, 16, 10, 13);
	TestEqual(TEXT("Preheat near 300 K"), M.FindStill(StillId)->TempK, 300.f, 0.5f);
	M.LoadHopper(StillId, TEXT("C1")); FRun Run; Run.Advance(M, 400);
	TestEqual(TEXT("Hot cask completes"), M.State().CasksRefined, 1);
	TestEqual(TEXT("No hot diesel"), M.State().TankDieselL, 0.f, 0.5f);
	TestEqual(TEXT("Hot gas"), M.State().TotalGasL, 650.f, 0.5f);
	TestEqual(TEXT("Hot tar"), M.State().TotalTarL, 350.f, 0.5f);
	TestTrue(TEXT("Gas cut plus vent excess fumes"), Run.Ppm > 650.f * M.Rates.RoomPpmPerGasL);
	// Brief contradiction: 12 K cooling takes the 300 K bath to 288 K, below the 290 K coke threshold.
	TestEqual(TEXT("Actual coke after preheat and one batch"), M.FindStill(StillId)->Coke, 46.5f, 0.01f);
	TestEqual(TEXT("Bath settles below coke threshold"), M.FindStill(StillId)->TempK, 288.f, 0.01f);
	TestFalse(TEXT("Specified hot recipe never fouls"), M.FindStill(StillId)->bFouled);
	TestEqual(TEXT("No Fouled event rather than the requested one"), Run.Count(ERefineryEvent::Fouled), 0);
	AddInfo(TEXT("Heat 80% / feed 50%: 0/650/350 L, 288 K, coke 46.5; not the brief's coke 90..95 or Fouled."));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryFlashTest, "Airlock.Refinery.Model.FlashThresholdsLossAndWarmPurge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryFlashTest::RunTest(const FString& Parameters)
{
	FRefineryModel Hot; Hot.Build(1, Manifest(2)); Recipe(Hot, 18, 10, 13);
	TestFalse(TEXT("Unknown purge"), Hot.Purge(TEXT("Missing")));
	TestFalse(TEXT("Idle purge refused"), Hot.Purge(StillId));
	Hot.LoadHopper(StillId, TEXT("C1")); Hot.LoadHopper(StillId, TEXT("C2"));
	for (float Room : { 39.99f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
	{
		FRefineryModel DryAir = Hot; FRun Run; Run.Advance(DryAir, 1, Room);
		TestEqual(TEXT("Hot alone or nonfinite room never flashes"), DryAir.State().Flashes, 0);
		TestEqual(TEXT("Still running"), DryAir.FindStill(StillId)->Status, EStillStatus::Running);
		TestFalse(TEXT("Running purge refused"), DryAir.Purge(StillId));
	}
	FRefineryModel Cool; Cool.Build(1, Manifest(1)); Recipe(Cool); Cool.LoadHopper(StillId, TEXT("C1"));
	FRun Safe; Safe.Advance(Cool, 1, 500.f);
	TestEqual(TEXT("Gas alone never flashes"), Cool.State().Flashes, 0);
	FRefineryModel Control = Hot; FRun NoFlash; NoFlash.Advance(Control, 1, 0.f);
	FRun Run; Run.Advance(Hot, 1, 40.f);
	TestEqual(TEXT("Exactly 40 ppm triggers"), Run.Count(ERefineryEvent::Flash), 1);
	TestEqual(TEXT("Flash adds 40 beyond gas and vent"), Run.Ppm - NoFlash.Ppm, 40.f, 0.001f);
	TestEqual(TEXT("Flash status"), Hot.FindStill(StillId)->Status, EStillStatus::Flash);
	TestEqual(TEXT("Thirty condition lost"), Hot.FindStill(StillId)->Condition, 70.f);
	TestEqual(TEXT("Charge lost once"), Hot.State().CasksLost, 1);
	TestEqual(TEXT("Charge gone"), Hot.FindStill(StillId)->ChargeL, 0.f);
	TestEqual(TEXT("Processed gas kept"), Hot.State().TotalGasL, Control.State().TotalGasL);
	TestEqual(TEXT("Processed tar kept"), Hot.State().TotalTarL, Control.State().TotalTarL);
	TestEqual(TEXT("Flash identity"), Run.Events[0].CaskId, FName(TEXT("C1")));
	const float Coke = Hot.FindStill(StillId)->Coke, Temp = Hot.FindStill(StillId)->TempK;
	Run.Advance(Hot, 4, 40.f);
	TestEqual(TEXT("Latched fire does not repeat"), Hot.State().Flashes, 1);
	TestTrue(TEXT("Flashed coils keep cooking"), Hot.FindStill(StillId)->TempK > Temp);
	TestEqual(TEXT("Hot and dry coke both accrue in flash"), Hot.FindStill(StillId)->Coke - Coke, 0.9f, 0.001f);
	const FStillState Before = *Hot.FindStill(StillId);
	TestTrue(TEXT("Purge accepted"), Hot.Purge(StillId));
	FStillState Expected = Before; Expected.Status = EStillStatus::Idle; Expected.PressureBar = 1.f;
	TestTrue(TEXT("Purge preserves warm bath, damage, coke and hopper"), *Hot.FindStill(StillId) == Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryFourFlashesTest, "Airlock.Refinery.Model.FourFlashesFailAndCutCoils",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryFourFlashesTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Rates.CokeHotRate = M.Rates.CokeDryRate = 0.f;
	M.Build(1, Manifest(4)); Recipe(M, 18, 10, 13); FRun Run;
	for (int32 I = 1; I <= 4; ++I)
	{
		M.LoadHopper(StillId, FName(*FString::Printf(TEXT("C%d"), I))); Run.Advance(M, 1, 40.f);
		TestEqual(TEXT("One lost per flash"), M.State().CasksLost, I);
		TestEqual(TEXT("Condition clamps at zero"), M.FindStill(StillId)->Condition, FMath::Max(0.f, 100.f - 30.f * I));
		if (I < 4) { TestTrue(TEXT("Purge between flashes"), M.Purge(StillId)); }
	}
	TestEqual(TEXT("Four flashes"), Run.Count(ERefineryEvent::Flash), 4);
	TestEqual(TEXT("One failure"), Run.Count(ERefineryEvent::Failed), 1);
	TestEqual(TEXT("Flash then failure"), Run.Events[3].Type, ERefineryEvent::Flash);
	TestEqual(TEXT("Failure follows fourth flash"), Run.Events[4].Type, ERefineryEvent::Failed);
	TestEqual(TEXT("Failure keeps cask identity"), Run.Events[4].CaskId, FName(TEXT("C4")));
	TestEqual(TEXT("Failed"), M.FindStill(StillId)->Status, EStillStatus::Failed);
	TestEqual(TEXT("Coils cut"), M.FindStill(StillId)->HeatSteps, 0);
	TestFalse(TEXT("Failed cannot purge"), M.Purge(StillId));
	TestFalse(TEXT("Failed cannot calibrate"), M.Calibrate(StillId));
	TestFalse(TEXT("Failed cannot reset pop"), M.ClearPop(StillId));
	const float Temp = M.FindStill(StillId)->TempK; Run.Advance(M, 4, 500.f);
	TestTrue(TEXT("Failed bath cools"), M.FindStill(StillId)->TempK < Temp);
	TestEqual(TEXT("No repeated failure or phantom cask loss"), M.State().CasksLost, 4);
	TestEqual(TEXT("Events stay latched"), Run.Events.Num(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryFouledTest, "Airlock.Refinery.Model.DryCokeFoulsButKeepsFeedingAtHalfYield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryFouledTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(1)); M.SetKnob(StillId, ERefineryKnob::Heat, 12);
	FRun Dry; Dry.Advance(M, 801);
	TestEqual(TEXT("Dry source alone at 0.3/s"), M.FindStill(StillId)->Coke, 60.075f, 0.01f);
	TestEqual(TEXT("Crossing emits once"), Dry.Count(ERefineryEvent::Fouled), 1);
	TestTrue(TEXT("Fouled mirror"), M.FindStill(StillId)->bFouled);
	TestEqual(TEXT("Fouled is not a status"), M.FindStill(StillId)->Status, EStillStatus::Idle);
	Recipe(M); FRefineryModel Clean = M;
	TestTrue(TEXT("Idle scraping allowed"), Clean.Scrape(StillId));
	M.LoadHopper(StillId, TEXT("C1")); Clean.LoadHopper(StillId, TEXT("C1"));
	FRun DirtyRun, CleanRun; DirtyRun.Advance(M, 200); CleanRun.Advance(Clean, 200);
	TestEqual(TEXT("Fouled continues running"), M.FindStill(StillId)->Status, EStillStatus::Running);
	TestEqual(TEXT("Feed rate unchanged"), M.FindStill(StillId)->ChargeL, 500.f);
	TestFalse(TEXT("Cannot scrape a running batch"), M.Scrape(StillId));
	DirtyRun.Advance(M, 200); CleanRun.Advance(Clean, 200);
	TestEqual(TEXT("Fouled batch still completes"), M.State().CasksRefined, 1);
	TestEqual(TEXT("Exactly half clean diesel"), M.State().TankDieselL, Clean.State().TankDieselL * 0.5f, 0.01f);
	TestEqual(TEXT("No repeat Fouled while over threshold"), DirtyRun.Count(ERefineryEvent::Fouled), 0);
	TestTrue(TEXT("Scrape after batch"), M.Scrape(StillId));
	M.SetKnob(StillId, ERefineryKnob::Heat, 12); DirtyRun.Advance(M, 801);
	TestEqual(TEXT("Scrape re-arms crossing"), DirtyRun.Count(ERefineryEvent::Fouled), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryCokeFailureTest, "Airlock.Refinery.Model.CokeFailureRepairAndRearmedFouling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryCokeFailureTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(2)); Recipe(M, 16, 10, 13); M.LoadHopper(StillId, TEXT("C1"));
	FRun Run; Run.Advance(M, 80); M.SetKnob(StillId, ERefineryKnob::Feed, 0);
	const float Made = M.State().TankDieselL + M.State().TotalGasL + M.State().TotalTarL;
	for (int32 I = 0; I < 800 && M.FindStill(StillId)->Status != EStillStatus::Failed; ++I) { Run.Advance(M, 1); }
	TestEqual(TEXT("Coke fails the loaded still"), M.FindStill(StillId)->Status, EStillStatus::Failed);
	TestEqual(TEXT("Coke capped"), M.FindStill(StillId)->Coke, 100.f);
	TestEqual(TEXT("Heat cut"), M.FindStill(StillId)->HeatSteps, 0);
	TestEqual(TEXT("Lost one loaded cask"), M.State().CasksLost, 1);
	TestEqual(TEXT("Charge gone"), M.FindStill(StillId)->ChargeL, 0.f);
	TestEqual(TEXT("Tank cuts survive failure"), M.State().TankDieselL + M.State().TotalGasL + M.State().TotalTarL, Made);
	TestEqual(TEXT("One Fouled"), Run.Count(ERefineryEvent::Fouled), 1);
	TestEqual(TEXT("One Failed"), Run.Count(ERefineryEvent::Failed), 1);
	TestEqual(TEXT("No completed batch on failure"), Run.Count(ERefineryEvent::BatchDone), 0);
	TestTrue(TEXT("Failed hopper still accepts"), M.LoadHopper(StillId, TEXT("C2")));
	M.SetKnob(StillId, ERefineryKnob::Feed, 18); Run.Advance(M, 40);
	TestEqual(TEXT("Failed still adds no dry or hot coke"), M.FindStill(StillId)->Coke, 100.f);
	TestEqual(TEXT("Failed feed neither spills nor consumes"), M.State().Spills, 0);
	TestEqual(TEXT("Failed queue stays"), M.FindStill(StillId)->Hopper.Num(), 1);
	TestFalse(TEXT("Scrape cannot substitute for repair"), M.Scrape(StillId));
	TestFalse(TEXT("Unknown repair"), M.Repair(TEXT("Missing")));
	const FStillState Before = *M.FindStill(StillId); TestTrue(TEXT("Repair accepted"), M.Repair(StillId));
	FStillState Expected = Before; Expected.Status = EStillStatus::Idle; Expected.Coke = 40.f;
	Expected.bFouled = false; Expected.Condition = 100.f;
	TestTrue(TEXT("Repair only resets status coke mirror and condition"), *M.FindStill(StillId) == Expected);
	TestFalse(TEXT("Cannot repair twice"), M.Repair(StillId));
	M.SetKnob(StillId, ERefineryKnob::Heat, 12); M.SetKnob(StillId, ERefineryKnob::Feed, 0); Run.Advance(M, 268);
	TestEqual(TEXT("Repair re-arms Fouled"), Run.Count(ERefineryEvent::Fouled), 2);
	TestEqual(TEXT("Queued charge after repair"), M.FindStill(StillId)->ChargeL, 1000.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryScrapeTest, "Airlock.Refinery.Model.IdleScrapeAllowsHopperAndVerbsRejectWithoutMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryScrapeTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(1)); M.SetKnob(StillId, ERefineryKnob::Heat, 12);
	FRun Run; Run.Advance(M, 40); M.LoadHopper(StillId, TEXT("C1"));
	const FStillState Before = *M.FindStill(StillId);
	TestTrue(TEXT("Loaded hopper is still scrapeable before next tick"), M.Scrape(StillId));
	FStillState Expected = Before; Expected.Coke = 0.f; Expected.bFouled = false;
	TestTrue(TEXT("Scrape changes only coke and mirror"), *M.FindStill(StillId) == Expected);
	Run.Advance(M, 1);
	const FRefineryState Loaded = M.State();
	TestFalse(TEXT("Loaded separator cannot scrape"), M.Scrape(StillId));
	TestFalse(TEXT("Unknown scrape"), M.Scrape(TEXT("Missing")));
	TestFalse(TEXT("Running cannot repair"), M.Repair(StillId));
	TestFalse(TEXT("Running cannot purge"), M.Purge(StillId));
	TestFalse(TEXT("Running cannot reset"), M.ClearPop(StillId));
	TestTrue(TEXT("Rejections change nothing"), M.State() == Loaded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryTallyExampleTest, "Airlock.Refinery.Model.ExactWorkedTallyAndConstResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryTallyExampleTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(12)); auto& S = Fixture(M);
	S.Waiting = { Manifest(12)[10], Manifest(12)[11] };
	S.CasksRefined = 9; S.CasksLost = 1; S.TankDieselL = 6420.f; S.TotalGasL = 900.f; S.TotalTarL = 1100.f;
	S.Spills = 1; S.SpilledL = 200.f;
	const FRefineryState Before = M.State(); const FFloorResult R = M.Tally(1, 100.f);
	TestEqual(TEXT("Delivered"), R.CasksTotal, 12); TestEqual(TEXT("Refined"), R.CasksRefined, 9);
	TestEqual(TEXT("Unrefined"), R.CasksUnrefined, 2); TestEqual(TEXT("Lost"), R.CasksLost, 1);
	TestEqual(TEXT("Diesel"), R.DieselL, 6420.f); TestEqual(TEXT("Gas"), R.GasL, 900.f);
	TestEqual(TEXT("Tar"), R.TarL, 1100.f); TestEqual(TEXT("Spills"), R.Spills, 1);
	TestEqual(TEXT("Pops"), R.Pops, 0); TestEqual(TEXT("Flashes"), R.Flashes, 0);
	TestEqual(TEXT("Criticals"), R.Criticals, 1); TestEqual(TEXT("Refuels ship first"), R.RefuelUnits, 100.f);
	TestEqual(TEXT("Surplus"), R.SurplusL, 5420.f); TestEqual(TEXT("Base tonnes floored"), R.BaseFuelTonnes, 54);
	TestEqual(TEXT("3210 + 80 - 110 - 200"), R.Pay, 2980);
	TestEqual(TEXT("Exact worked line"), R.Summary(), FString(TEXT("floor: 9/12 casks refined, 6420 L diesel, 1100 L tar, 1 spill, 0 pops, 1 critical, pay 2980, fuel +54 t")));
	TestTrue(TEXT("Tally records nothing"), M.State() == Before);
	TestEqual(TEXT("Repeated tally unchanged"), M.Tally(1, 100.f).Summary(), R.Summary());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryPartialTallyTest, "Airlock.Refinery.Model.PartialCaskLostQueuedCrudeAndFuelBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryPartialTallyTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(3)); Recipe(M); M.LoadHopper(StillId, TEXT("C1")); M.LoadHopper(StillId, TEXT("C2"));
	FRun Run; Run.Advance(M, 200); const auto Before = M.State();
	const FFloorResult Partial = M.Tally(0, 0.f);
	TestEqual(TEXT("One waiting plus one hopper"), Partial.CasksUnrefined, 2);
	TestEqual(TEXT("Mid batch is lost at horn"), Partial.CasksLost, 1);
	TestEqual(TEXT("Not refined early"), Partial.CasksRefined, 0);
	TestEqual(TEXT("All delivered casks classified"), Partial.CasksRefined + Partial.CasksUnrefined + Partial.CasksLost, Partial.CasksTotal);
	TestEqual(TEXT("Product already in tank survives horn"), Partial.DieselL, M.State().TankDieselL);
	TestTrue(TEXT("Horn tally leaves batch running"), M.State() == Before);
	for (float Missing : { -10.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{
		const FFloorResult R = M.Tally(-1, Missing);
		TestEqual(TEXT("Bad missing reads zero"), R.RefuelUnits, 0.f);
		TestEqual(TEXT("All product surplus"), R.SurplusL, M.State().TankDieselL);
		TestEqual(TEXT("Negative criticals do not award a bonus"), R.Criticals, 0);
	}
	const FFloorResult EmptyTank = M.Tally(100, 10000.f);
	TestEqual(TEXT("Ship takes all available units"), EmptyTank.RefuelUnits, M.State().TankDieselL * 0.1f);
	TestEqual(TEXT("No surplus after emptying tank"), EmptyTank.SurplusL, 0.f, 0.001f);
	TestEqual(TEXT("No negative fuel from rounding"), EmptyTank.BaseFuelTonnes, 0);
	TestEqual(TEXT("Pay cannot go negative"), EmptyTank.Pay, 0);
	FRefineryModel Empty; Empty.Build(0, {});
	TestEqual(TEXT("Empty floor pay"), Empty.Tally(0, 100.f).Pay, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryRoundingTest, "Airlock.Refinery.Model.TallyRoundingAndFlooredReadouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryRoundingTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(2, {}); FRefineryState& S = Fixture(M);
	S.CasksDelivered = 12; S.CasksRefined = 9; S.CasksLost = 1;
	S.Waiting = Manifest(2); S.TankDieselL = 6420.9f; S.TotalGasL = 900.9f; S.TotalTarL = 1100.9f; S.SpilledL = 200.9f;
	FStillState& Still = S.Stills[1]; Still.Status = EStillStatus::Running;
	Still.TempK = 184.9f; Still.PressureBar = 1.64f; Still.HeatSteps = 8; Still.FeedSteps = 10;
	Still.Calibration = 0.829f; Still.Coke = 12.9f; Still.Condition = 100.f;
	Still.CurrentCask = TEXT("C4"); Still.ChargeL = 420.9f;
	TestEqual(TEXT("Constructed gauge"), M.StillReadout(TEXT("Still_2")), FString(TEXT("T 184 K  P 1.6 bar  feed 50%  cal 82%")));
	TestEqual(TEXT("Unknown gauge empty"), M.StillReadout(TEXT("Missing")), FString());
	TestEqual(TEXT("Constructed still and tank summary"), M.Summary(), FString(
		TEXT("Still_1 Idle T=92 P=1.0 heat=0% feed=0% vent=55% cal=100% coke=0 cond=100 cask=none charge=0\n")
		TEXT("Still_2 Running T=184 P=1.6 heat=40% feed=50% vent=55% cal=82% coke=12 cond=100 cask=C4 charge=420\n")
		TEXT("Tank diesel=6420 gas=900 tar=1100 spilled=200 refined=9 lost=1 waiting=2")));
	for (int32 Step = 0; Step <= 20; ++Step)
	{
		M.SetKnob(TEXT("Still_2"), ERefineryKnob::Feed, Step);
		TestTrue(TEXT("Every five percent prints exactly"), M.StillReadout(TEXT("Still_2")).Contains(FString::Printf(TEXT("feed %d%%"), Step * 5)));
	}
	S.TankDieselL = 1001.f; S.TotalTarL = 199.99f; S.Waiting.Reset(); Still.CurrentCask = NAME_None;
	const FFloorResult R = M.Tally(0, 0.f);
	TestEqual(TEXT("Diesel rounds 500.5 up; one complete tar hundred costs ten"), R.Pay, 491);
	TestEqual(TEXT("Only whole surplus tonnes"), R.BaseFuelTonnes, 10);
	FFloorResult Display; Display.CasksTotal = 2; Display.CasksRefined = 1; Display.DieselL = 850.9f;
	Display.TarL = 100.9f; Display.Spills = 2; Display.Pops = 1; Display.Criticals = 2;
	TestEqual(TEXT("Constructed plural and floored result"), Display.Summary(), FString(TEXT("floor: 1/2 casks refined, 850 L diesel, 100 L tar, 2 spills, 1 pop, 2 criticals, pay 0, fuel +0 t")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryEventOrderTest, "Airlock.Refinery.Model.EventOrderAndDestructiveLossAccounting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryEventOrderTest::RunTest(const FString& Parameters)
{
	FRefineryModel M; M.Build(1, Manifest(1)); Recipe(M, 8, 18, 0); M.LoadHopper(StillId, TEXT("C1"));
	FRun Run; Run.Advance(M, 19);
	// Place the pressure just below relief so this slice floods first, then pops.
	Fixture(M).Stills[0].PressureBar = 2.9999f;
	Run.Advance(M, 1);
	TestEqual(TEXT("Flood pop and relief spill"), Run.Events.Num(), 3);
	TestEqual(TEXT("Flood first"), Run.Events[0].Type, ERefineryEvent::Spill);
	TestEqual(TEXT("Pop second"), Run.Events[1].Type, ERefineryEvent::Pop);
	TestEqual(TEXT("Relief spill third"), Run.Events[2].Type, ERefineryEvent::Spill);
	TestEqual(TEXT("Flood exactly 200"), Run.Events[0].Litres, 200.f);
	TestEqual(TEXT("Relief exactly 300"), Run.Events[2].Litres, 300.f);
	TestEqual(TEXT("One lost cask for both causes"), M.State().CasksLost, 1);
	TestEqual(TEXT("Two physical spills"), M.State().Spills, 2);
	TestTrue(TEXT("Diesel credited before pop"), M.State().TankDieselL > 0.f);
	const float Tank = M.State().TankDieselL;
	const float Accounted = Tank + M.State().TotalGasL + M.State().TotalTarL + M.State().SpilledL;
	// A destructive event discards unfed litres beyond its spill. No LostL field exists in the brief.
	TestEqual(TEXT("90 processed + 500 spilled; 410 discarded by pop"), Accounted, 590.f, 0.5f);
	Run.Advance(M, 40); TestEqual(TEXT("Pop never erases prior diesel"), M.State().TankDieselL, Tank);
	AddInfo(TEXT("Conservation excludes discarded charge: simultaneous flood/pop leaves 590 L tracked and 410 L lost; no LostL field is specified."));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryDeterminismTest, "Airlock.Refinery.Model.ScriptedTenMinuteDeterminismAndEventEquality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryDeterminismTest::RunTest(const FString& Parameters)
{
	FRefineryModel A, B; A.Build(3, Manifest(12)); B.Build(3, Manifest(12));
	const auto Inputs = [](FRefineryModel& M, int32 I)
	{
		if (I == 0)
		{
			for (int32 J = 1; J <= 3; ++J)
			{
				const FName Id(*FString::Printf(TEXT("Still_%d"), J));
				M.SetKnob(Id, ERefineryKnob::Heat, J == 1 ? 10 : J == 2 ? 16 : 18);
				M.SetKnob(Id, ERefineryKnob::Feed, J == 2 ? 18 : 10);
				M.SetKnob(Id, ERefineryKnob::Vent, J == 1 ? 2 : 13);
			}
		}
		if (I == 240)
		{
			for (int32 J = 0; J < 9; ++J)
			{
				M.LoadHopper(FName(*FString::Printf(TEXT("Still_%d"), J / 3 + 1)), FName(*FString::Printf(TEXT("C%d"), J + 1)));
			}
		}
		if (I == 400)
		{
			M.ClearPop(StillId); M.SetKnob(StillId, ERefineryKnob::Heat, 8); M.SetKnob(StillId, ERefineryKnob::Vent, 11);
			M.Purge(TEXT("Still_3")); M.SetKnob(TEXT("Still_3"), ERefineryKnob::Heat, 6);
		}
		if (I == 600) { M.SetKnob(TEXT("Still_2"), ERefineryKnob::Feed, 10); M.SetKnob(TEXT("Still_2"), ERefineryKnob::Heat, 8); }
		if (I == 1000) { M.LoadHopper(StillId, TEXT("C10")); M.LoadHopper(TEXT("Still_2"), TEXT("C11")); M.LoadHopper(TEXT("Still_3"), TEXT("C12")); }
		if (I % 360 == 0) { M.Calibrate(StillId); M.Calibrate(TEXT("Still_2")); M.Calibrate(TEXT("Still_3")); }
		if (I == 1600) { M.Repair(TEXT("Still_3")); M.Scrape(TEXT("Still_2")); }
	};
	int32 Pops = 0, Flashes = 0, Spills = 0, Done = 0;
	for (int32 I = 0; I < 2400; ++I)
	{
		Inputs(A, I); Inputs(B, I);
		const float Room = I >= 240 && I < 320 ? 45.f : 0.f;
		float PpmA, PpmB; TArray<FRefineryEvent> EA, EB;
		TestEqual(TEXT("Identical publication"), A.Tick(0.25f, Room, PpmA, EA), B.Tick(0.25f, Room, PpmB, EB));
		TestTrue(TEXT("Every field exactly deterministic"), A.State() == B.State());
		TestEqual(TEXT("Identical emitted ppm"), PpmA, PpmB); TestEqual(TEXT("Identical event count"), EA.Num(), EB.Num());
		for (int32 J = 0; J < FMath::Min(EA.Num(), EB.Num()); ++J)
		{
			TestTrue(TEXT("Every event field exactly equal"), SameEvent(EA[J], EB[J]));
			Pops += EA[J].Type == ERefineryEvent::Pop; Flashes += EA[J].Type == ERefineryEvent::Flash;
			Spills += EA[J].Type == ERefineryEvent::Spill; Done += EA[J].Type == ERefineryEvent::BatchDone;
		}
	}
	TestTrue(TEXT("Script exercises pops"), Pops > 0); TestTrue(TEXT("Script exercises flashes"), Flashes > 0);
	TestTrue(TEXT("Script exercises spills"), Spills > 0); TestTrue(TEXT("Script completes batches after recovery"), Done > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryHazardSubstepsTest, "Airlock.Refinery.Model.SubstepsPreserveHazardOrderAndRoomInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryHazardSubstepsTest::RunTest(const FString& Parameters)
{
	FRefineryModel A; A.Build(2, Manifest(2)); Recipe(A, 18, 10, 13);
	A.LoadHopper(StillId, TEXT("C1")); A.LoadHopper(TEXT("Still_2"), TEXT("C2"));
	Fixture(A).Stills[1].TempK = A.FindStill(StillId)->TempK;
	A.SetKnob(TEXT("Still_2"), ERefineryKnob::Heat, 18);
	A.SetKnob(TEXT("Still_2"), ERefineryKnob::Vent, 13);
	FRefineryModel B = A; TArray<FRefineryEvent> LargeEvents; float Ppm;
	A.Tick(1.f, 40.f, Ppm, LargeEvents); FRun Small; Small.Advance(B, 4, 40.f);
	TestTrue(TEXT("Hazard states equal across caller deltas"), A.State() == B.State());
	TestEqual(TEXT("Same hazard count"), LargeEvents.Num(), Small.Events.Num());
	for (int32 I = 0; I < FMath::Min(LargeEvents.Num(), Small.Events.Num()); ++I)
	{ TestTrue(TEXT("Hazard order and values equal"), SameEvent(LargeEvents[I], Small.Events[I])); }
	TestEqual(TEXT("Accumulated ppm"), Ppm, Small.Ppm, 0.001f);
	TestEqual(TEXT("Both flash in still order"), LargeEvents.Num(), 2);
	TestEqual(TEXT("First still first"), LargeEvents[0].StillId, StillId);
	TestEqual(TEXT("Second still second"), LargeEvents[1].StillId, FName(TEXT("Still_2")));
	A.Rebuild(); Recipe(A, 18, 10, 13); A.LoadHopper(StillId, TEXT("C1"));
	FRun Below; Below.Advance(A, 4, 39.99f);
	TestTrue(TEXT("Room sources are produced"), Below.Ppm > 0.f);
	TestEqual(TEXT("Output never feeds back into input to cause flash"), A.State().Flashes, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRefineryCalibrationCutTest, "Airlock.Refinery.Model.CalibrationLossActuallyBecomesTar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRefineryCalibrationCutTest::RunTest(const FString& Parameters)
{
	FRefineryModel Clean; Clean.Build(1, Manifest(1)); Recipe(Clean);
	FRefineryModel Drifted = Clean; Fixture(Drifted).Stills[0].Calibration = 0.5f;
	Clean.LoadHopper(StillId, TEXT("C1")); Drifted.LoadHopper(StillId, TEXT("C1"));
	FRun CleanRun, DriftedRun; CleanRun.Advance(Clean, 400); DriftedRun.Advance(Drifted, 400);
	const float LostDiesel = Clean.State().TankDieselL - Drifted.State().TankDieselL;
	TestEqual(TEXT("Half calibration loses a quarter of the undrifted bell cut"), LostDiesel, 207.95f, 0.5f);
	TestEqual(TEXT("Gas formula does not depend on calibration"), Drifted.State().TotalGasL, Clean.State().TotalGasL);
	TestEqual(TEXT("Lost diesel instead becomes tar by residual formula"), Drifted.State().TotalTarL - Clean.State().TotalTarL, LostDiesel, 0.01f);
	TestEqual(TEXT("Calibration does not raise gas ppm with this formula"), DriftedRun.Ppm, CleanRun.Ppm);
	AddInfo(TEXT("Starting calibration 50% loses 207.95 L diesel over the heavy recipe; all 207.95 L goes to tar, not gas, under the brief's formulas."));
	return true;
}
#endif
