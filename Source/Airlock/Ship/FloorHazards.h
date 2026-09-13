// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "FloorHazards.generated.h"

/** Rates for the crude on the refinery floor (M5-4a). Per second unless named otherwise. */
USTRUCT()
struct AIRLOCK_API FFloorHazardRates
{
	GENERATED_BODY()
	/** A puddle loses this many litres a second to the air. */
	UPROPERTY(EditAnywhere) float EvaporationLps = 1.f;
	/** What every evaporated litre adds to the room. */
	UPROPERTY(EditAnywhere) float PpmPerLitreEvaporated = 0.05f;
	UPROPERTY(EditAnywhere) float RadiusBaseCm = 50.f;
	UPROPERTY(EditAnywhere) float RadiusPerLitre = 0.75f;
	UPROPERTY(EditAnywhere) float RadiusMaxCm = 250.f;
	/** A spill this close to an existing puddle's centre joins it. */
	UPROPERTY(EditAnywhere) float MergeDistanceCm = 250.f;
	/** Feet more than this above or below the puddle are not in it (the end platforms stand 87 cm over the deck). */
	UPROPERTY(EditAnywhere) float ZToleranceCm = 60.f;
	/** The owner's rules for a pawn inside a puddle: walk speed factor, seconds of dwell per dropped cask, the hold. */
	UPROPERTY(EditAnywhere) float SlowFactor = 0.6f;
	UPROPERTY(EditAnywhere) float StumbleSeconds = 4.f;
	UPROPERTY(EditAnywhere) float SqueegeeSeconds = 6.f;
	/** Litres one completed squeegee hold removes. */
	UPROPERTY(EditAnywhere) float SqueegeeLitres = 300.f;
};

USTRUCT()
struct AIRLOCK_API FFloorSpillState
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() FName StillId;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() float Litres = 0.f;
	UPROPERTY() float RadiusCm = 0.f;
	bool operator==(const FFloorSpillState& Other) const;
};

USTRUCT()
struct AIRLOCK_API FFloorHazardState
{
	GENERATED_BODY()
	UPROPERTY() TArray<FFloorSpillState> Spills;
	/** Spill events accepted, litres they brought, litres taken off the deck by hand, casks dropped in the crude. */
	UPROPERTY() int32 SpillsTotal = 0;
	UPROPERTY() float LitresSpilled = 0.f;
	UPROPERTY() float LitresCleared = 0.f;
	UPROPERTY() int32 Stumbles = 0;
	bool operator==(const FFloorHazardState& Other) const;
};

/**
 * Pure puddles on the refinery floor (M5-4a): a spill or a pop puts litres on the deck at a point, the puddle's
 * radius follows its litres, it evaporates into the room and a squeegee hold takes it away. The owner (the game
 * mode) feeds it the model's Spill and Pop events, adds the ppm to the floor's air, asks `Contains` for each
 * pawn's feet and applies the slow / stumble rules itself. Deterministic, no UObject state; ids `S1, S2, ...`.
 */
class AIRLOCK_API FFloorHazards
{
public:
	FFloorHazardRates Rates;
	void Reset();
	/** Litres at Location by StillId: joins a puddle within MergeDistanceCm (litres add, the centre stays), else a
	 * new one. Returns the puddle's id; NAME_None (and no change) for a non-positive or non-finite amount. */
	FName Spill(FName StillId, const FVector& Location, float Litres);
	/** Evaporation over DeltaSeconds; OutPpm is what the room gains this call. True when the published state changed
	 * (a whole litre, a whole centimetre of radius, or a puddle gone). Non-finite or non-positive deltas do nothing. */
	bool Tick(float DeltaSeconds, float& OutPpm);
	/** One completed squeegee hold on a puddle: removes up to SqueegeeLitres; the puddle goes when it is dry. */
	bool Squeegee(FName Id);
	/** The puddle a point (a pawn's feet) stands in, by XY distance within ZToleranceCm; NAME_None on dry deck. */
	FName Contains(const FVector& Location) const;
	void CountStumble() { ++Floor.Stumbles; }
	float RadiusFor(float Litres) const;
	const FFloorHazardState& State() const { return Floor; }
	const FFloorSpillState* Find(FName Id) const;
	/** `No spills` or `Spills 2: S1 by Still_2 200 L r=200; S2 by Still_1 300 L r=250`. */
	FString Summary() const;
private:
	FFloorHazardState Floor;
	int32 NextId = 1;
	FFloorSpillState* MutableSpill(FName Id);
	static float Published(float Value) { return FMath::FloorToFloat(Value); }
};
