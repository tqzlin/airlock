// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Ship/ShipAtmosphere.h"
#include "Ship/CargoModel.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FCargoCrateState WarmAirTestCrate(FName Id = TEXT("C2"))
{
	FCargoCrateState C; C.Id = Id; C.bVolatile = true; C.Temperature = 45; return C;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirPuffTest, "Airlock.Atmosphere.PuffTimingAndEligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirPuffTest::RunTest(const FString& Parameters)
{
	FShipAtmosphere M;
	const FAtmosphereRates R;
	for (int32 I = 0; I < 100; ++I) { TestFalse(TEXT("Empty air stays unchanged"), M.Tick(10, {})); }
	TestEqual(TEXT("No sources stays zero"), M.GetPpm(), 0.f);
	TestTrue(TEXT("Default matches fresh model"), M.State() == FAtmosphereState());
	TArray<FCargoCrateState> C = { WarmAirTestCrate() };
	M.Tick(R.PuffSeconds, C);
	TestEqual(TEXT("2 ppm/s for 10 s minus 0.5 ppm/s = 15 ppm"), M.GetPpm(), R.PuffRate * R.PuffSeconds - R.ScrubNormal * R.PuffSeconds);
	M.Tick(R.PuffPeriodSeconds - R.PuffSeconds, C);
	TestEqual(TEXT("Off phase scrubs to 10 ppm"), M.GetPpm(), 10.f);
	TestEqual(TEXT("Off phase adds no source"), M.State().ActiveSources, 0);
	TestTrue(TEXT("FX remains through off phase"), M.IsPuffing(C[0].Id));
	M.Tick(1, C);
	TestEqual(TEXT("Second puff starts at 20 s"), M.GetPpm(), 11.5f);
	C[0].Temperature = 39; M.Tick(1, C);
	TestFalse(TEXT("Below warning drops cycle"), M.IsPuffing(C[0].Id));
	C[0].Temperature = 40; M.Tick(9, C);
	TestEqual(TEXT("Warming restarts at zero"), M.State().ActiveSources, 1);
	M.Tick(1, C); TestEqual(TEXT("Fresh cycle has full ten seconds"), M.State().ActiveSources, 1);
	M.Tick(1, C); TestEqual(TEXT("Then off"), M.State().ActiveSources, 0);
	for (const ECargoState State : { ECargoState::Loose, ECargoState::Carried, ECargoState::Secured, ECargoState::Racked })
	{
		M.Reset(); C[0].State = State; M.Tick(1, C);
		TestEqual(TEXT("Only rack prevents warm volatile puff"), M.IsPuffing(C[0].Id), State != ECargoState::Racked);
	}
	C[0].bVolatile = false; C[0].State = ECargoState::Loose; M.Reset();
	TestFalse(TEXT("Nonvolatile never puffs"), M.Tick(20, C));
	M.Reset(); C = { WarmAirTestCrate(TEXT("C4")), WarmAirTestCrate(TEXT("C2")) }; M.Tick(1, C);
	TestTrue(TEXT("FX list follows cargo order"), M.PuffingCrates() == TArray<FName>({ TEXT("C4"), TEXT("C2") }));
	M.Tick(1, {}); TestTrue(TEXT("Vanished ids removed"), M.PuffingCrates().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirRuptureTest, "Airlock.Atmosphere.RupturesOnceAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirRuptureTest::RunTest(const FString& Parameters)
{
	FShipAtmosphere M; M.SetScrubMode(EScrubMode::Off);
	TArray<FCargoCrateState> C = { WarmAirTestCrate() }; C[0].State = ECargoState::Vented;
	M.Tick(29.75f, C); M.Tick(1, C);
	TestEqual(TEXT("5 ppm/s for exactly 30 s = 150 ppm"), M.GetPpm(), FAtmosphereRates().RuptureRate * FAtmosphereRates().RuptureSeconds);
	M.Tick(1, C); TestFalse(TEXT("Finished rupture leaves FX list"), M.IsPuffing(C[0].Id));
	TestFalse(TEXT("Never bursts again"), M.Tick(1000, C));
	TestEqual(TEXT("Still 150"), M.GetPpm(), 150.f);
	M.Tick(1, {}); M.Tick(1, C);
	TestEqual(TEXT("Vanishing removed rupture clock"), M.GetPpm(), 155.f);
	M.Reset(); TestTrue(TEXT("Reset clears state"), M.State() == FAtmosphereState());
	M.Tick(1, C); TestEqual(TEXT("Already vented on reset bursts"), M.GetPpm(), 4.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirSinkTest, "Airlock.Atmosphere.ScrubModesBrownoutAndDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirSinkTest::RunTest(const FString& Parameters)
{
	const FAtmosphereRates R;
	for (const EScrubMode Mode : { EScrubMode::Off, EScrubMode::Normal, EScrubMode::Hazard })
	{
		for (const bool Brownout : { false, true })
		{
			FShipAtmosphere M; M.SetScrubMode(Mode); M.SetBrownout(Brownout); M.SetPpm(100);
			const float Sink = (Mode == EScrubMode::Off ? 0.f : Mode == EScrubMode::Normal ? R.ScrubNormal : R.ScrubHazard) * (Brownout ? R.BrownoutFactor : 1.f);
			M.Tick(10, {}); TestEqual(TEXT("Selected sink with power factor"), M.GetPpm(), 100.f - 10 * Sink);
			M.SetScrubDisabled(true); TestFalse(TEXT("Disabled changes nothing in tick"), M.Tick(10, {}));
			TestEqual(TEXT("Disabled removes nothing"), M.GetPpm(), 100.f - 10 * Sink);
			M.SetScrubDisabled(false); M.SetScrubMode(EScrubMode::Hazard); M.Tick(1000, {});
			TestEqual(TEXT("Scrubbing clamps at zero"), M.GetPpm(), 0.f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirValidationTest, "Airlock.Atmosphere.SettersValidationAndOwnTickChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirValidationTest::RunTest(const FString& Parameters)
{
	FShipAtmosphere M;
	TestTrue(TEXT("Mode changes"), M.SetScrubMode(EScrubMode::Off));
	TestFalse(TEXT("Same mode unchanged"), M.SetScrubMode(EScrubMode::Off));
	TestFalse(TEXT("Invalid mode"), M.SetScrubMode(static_cast<EScrubMode>(255)));
	TestTrue(TEXT("Brownout changes"), M.SetBrownout(true)); TestFalse(TEXT("Same brownout"), M.SetBrownout(true));
	TestTrue(TEXT("Disabled changes"), M.SetScrubDisabled(true)); TestFalse(TEXT("Same disabled"), M.SetScrubDisabled(true));
	TestTrue(TEXT("Ppm changes"), M.SetPpm(40.5f)); TestEqual(TEXT("Setter state immediate"), M.State().Ppm, 40.5f);
	TestFalse(TEXT("Tick does not report setter changes"), M.Tick(1, {}));
	TestFalse(TEXT("Same ppm"), M.SetPpm(40.5f));
	for (float V : { -1.f, 501.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{ TestFalse(TEXT("Invalid set"), M.SetPpm(V)); }
	TestTrue(TEXT("Add clamps max"), M.AddPpm(1000)); TestEqual(TEXT("Max ppm"), M.GetPpm(), FAtmosphereRates().MaxPpm);
	TestFalse(TEXT("Already capped"), M.AddPpm(1)); TestTrue(TEXT("Subtract clamps zero"), M.AddPpm(-1000));
	TestFalse(TEXT("Nonfinite add"), M.AddPpm(std::numeric_limits<float>::infinity()));
	TestFalse(TEXT("NaN add"), M.AddPpm(std::numeric_limits<float>::quiet_NaN()));
	for (float D : { 0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{ TestFalse(TEXT("Invalid delta no-op"), M.Tick(D, { WarmAirTestCrate() })); }
	TestTrue(TEXT("No clocks created by invalid tick"), M.PuffingCrates().IsEmpty());
	M.Reset(); M.SetPpm(1); TestTrue(TEXT("Crossing a whole ppm publishes"), M.Tick(0.25f, {}));
	// 1 -> 0.875 crosses a floored integer; a subsequent fraction stays in that integer.
	TestFalse(TEXT("Same floor not published"), M.Tick(0.25f, {}));
	M.SetScrubMode(EScrubMode::Off); M.Tick(10, { WarmAirTestCrate() });
	M.SetPpm(100); M.Tick(1, { WarmAirTestCrate() });
	TestEqual(TEXT("SetPpm does not restart off phase"), M.GetPpm(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirDeterminismTest, "Airlock.Atmosphere.Determinism2000Ticks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirDeterminismTest::RunTest(const FString& Parameters)
{
	FShipAtmosphere A, B;
	for (int32 I = 0; I < 2000; ++I)
	{
		TArray<FCargoCrateState> C = { WarmAirTestCrate(), WarmAirTestCrate(TEXT("C4")) };
		C[0].Temperature = I % 71; C[0].State = I % 90 < 10 ? ECargoState::Racked : ECargoState::Secured;
		if (I > 1000) { C[1].State = ECargoState::Vented; }
		if (I % 100 == 0) { const auto Mode = static_cast<EScrubMode>((I / 100) % 3); A.SetScrubMode(Mode); B.SetScrubMode(Mode); }
		if (I % 73 == 0) { A.SetBrownout(I % 2 != 0); B.SetBrownout(I % 2 != 0); A.AddPpm(7); B.AddPpm(7); }
		TestEqual(TEXT("Same tick change"), A.Tick(0.25f, C), B.Tick(0.25f, C));
		TestTrue(TEXT("Same state"), A.State() == B.State());
		TestTrue(TEXT("Same FX ids"), A.PuffingCrates() == B.PuffingCrates());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirExposureModelTest, "Airlock.Atmosphere.ExposureRatesRecoveryAndClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirExposureModelTest::RunTest(const FString& Parameters)
{
	const FAtmosphereRates R;
	TestEqual(TEXT("20 ppm gives 1 percent per second"), FShipAtmosphere::TickExposure(0, 20, 10, R), 10.f);
	TestEqual(TEXT("50 ppm gives 2.5 percent per second"), FShipAtmosphere::TickExposure(0, 50, 10, R), 25.f);
	TestEqual(TEXT("Below warn recovers 0.5 per second"), FShipAtmosphere::TickExposure(50, 19, 10, R), 45.f);
	TestEqual(TEXT("Upper clamp"), FShipAtmosphere::TickExposure(99, 500, 100, R), 100.f);
	TestEqual(TEXT("Lower clamp"), FShipAtmosphere::TickExposure(1, 0, 100, R), 0.f);
	for (float D : { 0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
	{ TestEqual(TEXT("Invalid delta returns input"), FShipAtmosphere::TickExposure(37, 100, D, R), 37.f); }
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirBurnedTest, "Airlock.Atmosphere.BurnedCaskDropsPuffClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAirBurnedTest::RunTest(const FString& Parameters)
{
	FShipAtmosphere M; M.SetScrubMode(EScrubMode::Off);
	TArray<FCargoCrateState> C = { WarmAirTestCrate() };
	M.Tick(9, C); C[0].State = ECargoState::Burned; M.Tick(100, C);
	TestEqual(TEXT("Burn adds no gas or rupture"), M.GetPpm(), 18.f);
	TestFalse(TEXT("Burn drops puff FX"), M.IsPuffing(C[0].Id));
	TestEqual(TEXT("No sources"), M.State().ActiveSources, 0);
	C[0].State = ECargoState::Carried; M.Tick(10, C);
	TestEqual(TEXT("Requalification has a fresh ten second puff"), M.GetPpm(), 38.f);
	return true;
}
#endif
