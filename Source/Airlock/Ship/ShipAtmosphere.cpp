// Copyright Airlock project. All Rights Reserved.
#include "Ship/ShipAtmosphere.h"
#include "Ship/CargoModel.h"

bool FAtmosphereState::operator==(const FAtmosphereState& Other) const
{
	return Ppm == Other.Ppm && ScrubMode == Other.ScrubMode && bBrownout == Other.bBrownout
		&& bScrubDisabled == Other.bScrubDisabled && ActiveSources == Other.ActiveSources;
}
void FShipAtmosphere::Reset()
{
	Air = FAtmosphereState();
	PuffClocks.Reset(); RuptureClocks.Reset(); Puffing.Reset();
}
bool FShipAtmosphere::SetScrubMode(EScrubMode Mode)
{
	if (Mode != EScrubMode::Off && Mode != EScrubMode::Normal && Mode != EScrubMode::Hazard) { return false; }
	if (Air.ScrubMode == Mode) { return false; }
	Air.ScrubMode = Mode; return true;
}
bool FShipAtmosphere::SetBrownout(bool bBrownout)
{
	if (Air.bBrownout == bBrownout) { return false; }
	Air.bBrownout = bBrownout; return true;
}
bool FShipAtmosphere::SetScrubDisabled(bool bDisabled)
{
	if (Air.bScrubDisabled == bDisabled) { return false; }
	Air.bScrubDisabled = bDisabled; return true;
}
bool FShipAtmosphere::SetPpm(float Ppm)
{
	if (!FMath::IsFinite(Ppm) || Ppm < 0 || Ppm > Rates.MaxPpm || Ppm == Air.Ppm) { return false; }
	Air.Ppm = Ppm; return true;
}
bool FShipAtmosphere::AddPpm(float Ppm)
{
	if (!FMath::IsFinite(Ppm)) { return false; }
	return SetPpm(static_cast<float>(FMath::Clamp(static_cast<double>(Air.Ppm) + Ppm, 0.0, static_cast<double>(Rates.MaxPpm))));
}
bool FShipAtmosphere::Tick(float DeltaSeconds, const TArray<FCargoCrateState>& Cargo)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return false; }
	const int32 BeforePpm = FMath::FloorToInt(Air.Ppm);
	const int32 BeforeSources = Air.ActiveSources;
	const TArray<FName> BeforePuffing = Puffing;
	Puffing.Reset(); Air.ActiveSources = 0;
	TSet<FName> Present;
	double Added = 0;
	for (const auto& Crate : Cargo)
	{
		Present.Add(Crate.Id);
		double Source = 0;
		if (Crate.State == ECargoState::Vented)
		{
			PuffClocks.Remove(Crate.Id);
			double& Clock = RuptureClocks.FindOrAdd(Crate.Id);
			const double Seconds = FMath::Min(static_cast<double>(DeltaSeconds), FMath::Max(0.0, Rates.RuptureSeconds - Clock));
			Source = Rates.RuptureRate * Seconds;
			Clock = FMath::Min(Clock + DeltaSeconds, static_cast<double>(Rates.RuptureSeconds));
			if (Seconds > 0) { Puffing.Add(Crate.Id); }
		}
		else if (Crate.State != ECargoState::Burned && Crate.bVolatile && Crate.Temperature >= FCargoRates().WarnTemperature && Crate.State != ECargoState::Racked)
		{
			double& Clock = PuffClocks.FindOrAdd(Crate.Id);
			// Integrate the on portions exactly, including ticks spanning several periods.
			const double Period = Rates.PuffPeriodSeconds;
			const double On = FMath::Clamp(static_cast<double>(Rates.PuffSeconds), 0.0, Period);
			if (Period > 0)
			{
				const auto OnTime = [Period, On](double T)
				{
					return FMath::FloorToDouble(T / Period) * On + FMath::Min(FMath::Fmod(T, Period), On);
				};
				Source = Rates.PuffRate * (OnTime(Clock + DeltaSeconds) - OnTime(Clock));
				Clock = FMath::Fmod(Clock + DeltaSeconds, Period);
			}
			Puffing.Add(Crate.Id);
		}
		else { PuffClocks.Remove(Crate.Id); }
		if (Source > 0) { ++Air.ActiveSources; Added += Source; }
	}
	for (auto It = PuffClocks.CreateIterator(); It; ++It) { if (!Present.Contains(It.Key())) { It.RemoveCurrent(); } }
	for (auto It = RuptureClocks.CreateIterator(); It; ++It) { if (!Present.Contains(It.Key())) { It.RemoveCurrent(); } }
	double Sink = Air.ScrubMode == EScrubMode::Hazard ? Rates.ScrubHazard : Air.ScrubMode == EScrubMode::Normal ? Rates.ScrubNormal : 0;
	if (Air.bBrownout) { Sink *= Rates.BrownoutFactor; }
	if (Air.bScrubDisabled) { Sink = 0; }
	Air.Ppm = static_cast<float>(FMath::Clamp(Air.Ppm + Added - Sink * DeltaSeconds, 0.0, static_cast<double>(Rates.MaxPpm)));
	return BeforePpm != FMath::FloorToInt(Air.Ppm) || BeforeSources != Air.ActiveSources || BeforePuffing != Puffing;
}
const TCHAR* FShipAtmosphere::ModeName(EScrubMode Mode)
{
	switch (Mode)
	{
	case EScrubMode::Off: return TEXT("Off");
	case EScrubMode::Normal: return TEXT("Normal");
	case EScrubMode::Hazard: return TEXT("Hazard");
	default: return TEXT("Unknown");
	}
}
FString FShipAtmosphere::Summary() const
{
	return FString::Printf(TEXT("Air %d ppm scrub=%s brownout=%d disabled=%d sources=%d"), FMath::FloorToInt(Air.Ppm),
		ModeName(Air.ScrubMode), Air.bBrownout, Air.bScrubDisabled, Air.ActiveSources);
}
float FShipAtmosphere::TickExposure(float Exposure, float Ppm, float DeltaSeconds, const FAtmosphereRates& Rates)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return Exposure; }
	const double Rate = Ppm >= Rates.WarnPpm ? Rates.ExposureRateAtWarn * (Ppm / Rates.WarnPpm) : -Rates.ExposureRecovery;
	return static_cast<float>(FMath::Clamp(Exposure + Rate * DeltaSeconds, 0.0, 100.0));
}
