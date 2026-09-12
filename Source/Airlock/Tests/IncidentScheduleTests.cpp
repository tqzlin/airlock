// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Voyage/IncidentSchedule.h"
#include "Ship/ShipLayouts.h"
#include "Ship/CargoLayouts.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FVoyageSpec IncidentVoyage(int32 Seed = 17, float Duration = 1200.f)
{
	FVoyageSpec Spec;
	Spec.Seed = Seed; Spec.DurationSeconds = Duration; Spec.Captain = TEXT("TestCaptain");
	return Spec;
}
bool SameIncidents(const TArray<FIncidentSpec>& A, const TArray<FIncidentSpec>& B)
{
	if (A.Num() != B.Num()) { return false; }
	for (int32 I = 0; I < A.Num(); ++I)
	{
		if (A[I].AtSeconds != B[I].AtSeconds || A[I].Type != B[I].Type || A[I].TargetId != B[I].TargetId ||
			A[I].Magnitude != B[I].Magnitude || !A[I].CaptainLine.EqualTo(B[I].CaptainLine)) { return false; }
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentDeterminismTest, "Airlock.Incidents.ScheduleDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentDeterminismTest::RunTest(const FString& Parameters)
{
	const FShipLayout& Layout = *ShipLayouts::Find(TEXT("Hauler_Mk1"));
	for (int32 Crew = 1; Crew <= 9; ++Crew)
	{
		const auto A = FIncidentSchedule::Build(IncidentVoyage(), Layout, Crew);
		TestTrue(TEXT("Same seed and crew reproduce all fields"), SameIncidents(A, FIncidentSchedule::Build(IncidentVoyage(), Layout, Crew)));
		TestFalse(TEXT("Different seed changes schedule"), SameIncidents(A, FIncidentSchedule::Build(IncidentVoyage(18), Layout, Crew)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentCountTest, "Airlock.Incidents.CountByCrew",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentCountTest::RunTest(const FString& Parameters)
{
	const FShipLayout& Layout = *ShipLayouts::Find(TEXT("Hauler_Mk1"));
	for (const int32 Crew : { MIN_int32, -1, 0, 1, 2, 3, 4, 9, MAX_int32 })
	{
		const int32 Expected = Crew <= 1 ? 4 : (Crew == 2 ? 5 : (Crew == 3 ? 6 : (Crew == 4 ? 7 : 8)));
		TestEqual(FString::Printf(TEXT("Count for crew %d"), Crew), FIncidentSchedule::Build(IncidentVoyage(), Layout, Crew).Num(), Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentWindowTest, "Airlock.Incidents.OrderWindowAndClimax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentWindowTest::RunTest(const FString& Parameters)
{
	const FShipLayout& Layout = *ShipLayouts::Find(TEXT("Hauler_Mk1"));
	for (int32 Seed = 0; Seed < 100; ++Seed)
	{
		for (const int32 Crew : { 1, 4, 9 })
		{
			for (const float Duration : { 900.f, 1200.f, 1800.f })
			{
			const auto Entries = FIncidentSchedule::Build(IncidentVoyage(Seed, Duration), Layout, Crew);
			if (!TestTrue(TEXT("Schedule exists"), Entries.Num() >= 4)) { return false; }
			TestEqual(TEXT("Teaching incident at calm deadline"), Entries[0].AtSeconds, FIncidentSchedule::CalmSeconds);
			for (int32 I = 0; I < Entries.Num(); ++I)
			{
				TestTrue(TEXT("Inside calm/docking window"), Entries[I].AtSeconds >= FIncidentSchedule::CalmSeconds && Entries[I].AtSeconds <= Duration - FIncidentSchedule::DockSeconds);
				if (I > 0)
				{
					const float Gap = Entries[I].AtSeconds - Entries[I - 1].AtSeconds;
					TestTrue(TEXT("Ordered with minimum gap except climax"), Gap + KINDA_SMALL_NUMBER >= (I == Entries.Num() - 1 ? FIncidentSchedule::ClimaxMinGapSeconds : FIncidentSchedule::MinGapSeconds));
				}
			}
			const float PairGap = Entries.Last().AtSeconds - Entries[Entries.Num() - 2].AtSeconds;
			TestTrue(TEXT("Climax gap at most 20 seconds"), PairGap <= FIncidentSchedule::ClimaxMaxGapSeconds + KINDA_SMALL_NUMBER);
			TestTrue(TEXT("Both climax entries at 78..85 percent"), Entries[Entries.Num() - 2].AtSeconds >= Duration * FIncidentSchedule::ClimaxStartFraction && Entries.Last().AtSeconds <= Duration * FIncidentSchedule::ClimaxEndFraction);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentTargetsTest, "Airlock.Incidents.TeachingFaultTargetsAndCaptain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentTargetsTest::RunTest(const FString& Parameters)
{
	const FShipLayout& Layout = *ShipLayouts::Find(TEXT("Hauler_Mk1"));
	TSet<EIncidentType> SeenTypes;
	for (int32 Seed = 0; Seed < 100; ++Seed)
	{
		const auto Entries = FIncidentSchedule::Build(IncidentVoyage(Seed), Layout, 4);
		if (!TestTrue(TEXT("Schedule exists"), Entries.Num() == 7)) { return false; }
		TestTrue(TEXT("First incident is a leak"), Entries[0].Type == EIncidentType::CoolantLeak);
		FName Previous;
		for (const FIncidentSpec& Entry : Entries)
		{
			SeenTypes.Add(Entry.Type);
			TestTrue(TEXT("Captain name in every line"), Entry.CaptainLine.ToString().Contains(TEXT("TestCaptain")));
			TestTrue(TEXT("No consecutive target repeats"), Entry.TargetId != Previous);
			Previous = Entry.TargetId;
			if (Entry.Type == EIncidentType::ModuleDamage)
			{
				const auto* Module = Layout.Modules.FindByPredicate([&](const FShipModuleSpec& M) { return M.Id == Entry.TargetId; });
				TestTrue(TEXT("Damage targets one of four eligible modules"), Module && (Module->Type == EShipModuleType::PowerCore || Module->Type == EShipModuleType::LifeSupport || Module->Type == EShipModuleType::CargoClamps || Module->Type == EShipModuleType::Engines));
				TestTrue(TEXT("Damage in 30..50"), Entry.Magnitude >= FIncidentSchedule::MinDamage && Entry.Magnitude <= FIncidentSchedule::MaxDamage);
			}
			else
			{
				const auto* Line = Layout.Lines.FindByPredicate([&](const FShipLineSpec& L) { return L.Id == Entry.TargetId; });
				TestTrue(TEXT("Fault target exists and has matching kind"), Line && Line->Kind == (Entry.Type == EIncidentType::CoolantLeak ? EShipLineKind::Coolant : EShipLineKind::Power));
				TestEqual(TEXT("Fault magnitude unused"), Entry.Magnitude, 0.f);
			}
		}
	}
	TestEqual(TEXT("Seed sweep includes every incident type"), SeenTypes.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentShortTest, "Airlock.Incidents.ShortAndInvalidWindows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentShortTest::RunTest(const FString& Parameters)
{
	const FShipLayout& Layout = *ShipLayouts::Find(TEXT("Hauler_Mk1"));
	for (const float Duration : { 0.f, 1.f, 30.f, 120.f, 239.f, 240.f, 300.f, 600.f })
	{
		const bool bShort = Duration < FIncidentSchedule::ShortWindowThreshold;
		const float Start = bShort ? Duration * .25f : FIncidentSchedule::CalmSeconds;
		const float End = bShort ? Duration * .75f : Duration - FIncidentSchedule::DockSeconds;
		for (const int32 Crew : { 1, 4, 9 })
		{
			const auto Entries = FIncidentSchedule::Build(IncidentVoyage(7, Duration), Layout, Crew);
			TestEqual(TEXT("Short duration retains count"), Entries.Num(), Crew == 1 ? 4 : (Crew == 4 ? 7 : 8));
			float Previous = Start;
			for (const FIncidentSpec& Entry : Entries)
			{
				TestTrue(TEXT("Finite, ordered, in window"), FMath::IsFinite(Entry.AtSeconds) && Entry.AtSeconds >= Previous && Entry.AtSeconds <= End);
				Previous = Entry.AtSeconds;
			}
		}
	}
	for (const float Duration : { -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
	{
		TestTrue(TEXT("Invalid duration yields no timers"), FIncidentSchedule::Build(IncidentVoyage(7, Duration), Layout, 1).IsEmpty());
	}
	TestTrue(TEXT("Empty layout has no valid schedule"), FIncidentSchedule::Build(IncidentVoyage(), FShipLayout(), 1).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIncidentCursorTest, "Airlock.Incidents.ClockMovesFireOnceInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FIncidentCursorTest::RunTest(const FString& Parameters)
{
	const auto Entries = FIncidentSchedule::Build(IncidentVoyage(), *ShipLayouts::Find(TEXT("Hauler_Mk1")), 1);
	TestEqual(TEXT("Nothing during calm period"), FIncidentSchedule::FindNextUnfired(Entries, 0, 149.9), 0);
	int32 Next = FIncidentSchedule::FindNextUnfired(Entries, 0, 160);
	TestEqual(TEXT("First clock jump catches teaching leak"), Next, 1);
	TestEqual(TEXT("Backward jump does not replay"), FIncidentSchedule::FindNextUnfired(Entries, Next, 0), Next);
	TestEqual(TEXT("Repeated forward jump does not replay"), FIncidentSchedule::FindNextUnfired(Entries, Next, 160), Next);
	Next = FIncidentSchedule::FindNextUnfired(Entries, Next, 1130);
	TestEqual(TEXT("70 seconds left catches all remaining in one ordered range"), Next, Entries.Num());
	TestEqual(TEXT("Exhausted schedule stays exhausted"), FIncidentSchedule::FindNextUnfired(Entries, Next, 1200), Next);
	const auto Zero = FIncidentSchedule::Build(IncidentVoyage(7, 0), *ShipLayouts::Find(TEXT("Hauler_Mk1")), 1);
	TestEqual(TEXT("Equal-time entries all consumed"), FIncidentSchedule::FindNextUnfired(Zero, 0, 0), Zero.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoIncidentScheduleTest, "Airlock.Cargo.ShiftScheduleDeterminismAndClimax",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoIncidentScheduleTest::RunTest(const FString& Parameters)
{
	const auto& Ship = *ShipLayouts::Find(TEXT("Hauler_Mk1"));
	const auto* Cargo = CargoLayouts::Find(TEXT("Hauler_Mk1"));
	const auto Targets = FIncidentSchedule::Targets(Ship, EIncidentType::CargoShift, Cargo);
	TestEqual(TEXT("Only three clamps eligible"), Targets.Num(), 3);
	TestFalse(TEXT("Never rack"), Targets.Contains(TEXT("ColdRack")));
	bool bSawMiddleShift = false;
	TSet<int32> ClimaxPositions;
	for (int32 Seed = 0; Seed < 100; ++Seed)
	{
		for (int32 Crew : { 1, 4, 9 })
		{
			const auto Spec = IncidentVoyage(Seed);
			const auto Plain = FIncidentSchedule::Build(Spec, Ship, Crew);
			const auto Entries = FIncidentSchedule::Build(Spec, Ship, Crew, Cargo);
			if (!TestEqual(TEXT("Cargo replaces, never adds"), Entries.Num(), Plain.Num())) { return false; }
			TestTrue(TEXT("Deterministic cargo schedule"), SameIncidents(Entries, FIncidentSchedule::Build(Spec, Ship, Crew, Cargo)));
			TestTrue(TEXT("Teaching leak stays first"), Entries[0].Type == EIncidentType::CoolantLeak);
			int32 PairShifts = 0;
			FName Previous;
			for (int32 I = 0; I < Entries.Num(); ++I)
			{
				const auto& Entry = Entries[I];
				TestTrue(TEXT("No consecutive target repeat"), Entry.TargetId != Previous);
				Previous = Entry.TargetId;
				if (Entry.Type != EIncidentType::CargoShift) { continue; }
				TestTrue(TEXT("No shift before forty percent"), Entry.AtSeconds >= Spec.DurationSeconds * FIncidentSchedule::CargoShiftMinFraction);
				TestTrue(TEXT("Clamp target"), Targets.Contains(Entry.TargetId));
				TestEqual(TEXT("No magnitude"), Entry.Magnitude, 0.f);
				TestTrue(TEXT("Captain names shifted clamp"), Entry.CaptainLine.ToString().Contains(Entry.TargetId.ToString()) && Entry.CaptainLine.ToString().Contains(TEXT("Captain TestCaptain here. Hold on - cargo's shifted")));
				if (I >= Entries.Num() - 2) { ++PairShifts; ClimaxPositions.Add(I - (Entries.Num() - 2)); }
				else { bSawMiddleShift = true; }
			}
			TestEqual(TEXT("Exactly one climax shift, other normal"), PairShifts, 1);
			for (const auto& Entry : Plain) { TestTrue(TEXT("No cargo path has no shifts"), Entry.Type != EIncidentType::CargoShift); }
		}
	}
	TestTrue(TEXT("Middle shift possible"), bSawMiddleShift);
	TestEqual(TEXT("Extra draw can select either climax position"), ClimaxPositions.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoIncidentSingleClampTest, "Airlock.Cargo.SingleClampAndAbsentCargoSchedules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoIncidentSingleClampTest::RunTest(const FString& Parameters)
{
	const auto& Ship = *ShipLayouts::Find(TEXT("Hauler_Mk1"));
	FCargoLayout Single = *CargoLayouts::Find(TEXT("Hauler_Mk1"));
	Single.Slots.RemoveAll([](const auto& S) { return !S.bRack && S.Id != TEXT("Clamp_1"); });
	FCargoLayout NoClamps = Single;
	NoClamps.Slots.RemoveAll([](const auto& S) { return !S.bRack; });
	for (int32 Seed = 0; Seed < 100; ++Seed)
	{
		for (float Duration : { 0.f, 30.f, 239.f, 240.f, 600.f, 1200.f })
		{
			const auto Spec = IncidentVoyage(Seed, Duration);
			const auto Plain = FIncidentSchedule::Build(Spec, Ship, 9);
			TestTrue(TEXT("Rack-only layout consumes no extra random draws"), SameIncidents(Plain, FIncidentSchedule::Build(Spec, Ship, 9, &NoClamps)));
			const auto Entries = FIncidentSchedule::Build(Spec, Ship, 9, &Single);
			if (!TestEqual(TEXT("Single clamp retains count"), Entries.Num(), Plain.Num())) { return false; }
			int32 Shifts = 0;
			for (int32 I = 0; I < Entries.Num(); ++I)
			{
				if (Entries[I].Type == EIncidentType::CargoShift)
				{
					++Shifts;
					TestTrue(TEXT("Only climax, never middle"), I >= Entries.Num() - 2);
					TestEqual(TEXT("Only clamp"), Entries[I].TargetId, FName(TEXT("Clamp_1")));
				}
			}
			TestEqual(TEXT("One shift"), Shifts, 1);
		}
	}
	return true;
}
#endif
