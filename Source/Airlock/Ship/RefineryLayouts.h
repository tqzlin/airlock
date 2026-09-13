// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Ship/RefineryModel.h"
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

/** The lever holds on a still (M5-3): each completes into one model verb. */
UENUM()
enum class ERefineryLever : uint8 { Reset, Purge, Scrape, Repair, Calibrate };

UENUM()
enum class EStillControl : uint8 { Hopper, HeatMinus, HeatPlus, FeedMinus, FeedPlus, VentMinus, VentPlus, Reset, Purge, Scrape, Repair, Calibrate };

/** Control offsets and marker dimensions are world centimetres, independent of the tank's scale. */
USTRUCT()
struct AIRLOCK_API FStillControlSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() FName StillId;
	UPROPERTY() EStillControl Kind = EStillControl::Hopper;
	UPROPERTY() FVector Offset = FVector::ZeroVector;
	UPROPERTY() float Yaw = 0.f;
	UPROPERTY() FSoftObjectPath Mesh;
	UPROPERTY() FVector Scale = FVector::OneVector;
	UPROPERTY() FString Label;
	UPROPERTY() FTransform Transform;
	UPROPERTY() float LabelYaw = -90.f;
};

namespace RefineryLayouts
{
	AIRLOCK_API TArray<FStillControlSpec> StillControls(const FFloorFixtureSpec& Still);
	/** The floor's fixture table; null for every other section. Positions are probed like the hold's. */
	AIRLOCK_API const FRefineryLayout* Find(FName SectionId);
	/** The one section that is the floor. */
	AIRLOCK_API FName FloorSectionId();
	/** How many Still fixtures the layout has; the model builds that many stills, `Still_1..N`, matching the fixture ids. */
	AIRLOCK_API int32 StillCount(const FRefineryLayout& Layout);
	/** Names for the log and the console (M5-3a): `heat` / `feed` / `vent`, the levers, the still statuses. */
	AIRLOCK_API const TCHAR* KnobName(ERefineryKnob Knob);
	AIRLOCK_API bool ParseKnob(const FString& Text, ERefineryKnob& OutKnob);
	AIRLOCK_API const TCHAR* LeverName(ERefineryLever Lever);
	/** The past tense the log prints when a lever completes: reset, purged, scraped, repaired, calibrated. */
	AIRLOCK_API const TCHAR* LeverDone(ERefineryLever Lever);
	AIRLOCK_API bool ParseLever(const FString& Text, ERefineryLever& OutLever);
	AIRLOCK_API const TCHAR* StatusName(EStillStatus Status);
}
