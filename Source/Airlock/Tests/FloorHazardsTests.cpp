// Copyright Airlock project. All Rights Reserved.
// M5-4a: the pure spill model - radius, merging, evaporation into the room, the squeegee, containment, determinism.
#include "Misc/AutomationTest.h"
#include "Ship/FloorHazards.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVector Still2Point(-5400, 1670, -65);
	const FVector Still1Point(-5800, 1670, -65);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardRadiusTest, "Airlock.Floor.Hazards.RadiusMergeIdsAndRejections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardRadiusTest::RunTest(const FString& Parameters)
{
	FFloorHazards M; M.Reset();
	TestEqual(TEXT("Dry radius"), M.RadiusFor(0.f), 0.f);
	TestEqual(TEXT("A flood's 200 L"), M.RadiusFor(200.f), 200.f);
	TestEqual(TEXT("A pop's 300 L caps"), M.RadiusFor(300.f), 250.f);
	TestEqual(TEXT("No spills"), M.Summary(), FString(TEXT("No spills")));
	TestEqual(TEXT("Zero litres rejected"), M.Spill(TEXT("Still_2"), Still2Point, 0.f), FName(NAME_None));
	TestEqual(TEXT("Negative rejected"), M.Spill(TEXT("Still_2"), Still2Point, -5.f), FName(NAME_None));
	TestEqual(TEXT("NaN rejected"), M.Spill(TEXT("Still_2"), Still2Point, std::numeric_limits<float>::quiet_NaN()), FName(NAME_None));
	TestTrue(TEXT("Rejections change nothing"), M.State() == FFloorHazardState());
	const FName S1 = M.Spill(TEXT("Still_2"), Still2Point, 200.f);
	TestEqual(TEXT("First id"), S1, FName(TEXT("S1")));
	TestEqual(TEXT("One puddle"), M.State().Spills.Num(), 1);
	TestEqual(TEXT("Radius 200"), M.Find(S1)->RadiusCm, 200.f);
	// A second flood at the same still lands inside the merge distance: the litres add, the centre stays, the radius caps.
	const FName Again = M.Spill(TEXT("Still_2"), Still2Point + FVector(100, -50, 0), 200.f);
	TestEqual(TEXT("Merged into S1"), Again, S1);
	TestEqual(TEXT("Still one puddle"), M.State().Spills.Num(), 1);
	TestEqual(TEXT("400 L"), M.Find(S1)->Litres, 400.f);
	TestEqual(TEXT("Capped radius"), M.Find(S1)->RadiusCm, 250.f);
	TestTrue(TEXT("Centre unchanged"), M.Find(S1)->Location.Equals(Still2Point));
	// The next still's point is 400 cm away: its own puddle, its own id; a spill on a platform above is not merged either.
	const FName S2 = M.Spill(TEXT("Still_1"), Still1Point, 300.f);
	TestEqual(TEXT("Second id"), S2, FName(TEXT("S2")));
	const FName S3 = M.Spill(TEXT("Still_2"), Still2Point + FVector(0, 0, 87), 100.f);
	TestEqual(TEXT("A platform spill is its own"), S3, FName(TEXT("S3")));
	TestEqual(TEXT("Three puddles"), M.State().Spills.Num(), 3);
	TestEqual(TEXT("Four events"), M.State().SpillsTotal, 4);
	TestEqual(TEXT("800 L spilled"), M.State().LitresSpilled, 800.f);
	TestEqual(TEXT("Summary"), M.Summary(), FString(TEXT("Spills 3: S1 by Still_2 400 L r=250; S2 by Still_1 300 L r=250; S3 by Still_2 100 L r=125")));
	M.Reset();
	TestTrue(TEXT("Reset clears"), M.State() == FFloorHazardState());
	TestEqual(TEXT("Ids restart"), M.Spill(TEXT("Still_3"), Still2Point, 10.f), FName(TEXT("S1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardEvaporationTest, "Airlock.Floor.Hazards.EvaporationFumesAndPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardEvaporationTest::RunTest(const FString& Parameters)
{
	FFloorHazards M; const FFloorHazardRates R;
	float Ppm = 0.f, Total = 0.f;
	TestFalse(TEXT("Nothing to tick"), M.Tick(1.f, Ppm)); TestEqual(TEXT("Dry deck fumes nothing"), Ppm, 0.f);
	const FName S1 = M.Spill(TEXT("Still_2"), Still2Point, 200.f);
	TestFalse(TEXT("Invalid delta"), M.Tick(-1.f, Ppm)); TestFalse(TEXT("NaN delta"), M.Tick(std::numeric_limits<float>::quiet_NaN(), Ppm));
	TestEqual(TEXT("Untouched"), M.Find(S1)->Litres, 200.f);
	// A litre a second: 199 s leave a litre, every second a whole litre less (published), the room gains 0.05 per litre.
	for (int32 I = 0; I < 199; ++I)
	{
		TestTrue(TEXT("Each second publishes"), M.Tick(1.f, Ppm)); Total += Ppm;
	}
	TestEqual(TEXT("One litre left"), M.Find(S1)->Litres, 1.f, 0.001f);
	TestEqual(TEXT("Radius follows"), M.Find(S1)->RadiusCm, R.RadiusBaseCm + R.RadiusPerLitre * 1.f, 0.01f);
	TestEqual(TEXT("Fumes so far"), Total, 199.f * R.PpmPerLitreEvaporated, 0.001f);
	TestTrue(TEXT("The last litre goes"), M.Tick(2.f, Ppm)); Total += Ppm;
	TestEqual(TEXT("Puddle gone"), M.State().Spills.Num(), 0);
	TestEqual(TEXT("Every litre fumed once"), Total, 200.f * R.PpmPerLitreEvaporated, 0.001f);
	TestNull(TEXT("Find on a gone puddle"), M.Find(S1));
	// A quarter-second tick and a one-second tick agree; a tick inside a whole litre publishes nothing.
	FFloorHazards A, B; A.Spill(TEXT("Still_1"), Still1Point, 50.f); B.Spill(TEXT("Still_1"), Still1Point, 50.f);
	float PpmA = 0.f, PpmB = 0.f, SumA = 0.f;
	for (int32 I = 0; I < 40; ++I) { A.Tick(0.25f, PpmA); SumA += PpmA; }
	B.Tick(10.f, PpmB);
	TestEqual(TEXT("Same litres"), A.State().Spills[0].Litres, B.State().Spills[0].Litres, 0.001f);
	TestEqual(TEXT("Same fumes"), SumA, PpmB, 0.001f);
	FFloorHazards C; C.Spill(TEXT("Still_1"), Still1Point, 50.f);
	TestTrue(TEXT("The first tenth crosses a whole litre (50 -> 49.9)"), C.Tick(0.1f, Ppm));
	TestFalse(TEXT("The next tenth stays inside the litre"), C.Tick(0.1f, Ppm));
	TestTrue(TEXT("The next whole litre publishes"), C.Tick(0.9f, Ppm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardSqueegeeTest, "Airlock.Floor.Hazards.SqueegeeContainmentAndStumbles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardSqueegeeTest::RunTest(const FString& Parameters)
{
	FFloorHazards M; const FFloorHazardRates R;
	const FName S1 = M.Spill(TEXT("Still_2"), Still2Point, 200.f);
	M.Spill(TEXT("Still_2"), Still2Point, 200.f);
	// Feet on the edge are in it; a step past the radius or a platform above are not.
	TestEqual(TEXT("At the centre"), M.Contains(Still2Point), S1);
	TestEqual(TEXT("On the rim"), M.Contains(Still2Point + FVector(249, 0, 0)), S1);
	TestEqual(TEXT("Past the rim"), M.Contains(Still2Point + FVector(251, 0, 0)), FName(NAME_None));
	TestEqual(TEXT("On the platform above"), M.Contains(Still2Point + FVector(0, 0, 87)), FName(NAME_None));
	TestEqual(TEXT("A pawn's feet a little off the deck"), M.Contains(Still2Point + FVector(0, 100, 20)), S1);
	TestEqual(TEXT("NaN feet"), M.Contains(FVector(std::numeric_limits<float>::quiet_NaN(), 0, 0)), FName(NAME_None));
	M.CountStumble(); M.CountStumble();
	TestEqual(TEXT("Stumbles counted"), M.State().Stumbles, 2);
	// One hold takes 300 of the 400 L; the second takes the rest and the puddle goes; an unknown id refuses.
	TestFalse(TEXT("Unknown puddle"), M.Squeegee(TEXT("S9")));
	TestTrue(TEXT("First hold"), M.Squeegee(S1));
	TestEqual(TEXT("100 L left"), M.Find(S1)->Litres, 100.f);
	TestEqual(TEXT("Radius shrinks"), M.Find(S1)->RadiusCm, R.RadiusBaseCm + R.RadiusPerLitre * 100.f);
	TestEqual(TEXT("Cleared so far"), M.State().LitresCleared, 300.f);
	TestTrue(TEXT("Second hold"), M.Squeegee(S1));
	TestEqual(TEXT("Gone"), M.State().Spills.Num(), 0);
	TestEqual(TEXT("All cleared"), M.State().LitresCleared, 400.f);
	TestFalse(TEXT("Nothing left to squeegee"), M.Squeegee(S1));
	TestEqual(TEXT("Dry deck"), M.Contains(Still2Point), FName(NAME_None));
	TestEqual(TEXT("Rates the owner applies"), R.SlowFactor, 0.6f); TestEqual(TEXT("Stumble every 4 s"), R.StumbleSeconds, 4.f);
	TestEqual(TEXT("Six-second hold"), R.SqueegeeSeconds, 6.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardDeterminismTest, "Airlock.Floor.Hazards.DeterministicOverAThousandTicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardDeterminismTest::RunTest(const FString& Parameters)
{
	FFloorHazards A, B;
	float PpmA = 0.f, PpmB = 0.f, SumA = 0.f, SumB = 0.f;
	for (int32 I = 0; I < 1000; ++I)
	{
		if (I % 100 == 0)
		{
			const FVector Point = (I / 100) % 2 == 0 ? Still2Point : Still1Point;
			A.Spill(TEXT("Still_2"), Point, 150.f); B.Spill(TEXT("Still_2"), Point, 150.f);
		}
		if (I == 450) { A.Squeegee(TEXT("S1")); B.Squeegee(TEXT("S1")); }
		A.Tick(0.25f, PpmA); B.Tick(0.25f, PpmB); SumA += PpmA; SumB += PpmB;
	}
	TestTrue(TEXT("Same state"), A.State() == B.State());
	TestEqual(TEXT("Same fumes"), SumA, SumB);
	TestEqual(TEXT("Ten events"), A.State().SpillsTotal, 10);
	TestEqual(TEXT("1500 L spilled"), A.State().LitresSpilled, 1500.f);
	TestTrue(TEXT("Something still on the deck"), A.State().Spills.Num() > 0);
	TestEqual(TEXT("Summary agrees"), A.Summary(), B.Summary());
	return true;
}
#endif
