// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Ship/ShipGraph.h"
#include "ShipLayouts.generated.h"

USTRUCT()
struct AIRLOCK_API FShipModuleSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() EShipModuleType Type = EShipModuleType::PowerCore;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FSoftObjectPath Mesh;
};

USTRUCT()
struct AIRLOCK_API FShipLineSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() EShipLineKind Kind = EShipLineKind::Coolant;
	UPROPERTY() FName From;
	UPROPERTY() FName To;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FSoftObjectPath Mesh;
};

USTRUCT()
struct AIRLOCK_API FShipLayout
{
	GENERATED_BODY()
	UPROPERTY() FName SectionId;
	UPROPERTY() TArray<FShipModuleSpec> Modules;
	UPROPERTY() TArray<FShipLineSpec> Lines;
};

namespace ShipLayouts
{
	AIRLOCK_API const FShipLayout* Find(FName SectionId);
}
