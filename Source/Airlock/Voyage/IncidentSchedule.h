// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Voyage/VoyageTypes.h"
#include "IncidentSchedule.generated.h"

struct FShipLayout;

UENUM()
enum class EIncidentType : uint8 { CoolantLeak, PowerShort, ModuleDamage };

USTRUCT()
struct AIRLOCK_API FIncidentSpec
{
	GENERATED_BODY()
	UPROPERTY() float AtSeconds = 0.f;
	UPROPERTY() EIncidentType Type = EIncidentType::CoolantLeak;
	UPROPERTY() FName TargetId;
	UPROPERTY() float Magnitude = 0.f;
	UPROPERTY() FText CaptainLine;
};

/** Pure, seeded pacing. No world, timers, or mutable random stream shared with other systems. */
struct AIRLOCK_API FIncidentSchedule
{
	static constexpr float CalmSeconds = 150.f;
	static constexpr float DockSeconds = 60.f;
	static constexpr int32 BaseIncidents = 4;
	static constexpr int32 MaxIncidents = 8;
	static constexpr float MinGapSeconds = 90.f;
	static constexpr float ClimaxMinGapSeconds = 10.f;
	static constexpr float ClimaxMaxGapSeconds = 20.f;
	static constexpr float ClimaxStartFraction = 0.78f;
	static constexpr float ClimaxEndFraction = 0.85f;
	static constexpr float MinDamage = 30.f;
	static constexpr float MaxDamage = 50.f;
	static constexpr float ShortWindowThreshold = CalmSeconds + DockSeconds + 30.f;
	static constexpr float ShortWindowStartFraction = 0.25f;
	static constexpr float ShortWindowEndFraction = 0.75f;
	static constexpr float ReferenceDuration = 1200.f;
	static constexpr int32 SeedSalt = 0x1AC1DE17;

	static TArray<FIncidentSpec> Build(const FVoyageSpec& Spec, const FShipLayout& Layout, int32 CrewCount);
	/** Returns the end of the due range [NextEntry, result); a backward move never rewinds the cursor. */
	static int32 FindNextUnfired(const TArray<FIncidentSpec>& Entries, int32 NextEntry, double ElapsedSeconds);
	static TArray<FName> Targets(const FShipLayout& Layout, EIncidentType Type);
	static FText MakeCaptainLine(EIncidentType Type, FName Target, FName Captain);
	static const TCHAR* TypeName(EIncidentType Type);
};
