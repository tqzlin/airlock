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
	/** World yaw the label text faces (towards the reader); 180 = readable from the -X side. */
	UPROPERTY() float LabelYaw = 180.f;
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
	/** World yaw the label text faces (towards the reader). */
	UPROPERTY() float LabelYaw = 180.f;
};

/** A control-room seat: Transform = the floor point a seated pawn stands on and the direction it faces. */
USTRUCT()
struct AIRLOCK_API FControlStationSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() FTransform Transform;
	/** How far behind the seat (against its forward) a pawn is put when it stands up. */
	UPROPERTY() float ExitDistance = 140.f;
	/** World yaw the label text faces (towards the approaching reader). */
	UPROPERTY() float LabelYaw = 180.f;
};

USTRUCT()
struct AIRLOCK_API FShipLayout
{
	GENERATED_BODY()
	UPROPERTY() FName SectionId;
	UPROPERTY() TArray<FShipModuleSpec> Modules;
	UPROPERTY() TArray<FShipLineSpec> Lines;
	UPROPERTY() TArray<FControlStationSpec> Stations;
};

namespace ShipLayouts
{
	AIRLOCK_API const FShipLayout* Find(FName SectionId);
}
