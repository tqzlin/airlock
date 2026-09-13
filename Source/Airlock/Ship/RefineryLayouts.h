// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "RefineryLayouts.generated.h"

/** What a floor fixture stands for; M5-3 replaces the Still fixtures with working stills. */
UENUM()
enum class EFloorFixtureKind : uint8 { Still, Tank, Drain, Bunk };

/** One code-spawned fixture on the refinery floor: a kit mesh and a two-line label (M5-2). */
USTRUCT()
struct AIRLOCK_API FFloorFixtureSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() EFloorFixtureKind Kind = EFloorFixtureKind::Still;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FSoftObjectPath Mesh;
	/** World yaw the label faces (the M3-8 policy). */
	UPROPERTY() float LabelYaw = 180.f;
	UPROPERTY() float LabelHeight = 230.f;
	UPROPERTY() FString Line1;
	UPROPERTY() FString Line2;
};

USTRUCT()
struct AIRLOCK_API FRefineryLayout
{
	GENERATED_BODY()
	UPROPERTY() FName SectionId;
	UPROPERTY() TArray<FFloorFixtureSpec> Fixtures;
};

namespace RefineryLayouts
{
	/** The floor's fixture table; null for every other section. Positions are probed like the hold's. */
	AIRLOCK_API const FRefineryLayout* Find(FName SectionId);
	/** The one section that is the floor. */
	AIRLOCK_API FName FloorSectionId();
}
