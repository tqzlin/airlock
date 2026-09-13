// Copyright Airlock project. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Voyage/VoyageSchedule.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockVoyageScheduleTest, "Airlock.Voyage.ScheduleBoundsAndCaptain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockVoyageScheduleTest::RunTest(const FString& Parameters)
{
	for (const EVoyageType Type : { EVoyageType::MilkRun, EVoyageType::Standard, EVoyageType::Priority })
	{
		for (const float Duration : { 0.f, 1.f, 30.f, 60.f, 120.f, 1200.f })
		{
			FVoyageSpec Spec;
			Spec.Type = Type;
			Spec.Captain = TEXT("TestCaptain");
			Spec.DurationSeconds = Duration;
			const TArray<FVoyageAnnouncement> Entries = FVoyageSchedule::Build(Spec);
			if (!TestTrue(TEXT("At least four lines, including short and zero-length voyages"), Entries.Num() >= 4)) { return false; }
			TestEqual(TEXT("Departure at zero"), Entries[0].AtSeconds, 0.f);
			TestEqual(TEXT("Docking at duration"), Entries.Last().AtSeconds, Duration);
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				TestTrue(TEXT("Every line names the captain"), Entries[Index].Line.ToString().Contains(Spec.Captain.ToString()));
				if (Index > 0) { TestTrue(TEXT("Schedule sorted"), Entries[Index - 1].AtSeconds <= Entries[Index].AtSeconds); }
			}

			// Compare translated text with the same lines from a normal-duration voyage, not English substrings.
			Spec.DurationSeconds = 1200.f;
			const TArray<FVoyageAnnouncement> Normal = FVoyageSchedule::Build(Spec);
			TestTrue(TEXT("Departure wins a tie at zero"), Entries[0].Line.EqualTo(Normal[0].Line));
			TestTrue(TEXT("Docking remains last even when every entry is at zero"), Entries.Last().Line.EqualTo(Normal.Last().Line));
			const int32 MinuteIndex = Entries.IndexOfByPredicate([&Normal](const FVoyageAnnouncement& Entry) { return Entry.Line.EqualTo(Normal[2].Line); });
			if (TestTrue(TEXT("One-minute line retained"), MinuteIndex != INDEX_NONE))
			{
				TestEqual(TEXT("One-minute time clamped to zero"), Entries[MinuteIndex].AtSeconds, FMath::Max(0.f, Duration - 60.f));
			}
			if (Duration == 0.f || Duration == 120.f)
			{
				TestTrue(TEXT("Mid-voyage precedes one-minute on a tie"), Entries[1].Line.EqualTo(Normal[1].Line));
				TestTrue(TEXT("One-minute follows mid-voyage on a tie"), Entries[2].Line.EqualTo(Normal[2].Line));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockVoyageTypeLinesTest, "Airlock.Voyage.ScheduleDistinctTypeLines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockVoyageTypeLinesTest::RunTest(const FString& Parameters)
{
	TSet<FString> Lines;
	for (const EVoyageType Type : { EVoyageType::MilkRun, EVoyageType::Standard, EVoyageType::Priority })
	{
		FVoyageSpec Spec;
		Spec.Type = Type;
		Spec.Captain = TEXT("SameCaptain"); // Distinct words must come from the type, not the captain.
		const TArray<FVoyageAnnouncement> Entries = FVoyageSchedule::Build(Spec);
		if (!TestTrue(TEXT("Mid-voyage line exists"), Entries.IsValidIndex(1))) { return false; }
		TestEqual(TEXT("Mid-voyage at halfway"), Entries[1].AtSeconds, Spec.DurationSeconds * 0.5f);
		Lines.Add(Entries[1].Line.ToString());
	}
	TestEqual(TEXT("Each voyage type has its own mid-voyage text"), Lines.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockVoyageClockMovesTest, "Airlock.Voyage.ClockMovesFireOnceInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockVoyageClockMovesTest::RunTest(const FString& Parameters)
{
	FVoyageSpec Spec;
	const TArray<FVoyageAnnouncement> Entries = FVoyageSchedule::Build(Spec);
	int32 Next = FVoyageSchedule::FindNextUnfiredEntry(Entries, 0, 0.0);
	TestEqual(TEXT("Departure fires on first tick"), Next, 1);
	Next = FVoyageSchedule::FindNextUnfiredEntry(Entries, Next, 1125.0);
	TestEqual(TEXT("Moving to 75 seconds remaining catches up mid-voyage"), Next, 2);
	TestEqual(TEXT("Moving back does not replay entries"), FVoyageSchedule::FindNextUnfiredEntry(Entries, Next, 0.0), Next);
	TestEqual(TEXT("One-minute line not early"), FVoyageSchedule::FindNextUnfiredEntry(Entries, Next, 1139.9), Next);
	Next = FVoyageSchedule::FindNextUnfiredEntry(Entries, Next, 1140.0);
	TestEqual(TEXT("One-minute line at deadline"), Next, 3);
	TestEqual(TEXT("Repeated clock adjustment does not repeat a line"), FVoyageSchedule::FindNextUnfiredEntry(Entries, Next, 1140.0), Next);
	Next = FVoyageSchedule::FindNextUnfiredEntry(Entries, Next, 1200.0);
	TestEqual(TEXT("Docking fires at zero remaining"), Next, Entries.Num());
	TestEqual(TEXT("All crossed entries form one ordered range"), FVoyageSchedule::FindNextUnfiredEntry(Entries, 0, 1200.0), Entries.Num());
	TestEqual(TEXT("No repeats after completion"), FVoyageSchedule::FindNextUnfiredEntry(Entries, Next, 1300.0), Next);
	Spec.DurationSeconds = 0.f;
	const TArray<FVoyageAnnouncement> Zero = FVoyageSchedule::Build(Spec);
	TestEqual(TEXT("All equal-time entries fire together without dropping any"), FVoyageSchedule::FindNextUnfiredEntry(Zero, 0, 0.0), Zero.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockVoyageClockClampTest, "Airlock.Voyage.ClockMoveClampsAndRejectsNonFinite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockVoyageClockClampTest::RunTest(const FString& Parameters)
{
	float Remaining = 75.f;
	TestTrue(TEXT("Accept 75 seconds"), FVoyageSchedule::TryClampRemainingSeconds(75.f, 1200.f, Remaining));
	TestEqual(TEXT("75 preserved"), Remaining, 75.f);
	TestTrue(TEXT("Accept negative, clamped"), FVoyageSchedule::TryClampRemainingSeconds(-1.f, 1200.f, Remaining));
	TestEqual(TEXT("Negative becomes zero"), Remaining, 0.f);
	TestTrue(TEXT("Accept overlong, clamped"), FVoyageSchedule::TryClampRemainingSeconds(2400.f, 1200.f, Remaining));
	TestEqual(TEXT("Overlong becomes full duration"), Remaining, 1200.f);
	TestTrue(TEXT("Zero-length voyage"), FVoyageSchedule::TryClampRemainingSeconds(75.f, 0.f, Remaining));
	TestEqual(TEXT("Zero-length stays zero"), Remaining, 0.f);
	for (const float Invalid : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() })
	{
		Remaining = 75.f;
		TestFalse(TEXT("Non-finite request ignored"), FVoyageSchedule::TryClampRemainingSeconds(Invalid, 1200.f, Remaining));
		TestEqual(TEXT("Rejected request leaves clock unchanged"), Remaining, 75.f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
