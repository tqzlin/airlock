// Copyright Airlock project. All Rights Reserved.
#include "Ship/FuelSystem.h"

bool FFuelState::operator==(const FFuelState& O) const
{
	return Units == O.Units && bFeedLeak == O.bFeedLeak && bBypassOpen == O.bBypassOpen
		&& BurningCrate == O.BurningCrate && BurnSecondsLeft == O.BurnSecondsLeft
		&& CasksBurned == O.CasksBurned && bInjectorsFouled == O.bInjectorsFouled && bExhausted == O.bExhausted;
}
void FFuelSystem::Reset()
{
	Fuel = FFuelState(); PreciseUnits = FMath::Clamp(Rates.StartFuel, 0.f, Rates.Capacity);
	Fuel.Units = static_cast<float>(PreciseUnits); BurnRemaining = 0;
}
bool FFuelSystem::SetUnits(float Units)
{
	if (!FMath::IsFinite(Units)) { return false; }
	const double Next = FMath::Clamp(Units, 0.f, Rates.Capacity);
	if (Next == PreciseUnits) { return false; }
	PreciseUnits = Next; Fuel.Units = static_cast<float>(Next); return true;
}
bool FFuelSystem::CloseLeak()
{
	if (!Fuel.bFeedLeak) { return false; }
	Fuel.bFeedLeak = false; return true;
}
bool FFuelSystem::OpenBypass()
{
	if (Fuel.bBypassOpen) { return false; }
	Fuel.bBypassOpen = true; return true;
}
bool FFuelSystem::FeedCask(FName Id)
{
	if (Id.IsNone() || !Fuel.bBypassOpen || !Fuel.BurningCrate.IsNone()) { return false; }
	Fuel.BurningCrate = Id; BurnRemaining = Rates.CaskBurnSeconds;
	Fuel.BurnSecondsLeft = FMath::CeilToInt(BurnRemaining); return true;
}
bool FFuelSystem::ClearInjectors()
{
	if (!Fuel.bInjectorsFouled) { return false; }
	Fuel.bInjectorsFouled = false; return true;
}
float FFuelSystem::SootPerSecond() const
{
	return Fuel.BurningCrate.IsNone() ? 0.f : Rates.SootPpmPerCask / Rates.CaskBurnSeconds;
}
float FFuelSystem::FoulingOnFeed() const
{
	return Fuel.bInjectorsFouled ? Rates.FoulingPerCaskFouled : Rates.FoulingPerCask;
}
float FFuelSystem::SecondsOfFuelLeft() const
{
	return static_cast<float>(PreciseUnits * 60.0 / Rates.NominalBurnPerMinute);
}
bool FFuelSystem::Tick(float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return false; }
	FFuelState Before = Fuel; Before.Units = FMath::FloorToFloat(Before.Units);
	const double Rate = Rates.NominalBurnPerMinute / 60.0 + (Fuel.bFeedLeak ? Rates.LeakPerSecond : 0.0);
	const auto Drain = [this, Rate](double Seconds)
	{
		PreciseUnits = FMath::Clamp(PreciseUnits - Rate * Seconds, 0.0, static_cast<double>(Rates.Capacity));
		if (PreciseUnits <= 0) { Fuel.bExhausted = true; }
	};
	double Remaining = DeltaSeconds;
	if (!Fuel.BurningCrate.IsNone())
	{
		const double Step = FMath::Min(Remaining, BurnRemaining);
		Drain(Step); Remaining -= Step; BurnRemaining -= Step;
		if (BurnRemaining <= 0)
		{
			PreciseUnits = FMath::Min(PreciseUnits + Rates.CaskFuel, static_cast<double>(Rates.Capacity));
			Fuel.BurningCrate = NAME_None; ++Fuel.CasksBurned;
			if (Rates.CasksPerFouling > 0 && Fuel.CasksBurned % Rates.CasksPerFouling == 0) { Fuel.bInjectorsFouled = true; }
		}
	}
	if (Remaining > 0) { Drain(Remaining); }
	Fuel.BurnSecondsLeft = FMath::CeilToInt(BurnRemaining);
	Fuel.Units = static_cast<float>(PreciseUnits);
	FFuelState After = Fuel; After.Units = FMath::FloorToFloat(After.Units);
	return !(Before == After);
}
FString FFuelSystem::Summary() const
{
	return FString::Printf(TEXT("Fuel %d%% leak=%d bypass=%s burning=%s burned=%d fouled=%d exhausted=%d"),
		FMath::FloorToInt(Fuel.Units), Fuel.bFeedLeak, Fuel.bBypassOpen ? TEXT("open") : TEXT("closed"),
		Fuel.BurningCrate.IsNone() ? TEXT("none") : *Fuel.BurningCrate.ToString(), Fuel.CasksBurned,
		Fuel.bInjectorsFouled, Fuel.bExhausted);
}
