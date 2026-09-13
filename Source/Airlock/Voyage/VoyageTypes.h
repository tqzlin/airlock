// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoyageTypes.generated.h"

/** The three contract tiers of the refining run (M3-7): the same ~20-minute NPC-captained trip in three sizes. */
UENUM(BlueprintType)
enum class EVoyageType : uint8
{
	MilkRun		UMETA(DisplayName = "Milk run"),
	Standard	UMETA(DisplayName = "Standard run"),
	Priority	UMETA(DisplayName = "Priority run")
};

/** Everything needed to spin up one voyage instance. Deterministic from (Type, Seed). */
USTRUCT(BlueprintType)
struct AIRLOCK_API FVoyageSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	EVoyageType Type = EVoyageType::MilkRun;

	/** Seed that drives every random decision in the voyage; identical seeds replay identically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	int32 Seed = 0;

	/** Target voyage length. MVP fixes this at 20 minutes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	float DurationSeconds = 1200.f;

	/** Which ship layout the voyage runs on (a level / set of sections). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	FName ShipClassId;

	/** NPC captain running the ship. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	FName Captain;

	/** Player slots on this voyage; also the ship's life-support cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	int32 CrewCapacity = 6;

	/** Casks in the hold for this contract (4 / 8 / 12) and how many of them are volatile (2 / 4 / 6). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	int32 CaskCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	int32 VolatileCount = 2;
};
