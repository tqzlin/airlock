// Copyright Airlock project. All Rights Reserved.
#include "Ship/ShipGraph.h"
#include "Ship/ShipLayouts.h"

bool FShipModuleState::operator==(const FShipModuleState& Other) const
{
	return Id == Other.Id && Type == Other.Type && Condition == Other.Condition && Status == Other.Status;
}
bool FShipLineState::operator==(const FShipLineState& Other) const
{
	return Id == Other.Id && Kind == Other.Kind && FromModule == Other.FromModule && ToModule == Other.ToModule && Fault == Other.Fault;
}
EShipModuleStatus FShipGraph::StatusFor(float Condition)
{
	return Condition >= 60.f ? EShipModuleStatus::Nominal : Condition >= 20.f ? EShipModuleStatus::Degraded : EShipModuleStatus::Failed;
}
const TCHAR* FShipGraph::StatusName(EShipModuleStatus Status)
{
	switch (Status)
	{
	case EShipModuleStatus::Nominal: return TEXT("Nominal");
	case EShipModuleStatus::Degraded: return TEXT("Degraded");
	default: return TEXT("Failed");
	}
}
const TCHAR* FShipGraph::FaultName(EShipLineFault Fault)
{
	switch (Fault)
	{
	case EShipLineFault::None: return TEXT("None");
	case EShipLineFault::Leak: return TEXT("Leak");
	default: return TEXT("Short");
	}
}
void FShipGraph::Build(const FShipLayout& Layout)
{
	ModuleStates.Reset(); LineStates.Reset();
	for (const FShipModuleSpec& Spec : Layout.Modules)
	{
		FShipModuleState State;
		State.Id = Spec.Id; State.Type = Spec.Type;
		ModuleStates.Add(State);
	}
	for (const FShipLineSpec& Spec : Layout.Lines)
	{
		FShipLineState State;
		State.Id = Spec.Id; State.Kind = Spec.Kind; State.FromModule = Spec.From; State.ToModule = Spec.To;
		LineStates.Add(State);
	}
}
const FShipModuleState* FShipGraph::FindModule(FName Id) const
{
	return ModuleStates.FindByPredicate([Id](const FShipModuleState& State) { return State.Id == Id; });
}
const FShipLineState* FShipGraph::FindLine(FName Id) const
{
	return LineStates.FindByPredicate([Id](const FShipLineState& State) { return State.Id == Id; });
}
bool FShipGraph::SetLineFault(FName Id, EShipLineFault Fault)
{
	if (Fault != EShipLineFault::None && Fault != EShipLineFault::Leak && Fault != EShipLineFault::Short) { return false; }
	for (FShipLineState& State : LineStates)
	{
		if (State.Id == Id) { State.Fault = Fault; return true; }
	}
	return false;
}
bool FShipGraph::AdjustModule(FName Id, float Amount, bool bRepair)
{
	if (!FMath::IsFinite(Amount) || Amount < 0.f) { return false; }
	for (FShipModuleState& State : ModuleStates)
	{
		if (State.Id == Id)
		{
			State.Condition = FMath::Clamp(State.Condition + (bRepair ? Amount : -Amount), 0.f, 100.f);
			State.Status = StatusFor(State.Condition);
			return true;
		}
	}
	return false;
}
bool FShipGraph::DamageModule(FName Id, float Amount) { return AdjustModule(Id, Amount, false); }
bool FShipGraph::RepairModule(FName Id, float Amount) { return AdjustModule(Id, Amount, true); }

TArray<FName> FShipGraph::Tick(float DeltaSeconds)
{
	TArray<FName> Changed;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return Changed; }
	// Snapshot all rates before changing any condition: layout order cannot change propagation.
	float CoreDrain = 0.f, Brownout = 0.f;
	for (const FShipModuleState& State : ModuleStates)
	{
		if (State.Type == EShipModuleType::CoolantLoop)
		{
			CoreDrain += State.Status == EShipModuleStatus::Failed ? Rates.CoreDriftFailed : State.Status == EShipModuleStatus::Degraded ? Rates.CoreDriftDegraded : 0.f;
		}
		if (State.Type == EShipModuleType::PowerCore)
		{
			Brownout += State.Status == EShipModuleStatus::Failed ? Rates.BrownoutFailed : State.Status == EShipModuleStatus::Degraded ? Rates.BrownoutDegraded : 0.f;
		}
	}
	for (FShipModuleState& State : ModuleStates)
	{
		float Drain = State.Type == EShipModuleType::PowerCore ? CoreDrain :
			(State.Type == EShipModuleType::LifeSupport || State.Type == EShipModuleType::CargoClamps) ? Brownout : 0.f;
		for (const FShipLineState& Line : LineStates)
		{
			if (Line.Kind == EShipLineKind::Coolant && Line.Fault == EShipLineFault::Leak && State.Type == EShipModuleType::CoolantLoop) { Drain += Rates.LeakDrain; }
			if (Line.Kind == EShipLineKind::Power && Line.Fault == EShipLineFault::Short && State.Id == Line.ToModule) { Drain += Rates.ShortDrain; }
		}
		State.Condition = FMath::Clamp(State.Condition - FMath::Max(0.f, Drain) * DeltaSeconds, 0.f, 100.f);
		const EShipModuleStatus Status = StatusFor(State.Condition);
		if (Status != State.Status) { Changed.Add(State.Id); State.Status = Status; }
	}
	return Changed;
}
FString FShipGraph::Summary() const
{
	TArray<FString> Entries;
	for (const FShipModuleState& State : ModuleStates)
	{
		Entries.Add(FString::Printf(TEXT("Ship %s %s cond=%.2f"), *State.Id.ToString(), StatusName(State.Status), State.Condition));
	}
	for (const FShipLineState& State : LineStates)
	{
		Entries.Add(FString::Printf(TEXT("Line %s fault=%s kind=%s from=%s to=%s"), *State.Id.ToString(), FaultName(State.Fault),
			State.Kind == EShipLineKind::Coolant ? TEXT("Coolant") : TEXT("Power"), *State.FromModule.ToString(), *State.ToModule.ToString()));
	}
	return FString::Join(Entries, TEXT("\n"));
}
