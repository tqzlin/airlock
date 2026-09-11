// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Voyage/VoyageTypes.h"
#include "VoyageSubsystem.generated.h"

/**
 * Owns the voyage lifecycle: offers voyages at the station hub, accepts requests, and (later) spins up
 * the instanced ship server for each one. Everything that changes state is server-only.
 */
UCLASS()
class AIRLOCK_API UVoyageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Deterministically builds a voyage spec from its type and seed. Pure: same inputs, same spec. */
	UFUNCTION(BlueprintPure, Category = "Airlock|Voyage")
	static FVoyageSpec MakeVoyageSpec(EVoyageType Type, int32 Seed);

	/** The voyages currently on offer at the hub board: one per type, seeded from the day seed. */
	UFUNCTION(BlueprintCallable, Category = "Airlock|Voyage")
	TArray<FVoyageSpec> GetOfferedVoyages(int32 DaySeed) const;

	/** Server-only. Registers a voyage and returns its id. Fails on clients. */
	UFUNCTION(BlueprintCallable, Category = "Airlock|Voyage")
	bool RequestVoyage(const FVoyageSpec& Spec, FGuid& OutVoyageId);

	UFUNCTION(BlueprintPure, Category = "Airlock|Voyage")
	int32 GetActiveVoyageCount() const { return ActiveVoyages.Num(); }

private:
	TMap<FGuid, FVoyageSpec> ActiveVoyages;
};
