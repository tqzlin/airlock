// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Voyage/IncidentSchedule.h"
#include "Ship/ShipLayouts.h"
#include "IncidentDirectorComponent.generated.h"

/** Server-only second timer chain alongside the captain's fixed voyage announcements. */
UCLASS()
class AIRLOCK_API UIncidentDirectorComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UIncidentDirectorComponent();
	void StartVoyage(const FVoyageSpec& Spec, const FShipLayout& Layout, int32 CrewCount);
	void Reschedule();
	/** Own timer callback; also flushed by the clock before docking, irrespective of equal-time timer order. */
	void FireDueIncidents();
	bool FireNow(EIncidentType Type, FName Target = NAME_None, float Magnitude = 40.f);
	bool ApplyIncident(const FIncidentSpec& Incident);
	const TArray<FIncidentSpec>& GetSchedule() const { return Schedule; }
	int32 GetNextEntry() const { return NextEntry; }
	void LogSchedule() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FVoyageSpec Voyage;
	FShipLayout ShipLayout;
	TArray<FIncidentSpec> Schedule;
	int32 NextEntry = 0;
	bool bStarted = false;
	FTimerHandle IncidentTimer;
	double ElapsedSeconds() const;
	bool IsVoyageActive() const;
	FString UnavailableReason(EIncidentType Type, FName Target) const;
};
