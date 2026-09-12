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
};

USTRUCT()
struct AIRLOCK_API FCargoCrateSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() bool bVolatile = false;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FSoftObjectPath Mesh;
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
	AIRLOCK_API const FCargoLayout* Find(FName SectionId);
}
