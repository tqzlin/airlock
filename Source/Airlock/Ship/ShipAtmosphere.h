// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "ShipAtmosphere.generated.h"

struct FCargoCrateState;

UENUM()
enum class EScrubMode : uint8 { Off, Normal, Hazard };

USTRUCT()
struct AIRLOCK_API FAtmosphereRates
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) float PuffRate = 2.f;
	UPROPERTY(EditAnywhere) float PuffSeconds = 10.f;
	UPROPERTY(EditAnywhere) float PuffPeriodSeconds = 20.f;
	UPROPERTY(EditAnywhere) float RuptureRate = 5.f;
	UPROPERTY(EditAnywhere) float RuptureSeconds = 30.f;
	UPROPERTY(EditAnywhere) float ScrubNormal = 0.5f;
	UPROPERTY(EditAnywhere) float ScrubHazard = 3.f;
	UPROPERTY(EditAnywhere) float BrownoutFactor = 0.5f;
	UPROPERTY(EditAnywhere) float WarnPpm = 20.f;
	UPROPERTY(EditAnywhere) float DangerPpm = 50.f;
	UPROPERTY(EditAnywhere) float CleanPpm = 10.f;
	UPROPERTY(EditAnywhere) float MaxPpm = 500.f;
	UPROPERTY(EditAnywhere) float ExposureRateAtWarn = 1.f;
	UPROPERTY(EditAnywhere) float ExposureRecovery = 0.5f;
	UPROPERTY(EditAnywhere) float SlowAtExposure = 60.f;
	UPROPERTY(EditAnywhere) float SlowFactor = 0.7f;
	UPROPERTY(EditAnywhere) float BlackoutSeconds = 30.f;
	UPROPERTY(EditAnywhere) float ExposureAfterBlackout = 50.f;
};

USTRUCT()
struct AIRLOCK_API FAtmosphereState
{
	GENERATED_BODY()
	UPROPERTY() float Ppm = 0.f;
	UPROPERTY() EScrubMode ScrubMode = EScrubMode::Normal;
	UPROPERTY() bool bBrownout = false;
	UPROPERTY() bool bScrubDisabled = false;
	UPROPERTY() int32 ActiveSources = 0;
	bool operator==(const FAtmosphereState& Other) const;
};

/** Pure section air model. Clocks are local to the model; only the resulting state and FX ids are published. */
class AIRLOCK_API FShipAtmosphere
{
public:
	explicit FShipAtmosphere(const FAtmosphereRates& InRates = FAtmosphereRates()) : Rates(InRates) {}
	void Reset();
	bool SetScrubMode(EScrubMode Mode);
	bool SetBrownout(bool bBrownout);
	bool SetScrubDisabled(bool bDisabled);
	bool SetPpm(float Ppm);
	bool AddPpm(float Ppm);
	bool Tick(float DeltaSeconds, const TArray<FCargoCrateState>& Cargo);
	float GetPpm() const { return Air.Ppm; }
	const FAtmosphereState& State() const { return Air; }
	bool IsPuffing(FName Id) const { return Puffing.Contains(Id); }
	TArray<FName> PuffingCrates() const { return Puffing; }
	FString Summary() const;
	static const TCHAR* ModeName(EScrubMode Mode);
	static float TickExposure(float Exposure, float Ppm, float DeltaSeconds, const FAtmosphereRates& Rates);
private:
	FAtmosphereRates Rates;
	FAtmosphereState Air;
	TMap<FName, double> PuffClocks;
	// Finished bursts remain here until the crate vanishes: a vented crate bursts only once.
	TMap<FName, double> RuptureClocks;
	TArray<FName> Puffing;
};
