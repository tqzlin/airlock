// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "FuelLayouts.generated.h"

USTRUCT()
struct AIRLOCK_API FFuelFixtureSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FSoftObjectPath Mesh;
	UPROPERTY() float LabelYaw = 180.f;
	UPROPERTY() FVector Scale = FVector(1);
};
USTRUCT()
struct AIRLOCK_API FFuelLayout
{
	GENERATED_BODY()
	UPROPERTY() FName SectionId;
	UPROPERTY() FFuelFixtureSpec FeedValve;
	UPROPERTY() FFuelFixtureSpec BypassValve;
	UPROPERTY() FFuelFixtureSpec Intake;
};
namespace FuelLayouts { AIRLOCK_API const FFuelLayout* Find(FName SectionId); }
