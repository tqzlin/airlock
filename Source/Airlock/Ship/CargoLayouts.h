// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "CargoLayouts.generated.h"

USTRUCT()
struct AIRLOCK_API FCargoSlotSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() bool bRack = false;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FSoftObjectPath Mesh;
	/** Applied to the pad mesh only (the actor stays at scale 1, so a placed crate keeps its size). */
	UPROPERTY() FVector MeshScale = FVector(1.f);
	UPROPERTY() float LabelYaw = 180.f;
};

USTRUCT()
struct AIRLOCK_API FCargoCrateSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() bool bVolatile = false;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FSoftObjectPath Mesh;
	UPROPERTY() float LabelYaw = 180.f;
};

USTRUCT()
struct AIRLOCK_API FCargoLayout
{
	GENERATED_BODY()
	UPROPERTY() FName SectionId;
	UPROPERTY() TArray<FCargoSlotSpec> Slots;
	UPROPERTY() TArray<FCargoCrateSpec> Crates;
};

namespace CargoLayouts
{
	/** Face the reader on the Y 1560 walkway, including spots on its centre line. */
	AIRLOCK_API float LabelYawFor(double Y);
	/** Today's four-cask, three-clamp manifest (what every proof books), or null for a section without a hold. */
	AIRLOCK_API const FCargoLayout* Find(FName SectionId);
	/** The manifest of a contract tier (M3-7): CaskCount casks (up to 12), every second one volatile up to VolatileCount, CaskCount * 3 / 4 clamps, one rack. */
	AIRLOCK_API FCargoLayout Build(FName SectionId, int32 CaskCount, int32 VolatileCount);
	/** The refinery floor's staging spots (M5-2): the run's intact casks by id in manifest order, the light ones volatile; no slots. */
	AIRLOCK_API FCargoLayout BuildFloor(const TArray<FName>& Casks, const TArray<FName>& LightCasks);
}
