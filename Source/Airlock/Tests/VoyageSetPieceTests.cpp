// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Voyage/VoyageSetPieces.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSetPieceScheduleTest, "Airlock.Fuel.SetPieceWindowSeedOrderAndCauses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSetPieceScheduleTest::RunTest(const FString& Parameters)
{
	TSet<FString> Causes;
	for (int32 Seed = 0; Seed < 100; ++Seed)
	{
		FVoyageSpec Spec; Spec.Seed = Seed; Spec.Captain = TEXT("TestCaptain");
		const auto A = FVoyageSetPieces::Build(Spec), B = FVoyageSetPieces::Build(Spec);
		TestEqual(TEXT("One entry"), A.Num(), 1); TestTrue(TEXT("Leak kind"), A[0].Kind == ESetPieceKind::FuelLeak);
		TestTrue(TEXT("40-55 percent window"), A[0].AtSeconds >= .4f * Spec.DurationSeconds && A[0].AtSeconds <= .55f * Spec.DurationSeconds);
		TestEqual(TEXT("Deterministic time"), A[0].AtSeconds, B[0].AtSeconds);
		TestEqual(TEXT("Deterministic line"), A[0].CaptainLine.ToString(), B[0].CaptainLine.ToString());
		FRandomStream Random(Seed ^ 0x5E7B1ECE);
		TestEqual(TEXT("Time is the first salted draw"), A[0].AtSeconds, Spec.DurationSeconds * Random.FRandRange(.4f,.55f));
		const TCHAR* Expected[] = { TEXT("Feed valve's failed"), TEXT("Fuel line's split"), TEXT("The Rig short-filled") };
		TestTrue(TEXT("Cause is the second draw"), A[0].CaptainLine.ToString().Contains(Expected[Random.RandRange(0,2)]));
		TestTrue(TEXT("Captain named"), A[0].CaptainLine.ToString().Contains(TEXT("Captain TestCaptain here.")));
		Causes.Add(A[0].CaptainLine.ToString());
	}
	TestEqual(TEXT("All three causes"), Causes.Num(), 3); return true;
}
#endif
