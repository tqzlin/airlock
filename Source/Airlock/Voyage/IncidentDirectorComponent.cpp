// Copyright Airlock project. All Rights Reserved.
#include "Voyage/IncidentDirectorComponent.h"
#include "Airlock.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

UIncidentDirectorComponent::UIncidentDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UIncidentDirectorComponent::IsVoyageActive() const
{
	const AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	return bStarted && GetOwner() && GetOwner()->HasAuthority() && GS && GS->bVoyageActive &&
		GS->CurrentSectionId == ShipLayout.SectionId && GS->CurrentSectionId != AAirlockGameMode::HubSectionId;
}

double UIncidentDirectorComponent::ElapsedSeconds() const
{
	const AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	return GS ? Voyage.DurationSeconds - (GS->VoyageEndServerTime - GS->GetServerWorldTimeSeconds()) : 0.0;
}

void UIncidentDirectorComponent::StartVoyage(const FVoyageSpec& Spec, const FShipLayout& Layout, int32 CrewCount)
{
	if (bStarted || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	Voyage = Spec;
	ShipLayout = Layout;
	Schedule = FIncidentSchedule::Build(Spec, Layout, CrewCount);
	bStarted = true;
	UE_LOG(LogAirlock, Log, TEXT("Incident director started: crew=%d entries=%d seed=%d"), CrewCount, Schedule.Num(), Spec.Seed);
	Reschedule();
}

void UIncidentDirectorComponent::Reschedule()
{
	if (!GetWorld()) { return; }
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.ClearTimer(IncidentTimer);
	if (!IsVoyageActive() || !Schedule.IsValidIndex(NextEntry)) { return; }
	const double Delay = Schedule[NextEntry].AtSeconds - ElapsedSeconds();
	if (Delay <= 0.0)
	{
		IncidentTimer = Timers.SetTimerForNextTick(this, &UIncidentDirectorComponent::FireDueIncidents);
	}
	else
	{
		Timers.SetTimer(IncidentTimer, this, &UIncidentDirectorComponent::FireDueIncidents, static_cast<float>(Delay), false);
	}
}

void UIncidentDirectorComponent::FireDueIncidents()
{
	if (!IsVoyageActive()) { return; }
	const int32 End = FIncidentSchedule::FindNextUnfired(Schedule, NextEntry, ElapsedSeconds());
	while (NextEntry < End)
	{
		// Consume even skipped entries: another player's repair must never resurrect an old event.
		ApplyIncident(Schedule[NextEntry++]);
	}
	Reschedule();
}

FString UIncidentDirectorComponent::UnavailableReason(EIncidentType Type, FName Target) const
{
	const AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	if (!GS || !FIncidentSchedule::Targets(ShipLayout, Type).Contains(Target)) { return TEXT("missing or incompatible target"); }
	if (Type == EIncidentType::ModuleDamage)
	{
		const FShipModuleState* Module = GS->ShipModules.FindByPredicate([Target](const FShipModuleState& M) { return M.Id == Target; });
		if (!Module) { return TEXT("missing module"); }
		if (Module->Status == EShipModuleStatus::Failed) { return TEXT("already Failed"); }
	}
	else
	{
		const FShipLineState* Line = GS->ShipLines.FindByPredicate([Target](const FShipLineState& L) { return L.Id == Target; });
		if (!Line) { return TEXT("missing line"); }
		const EShipLineKind Kind = Type == EIncidentType::CoolantLeak ? EShipLineKind::Coolant : EShipLineKind::Power;
		if (Line->Kind != Kind) { return TEXT("incompatible line kind"); }
		if (Line->Fault != EShipLineFault::None) { return TEXT("already faulted"); }
	}
	return FString();
}

bool UIncidentDirectorComponent::ApplyIncident(const FIncidentSpec& Incident)
{
	AAirlockGameMode* GM = Cast<AAirlockGameMode>(GetOwner());
	FString Reason = IsVoyageActive() && GM ? UnavailableReason(Incident.Type, Incident.TargetId) : TEXT("no active ship voyage");
	if (Reason.IsEmpty() && Incident.Type == EIncidentType::ModuleDamage &&
		(!FMath::IsFinite(Incident.Magnitude) || Incident.Magnitude <= 0.f)) { Reason = TEXT("invalid damage magnitude"); }
	bool bApplied = false;
	if (Reason.IsEmpty())
	{
		bApplied = Incident.Type == EIncidentType::ModuleDamage ? GM->ShipDamageModule(Incident.TargetId, Incident.Magnitude)
			: GM->ShipSetLineFault(Incident.TargetId, Incident.Type == EIncidentType::CoolantLeak ? EShipLineFault::Leak : EShipLineFault::Short);
		if (!bApplied) { Reason = TEXT("graph rejected target"); }
	}
	UE_LOG(LogAirlock, Log, TEXT("Incident %s %s (%.2f) at %.3fs elapsed=%.3fs%s"),
		FIncidentSchedule::TypeName(Incident.Type), *Incident.TargetId.ToString(), Incident.Magnitude, Incident.AtSeconds,
		ElapsedSeconds(), bApplied ? TEXT("") : *FString::Printf(TEXT(" skipped: %s"), *Reason));
	if (bApplied)
	{
		UE_LOG(LogAirlock, Log, TEXT("Captain %s: %s"), *Voyage.Captain.ToString(), *Incident.CaptainLine.ToString());
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (AAirlockPlayerController* PC = Cast<AAirlockPlayerController>(It->Get())) { PC->ClientCaptainSays(Incident.CaptainLine); }
		}
	}
	return bApplied;
}

bool UIncidentDirectorComponent::FireNow(EIncidentType Type, FName Target, float Magnitude)
{
	if (Target.IsNone())
	{
		for (FName Candidate : FIncidentSchedule::Targets(ShipLayout, Type))
		{
			if (UnavailableReason(Type, Candidate).IsEmpty()) { Target = Candidate; break; }
		}
	}
	FIncidentSpec Incident;
	Incident.Type = Type; Incident.TargetId = Target;
	Incident.Magnitude = Type == EIncidentType::ModuleDamage ? Magnitude : 0.f;
	Incident.AtSeconds = static_cast<float>(ElapsedSeconds());
	Incident.CaptainLine = FIncidentSchedule::MakeCaptainLine(Type, Target, Voyage.Captain);
	return ApplyIncident(Incident);
}

void UIncidentDirectorComponent::LogSchedule() const
{
	UE_LOG(LogAirlock, Log, TEXT("Incident schedule: %d entries next=%d duration=%.0fs"), Schedule.Num(), NextEntry, Voyage.DurationSeconds);
	for (int32 I = 0; I < Schedule.Num(); ++I)
	{
		const FIncidentSpec& Entry = Schedule[I];
		UE_LOG(LogAirlock, Log, TEXT("Incident schedule entry=%d at %.3fs %s %s (%.2f)"), I, Entry.AtSeconds,
			FIncidentSchedule::TypeName(Entry.Type), *Entry.TargetId.ToString(), Entry.Magnitude);
	}
}

void UIncidentDirectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(IncidentTimer); }
	Super::EndPlay(EndPlayReason);
}
