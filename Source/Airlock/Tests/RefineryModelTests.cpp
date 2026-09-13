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
#endif
