// Copyright Airlock project. All Rights Reserved.
#include "Ship/RefineryModel.h"

namespace
{
	int32* KnobSteps(FStillState& S, ERefineryKnob Knob)
	{
		switch (Knob)
		{
		case ERefineryKnob::Heat: return &S.HeatSteps;
		case ERefineryKnob::Feed: return &S.FeedSteps;
		case ERefineryKnob::Vent: return &S.VentSteps;
		default: return nullptr;
		}
	}

	FRefineryState Published(FRefineryState S)
	{
		S.TankDieselL = FMath::FloorToFloat(S.TankDieselL);
		S.TotalGasL = FMath::FloorToFloat(S.TotalGasL);
		S.TotalTarL = FMath::FloorToFloat(S.TotalTarL);
		S.SpilledL = FMath::FloorToFloat(S.SpilledL);
		for (FStillState& Still : S.Stills)
		{
			Still.TempK = FMath::FloorToFloat(Still.TempK);
			Still.PressureBar = FMath::FloorToFloat(Still.PressureBar * 10.f) / 10.f;
			Still.Calibration = FMath::FloorToFloat(Still.Calibration * 100.f) / 100.f;
			Still.Coke = FMath::FloorToFloat(Still.Coke);
			Still.Condition = FMath::FloorToFloat(Still.Condition);
			Still.ChargeL = FMath::FloorToFloat(Still.ChargeL);
			Still.BatchDieselL = FMath::FloorToFloat(Still.BatchDieselL);
			Still.BatchGasL = FMath::FloorToFloat(Still.BatchGasL);
			Still.BatchTarL = FMath::FloorToFloat(Still.BatchTarL);
			Still.FloodSeconds = FMath::FloorToFloat(Still.FloodSeconds);
		}
		return S;
	}

	FRefineryEvent Event(ERefineryEvent Type, const FStillState& S)
	{
		FRefineryEvent E; E.Type = Type; E.StillId = S.Id; E.CaskId = S.CurrentCask;
		return E;
	}
}

bool FStillState::operator==(const FStillState& O) const
{
	return Id == O.Id && Status == O.Status && HeatSteps == O.HeatSteps && FeedSteps == O.FeedSteps
		&& VentSteps == O.VentSteps && TempK == O.TempK && PressureBar == O.PressureBar
		&& Calibration == O.Calibration && Coke == O.Coke && bFouled == O.bFouled && Condition == O.Condition
		&& ChargeL == O.ChargeL && CurrentCask == O.CurrentCask && bCurrentLight == O.bCurrentLight
		&& Hopper == O.Hopper && BatchDieselL == O.BatchDieselL && BatchGasL == O.BatchGasL
		&& BatchTarL == O.BatchTarL && BatchesDone == O.BatchesDone && FloodSeconds == O.FloodSeconds;
}

bool FRefineryCask::operator==(const FRefineryCask& O) const { return Id == O.Id && bLight == O.bLight; }

bool FRefineryState::operator==(const FRefineryState& O) const
{
	return Stills == O.Stills && Waiting == O.Waiting && CasksDelivered == O.CasksDelivered
		&& TankDieselL == O.TankDieselL && TotalGasL == O.TotalGasL && TotalTarL == O.TotalTarL
		&& SpilledL == O.SpilledL && CasksRefined == O.CasksRefined && CasksLost == O.CasksLost
		&& Spills == O.Spills && Pops == O.Pops && Flashes == O.Flashes;
}

void FRefineryModel::Build(int32 StillCount, const TArray<FRefineryCask>& Casks)
{
	BuildStillCount = FMath::Max(0, StillCount);
	BuildCasks = Casks;
	Floor = FRefineryState();
	Floor.Waiting = BuildCasks;
	Floor.CasksDelivered = BuildCasks.Num();
	for (int32 I = 0; I < BuildStillCount; ++I)
	{
		FStillState Still;
		Still.Id = FName(*FString::Printf(TEXT("Still_%d"), I + 1));
		Still.TempK = Rates.HeatBaseK;
		Still.PressureBar = Rates.PressureFloorBar;
		Still.VentSteps = FMath::Clamp(11, 0, Rates.KnobSteps);
		Floor.Stills.Add(Still);
	}
}

void FRefineryModel::Rebuild() { Build(BuildStillCount, BuildCasks); }

const FStillState* FRefineryModel::FindStill(FName Id) const
{
	return Floor.Stills.FindByPredicate([Id](const FStillState& S) { return S.Id == Id; });
}

FStillState* FRefineryModel::MutableStill(FName Id)
{
	return Floor.Stills.FindByPredicate([Id](const FStillState& S) { return S.Id == Id; });
}

float FRefineryModel::BatchProgress(FName Id) const
{
	const FStillState* S = FindStill(Id);
	return S && !S->CurrentCask.IsNone() ? 1.f - S->ChargeL / Rates.CaskLitres : 0.f;
}

bool FRefineryModel::SetKnob(FName Still, ERefineryKnob Knob, int32 Steps)
{
	FStillState* S = MutableStill(Still);
	int32* Value = S ? KnobSteps(*S, Knob) : nullptr;
	if (!Value) { return false; }
	*Value = FMath::Clamp(Steps, 0, Rates.KnobSteps); return true;
}

bool FRefineryModel::StepKnob(FName Still, ERefineryKnob Knob, int32 Steps)
{
	FStillState* S = MutableStill(Still);
	int32* Value = S ? KnobSteps(*S, Knob) : nullptr;
	if (!Value || Steps == 0) { return false; }
	// Widen the addition so even an extreme console/actor input saturates instead of wrapping.
	*Value = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(*Value) + Steps, 0, Rates.KnobSteps));
	return true;
}

bool FRefineryModel::LoadHopper(FName Still, FName Cask)
{
	FStillState* S = MutableStill(Still);
	const int32 Index = Floor.Waiting.IndexOfByPredicate([Cask](const FRefineryCask& C) { return C.Id == Cask; });
	if (!S || Index == INDEX_NONE || S->Hopper.Num() >= Rates.HopperCasks) { return false; }
	S->Hopper.Add(Cask); Floor.Waiting.RemoveAt(Index); return true;
}

bool FRefineryModel::Calibrate(FName Still)
{
	FStillState* S = MutableStill(Still);
	if (!S || S->Status == EStillStatus::Failed) { return false; }
	S->Calibration = 1.f; return true;
}

bool FRefineryModel::Tick(float DeltaSeconds, float RoomPpm, float& OutRoomPpm, TArray<FRefineryEvent>& OutEvents)
{
	OutRoomPpm = 0.f;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return false; }
	if (!FMath::IsFinite(RoomPpm)) { RoomPpm = 0.f; }
	const FRefineryState Before = Published(Floor);
	float Remaining = DeltaSeconds;
	while (Remaining > 0.f)
	{
		const float Dt = FMath::Min(Remaining, Rates.MaxStepSeconds);
		for (FStillState& S : Floor.Stills) { TickStill(S, Dt, RoomPpm, OutRoomPpm, OutEvents); }
		Remaining -= Dt;
	}
	return !(Before == Published(Floor));
}

void FRefineryModel::TickStill(FStillState& S, float Dt, float RoomPpm, float& OutRoomPpm, TArray<FRefineryEvent>& OutEvents)
{
	const float Heat = static_cast<float>(S.HeatSteps) / Rates.KnobSteps;
	const float Feed = static_cast<float>(S.FeedSteps) / Rates.KnobSteps;

	// 1. The separator takes a cask even at feed zero: loaded preheating does not drift the trap.
	if (S.Status == EStillStatus::Idle || (S.Status == EStillStatus::Running && S.ChargeL == 0.f))
	{
		if (!S.Hopper.IsEmpty())
		{
			S.CurrentCask = S.Hopper[0]; S.Hopper.RemoveAt(0);
			const FRefineryCask* C = BuildCasks.FindByPredicate([&S](const FRefineryCask& Cask) { return Cask.Id == S.CurrentCask; });
			S.bCurrentLight = C->bLight; S.ChargeL = Rates.CaskLitres;
			S.BatchDieselL = S.BatchGasL = S.BatchTarL = 0.f;
			S.Status = EStillStatus::Running;
		}
		else { S.Status = EStillStatus::Idle; }
	}

	// 2. Product enters the tank immediately, using the temperature at the start of the slice.
	const float Fed = S.Status == EStillStatus::Running ? FMath::Min(S.ChargeL, Feed * Rates.MaxFeedLps * Dt) : 0.f;
	if (Fed > 0.f)
	{
		const float Ideal = S.bCurrentLight ? Rates.LightIdealK : Rates.HeavyIdealK;
		const float Bell = FMath::Max(0.f, 1.f - FMath::Square((S.TempK - Ideal) / Rates.BellHalfWidthK));
		const float YDiesel = Rates.YieldMax * Bell * (0.5f + 0.5f * S.Calibration) * (S.bFouled ? 0.5f : 1.f);
		const float YGas = FMath::Min(1.f - YDiesel, Rates.GasBase + Rates.GasSlope
			* FMath::Clamp((S.TempK - Rates.GasStartK) / Rates.GasSpanK, 0.f, 1.f));
		const float Diesel = Fed * YDiesel, Gas = Fed * YGas, Tar = Fed * (1.f - YDiesel - YGas);
		S.BatchDieselL += Diesel; Floor.TankDieselL += Diesel;
		S.BatchGasL += Gas; Floor.TotalGasL += Gas;
		S.BatchTarL += Tar; Floor.TotalTarL += Tar;
		S.ChargeL -= Fed; OutRoomPpm += Gas * Rates.RoomPpmPerGasL;
	}

	// 3. The warm bath survives a shutdown; heat zero cools it through the same lag.
	const float Target = Rates.HeatBaseK + Heat * Rates.HeatSpanK;
	S.TempK = FMath::Max(Rates.HeatBaseK, S.TempK + (Target - S.TempK)
		* FMath::Min(1.f, Dt / Rates.HeatTauSeconds) - Fed * Rates.FeedCoolKPerL);

	// 4. The weir spills unfed crude only; a flooded batch can still finish.
	if (S.Status == EStillStatus::Running && Feed > Rates.FloodFeed)
	{
		S.FloodSeconds += Dt;
		if (S.FloodSeconds >= Rates.FloodAfterSeconds)
		{
			FRefineryEvent E = Event(ERefineryEvent::Spill, S);
			E.Litres = FMath::Min(S.ChargeL, Rates.FloodSpillL);
			S.ChargeL -= E.Litres; Floor.SpilledL += E.Litres; ++Floor.Spills;
			OutEvents.Add(E); S.FloodSeconds = 0.f;
			S.TempK = FMath::Max(Rates.HeatBaseK, S.TempK - Rates.FloodCoolK);
		}
	}
	else { S.FloodSeconds = 0.f; }

	// 8. Drift is charged only for a slice that actually processed crude.
	if (Fed > 0.f) { S.Calibration = FMath::Max(0.f, S.Calibration - Rates.CalDriftPerS * Dt); }

	// 9. A queued batch starts next slice, without inserting an idle/dry slice.
	if (S.Status == EStillStatus::Running && S.ChargeL == 0.f && !S.CurrentCask.IsNone())
	{
		FRefineryEvent E = Event(ERefineryEvent::BatchDone, S);
		E.DieselL = S.BatchDieselL; E.GasL = S.BatchGasL; E.TarL = S.BatchTarL;
		E.Litres = E.DieselL + E.GasL + E.TarL; OutEvents.Add(E);
		++S.BatchesDone; ++Floor.CasksRefined; S.CurrentCask = NAME_None;
		if (S.Hopper.IsEmpty()) { S.Status = EStillStatus::Idle; }
	}
	if (S.Status != EStillStatus::Running) { S.FloodSeconds = 0.f; }
}
