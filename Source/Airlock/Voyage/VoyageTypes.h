// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoyageTypes.generated.h"

/** The three MVP voyage archetypes. Each is an instanced ~20-minute trip on an NPC-captained ship. */
UENUM(BlueprintType)
enum class EVoyageType : uint8
{
	Mining		UMETA(DisplayName = "Mining run"),
	Pirates		UMETA(DisplayName = "Pirate interdiction"),
	Derelict	UMETA(DisplayName = "Derelict salvage")
};

/** Everything needed to spin up one voyage instance. Deterministic from (Type, Seed). */
USTRUCT(BlueprintType)
struct AIRLOCK_API FVoyageSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voyage")
	EVoyageType Type = EVoyageType::Mining;

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
};
