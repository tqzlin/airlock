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
#endif
