// Copyright Airlock project. All Rights Reserved.
#include "Ship/RefineryModel.h"

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
