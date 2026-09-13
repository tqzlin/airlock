// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Voyage/VoyageTypes.h"
#include "Ship/CargoModel.h"
#include "VoyageSubsystem.generated.h"

class UAirlockSaveGame;

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

	/** The voyage the crew is about to leave on / is currently on. Survives level travel (GameInstance-owned). */
	UFUNCTION(BlueprintPure, Category = "Airlock|Voyage")
	bool HasPendingVoyage() const { return PendingVoyageId.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Airlock|Voyage")
	bool GetPendingVoyage(FVoyageSpec& OutSpec) const;

	/** Server-only. Ends the pending voyage (called when the crew returns to the hub). */
	UFUNCTION(BlueprintCallable, Category = "Airlock|Voyage")
	void ClearPendingVoyage();

	void SetLastVoyageResult(const FVoyageResult& Result);
	bool GetLastVoyageResult(FVoyageResult& OutResult) const;
	/** The ship leg's tally, kept (not saved) between the refinery dock and the floor's end (M5-2). Server only. */
	void SetShipResult(const FVoyageResult& Result);
	bool GetShipResult(FVoyageResult& OutResult) const;
	void ClearShipResult();

	// --- The Rig's persistent stock (M3-7): a save-game slot per machine until M4 moves it server-side. ---
	FString SaveSlotName = TEXT("Airlock");
	/** Loads the slot (creating defaults when absent). Called by Initialize; tests call it after setting the slot. */
	void ReloadSave();
	/** Deletes the slot and restores the defaults in memory. */
	bool ResetSave();
	int32 GetBaseFuelTonnes() const;
	int32 GetRunsCompleted() const;
	void SetBaseFuelTonnes(int32 Tonnes);

	/** Days since the Unix epoch in UTC: the same on every machine on the same day. */
	static int32 TodaySeed();

private:
	TMap<FGuid, FVoyageSpec> ActiveVoyages;
	FGuid PendingVoyageId;
	FVoyageResult LastVoyageResult;
	bool bHasLastVoyageResult = false;
	FVoyageResult ShipResult;
	bool bHasShipResult = false;
	UPROPERTY(Transient) TObjectPtr<UAirlockSaveGame> Save;
	void SaveNow();
};
