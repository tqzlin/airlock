// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "FuelSystem.generated.h"

USTRUCT()
struct AIRLOCK_API FFuelRates
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) float Capacity = 100.f;
	UPROPERTY(EditAnywhere) float StartFuel = 90.f;
	UPROPERTY(EditAnywhere) float NominalBurnPerMinute = 4.f;
	UPROPERTY(EditAnywhere) float LeakPerSecond = 40.f / 60.f;
	UPROPERTY(EditAnywhere) float CaskFuel = 15.f;
	UPROPERTY(EditAnywhere) float CaskBurnSeconds = 20.f;
	UPROPERTY(EditAnywhere) float SootPpmPerCask = 15.f;
	UPROPERTY(EditAnywhere) float FoulingPerCask = 10.f;
	UPROPERTY(EditAnywhere) float FoulingPerCaskFouled = 20.f;
	UPROPERTY(EditAnywhere) int32 CasksPerFouling = 2;
	UPROPERTY(EditAnywhere) float LowFuelPercent = 30.f;
	UPROPERTY(EditAnywhere) float CriticalFuelPercent = 15.f;
	UPROPERTY(EditAnywhere) float LateDockSeconds = 120.f;
};

USTRUCT()
struct AIRLOCK_API FFuelState
{
	GENERATED_BODY()
	UPROPERTY() float Units = 90.f;
	UPROPERTY() bool bFeedLeak = false;
	UPROPERTY() bool bBypassOpen = false;
	UPROPERTY() FName BurningCrate;
	UPROPERTY() int32 BurnSecondsLeft = 0;
	UPROPERTY() int32 CasksBurned = 0;
	UPROPERTY() bool bInjectorsFouled = false;
	UPROPERTY() bool bExhausted = false;
	bool operator==(const FFuelState& Other) const;
};

/** Pure fuel and burn clocks. State keeps precise units; publication floors them. */
class AIRLOCK_API FFuelSystem
{
public:
	explicit FFuelSystem(const FFuelRates& InRates = FFuelRates()) : Rates(InRates) { Reset(); }
	void Reset();
	bool Tick(float DeltaSeconds);
	void OpenLeak() { Fuel.bFeedLeak = true; }
	bool CloseLeak();
	bool OpenBypass();
	bool FeedCask(FName CrateId);
	bool ClearInjectors();
	bool SetUnits(float Units);
	float SootPerSecond() const;
	float FoulingOnFeed() const;
	float SecondsOfFuelLeft() const;
	bool IsShort(float VoyageSecondsRemaining) const { return SecondsOfFuelLeft() < VoyageSecondsRemaining; }
	const FFuelState& State() const { return Fuel; }
	FString Summary() const;
private:
	FFuelRates Rates;
	FFuelState Fuel;
	double PreciseUnits = 90;
	double BurnRemaining = 0;
};
