// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "RefineryModel.generated.h"

UENUM()
enum class EStillStatus : uint8 { Idle, Running, Popped, Flash, Failed };

UENUM()
enum class ERefineryKnob : uint8 { Heat, Feed, Vent };

UENUM()
enum class ERefineryEvent : uint8 { BatchDone, Spill, Pop, Flash, Fouled, Failed };

USTRUCT()
struct AIRLOCK_API FRefineryEvent
{
	GENERATED_BODY()
	UPROPERTY() ERefineryEvent Type = ERefineryEvent::BatchDone;
	UPROPERTY() FName StillId;
	UPROPERTY() FName CaskId;
	UPROPERTY() float Litres = 0.f;
	UPROPERTY() float DieselL = 0.f;
	UPROPERTY() float GasL = 0.f;
	UPROPERTY() float TarL = 0.f;
};

USTRUCT()
struct AIRLOCK_API FRefineryRates
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) float CaskLitres = 1000.f;
	UPROPERTY(EditAnywhere) float MaxFeedLps = 20.f;
	UPROPERTY(EditAnywhere) float HeatBaseK = 92.f;
	UPROPERTY(EditAnywhere) float HeatSpanK = 260.f;
	UPROPERTY(EditAnywhere) float HeatTauSeconds = 10.f;
	UPROPERTY(EditAnywhere) float FeedCoolKPerL = 0.12f;
	UPROPERTY(EditAnywhere) float HeavyIdealK = 183.f;
	UPROPERTY(EditAnywhere) float LightIdealK = 157.f;
	UPROPERTY(EditAnywhere) float BellHalfWidthK = 22.f;
	UPROPERTY(EditAnywhere) float YieldMax = 0.85f;
	UPROPERTY(EditAnywhere) float GasBase = 0.05f;
	UPROPERTY(EditAnywhere) float GasSlope = 0.6f;
	UPROPERTY(EditAnywhere) float GasStartK = 225.f;
	UPROPERTY(EditAnywhere) float GasSpanK = 60.f;
	UPROPERTY(EditAnywhere) float BoilStartK = 130.f;
	UPROPERTY(EditAnywhere) float PressureGain = 0.002f;
	UPROPERTY(EditAnywhere) float PressureFloorBar = 1.f;
	UPROPERTY(EditAnywhere) float VentBleedPerS = 0.3f;
	UPROPERTY(EditAnywhere) float TrapLimit = 0.6f;
	UPROPERTY(EditAnywhere) float RoomPpmPerVentExcess = 40.f;
	UPROPERTY(EditAnywhere) float RoomPpmPerGasL = 0.05f;
	UPROPERTY(EditAnywhere) float WarnBar = 2.5f;
	UPROPERTY(EditAnywhere) float PopBar = 3.f;
	UPROPERTY(EditAnywhere) float PopPpm = 60.f;
	UPROPERTY(EditAnywhere) float PopSpillL = 300.f;
	UPROPERTY(EditAnywhere) float FlashK = 320.f;
	UPROPERTY(EditAnywhere) float FlashRoomPpmMin = 40.f;
	UPROPERTY(EditAnywhere) float FlashPpm = 40.f;
	UPROPERTY(EditAnywhere) float FlashConditionHit = 30.f;
	UPROPERTY(EditAnywhere) float CokeHotK = 290.f;
	UPROPERTY(EditAnywhere) float CokeHotRate = 0.6f;
	UPROPERTY(EditAnywhere) float CokeDryRate = 0.3f;
	UPROPERTY(EditAnywhere) float CokeDryHeat = 0.5f;
	UPROPERTY(EditAnywhere) float FouledCoke = 60.f;
	UPROPERTY(EditAnywhere) float FailedCoke = 100.f;
	UPROPERTY(EditAnywhere) float RepairCoke = 40.f;
	UPROPERTY(EditAnywhere) float CalDriftPerS = 0.004f;
	UPROPERTY(EditAnywhere) float CalWarn = 0.8f;
	UPROPERTY(EditAnywhere) float FloodFeed = 0.8f;
	UPROPERTY(EditAnywhere) float FloodAfterSeconds = 5.f;
	UPROPERTY(EditAnywhere) float FloodSpillL = 200.f;
	UPROPERTY(EditAnywhere) float FloodCoolK = 30.f;
	UPROPERTY(EditAnywhere) int32 HopperCasks = 3;
	UPROPERTY(EditAnywhere) int32 KnobSteps = 20;
	UPROPERTY(EditAnywhere) float MaxStepSeconds = 0.25f;
	UPROPERTY(EditAnywhere) float SlotSeconds = 600.f;
	UPROPERTY(EditAnywhere) float DieselCrPerL = 0.5f;
	UPROPERTY(EditAnywhere) int32 CrudeSaleCr = 40;
	UPROPERTY(EditAnywhere) float TarFeeCrPer100L = 10.f;
	UPROPERTY(EditAnywhere) int32 MedicalFineCr = 200;
	UPROPERTY(EditAnywhere) float RefuelUnitsPerL = 0.1f;
	UPROPERTY(EditAnywhere) float BaseFuelTonnesPer100L = 1.f;
};

USTRUCT()
struct AIRLOCK_API FStillState
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() EStillStatus Status = EStillStatus::Idle;
	UPROPERTY() int32 HeatSteps = 0;
	UPROPERTY() int32 FeedSteps = 0;
	UPROPERTY() int32 VentSteps = 11;
	UPROPERTY() float TempK = 92.f;
	UPROPERTY() float PressureBar = 1.f;
	UPROPERTY() float Calibration = 1.f;
	UPROPERTY() float Coke = 0.f;
	UPROPERTY() bool bFouled = false;
	UPROPERTY() float Condition = 100.f;
	UPROPERTY() float ChargeL = 0.f;
	UPROPERTY() FName CurrentCask;
	UPROPERTY() bool bCurrentLight = false;
	UPROPERTY() TArray<FName> Hopper;
	UPROPERTY() float BatchDieselL = 0.f;
	UPROPERTY() float BatchGasL = 0.f;
	UPROPERTY() float BatchTarL = 0.f;
	UPROPERTY() int32 BatchesDone = 0;
	UPROPERTY() float FloodSeconds = 0.f;
	bool operator==(const FStillState& Other) const;
};

USTRUCT()
struct AIRLOCK_API FRefineryCask
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() bool bLight = false;
	bool operator==(const FRefineryCask& Other) const;
};

USTRUCT()
struct AIRLOCK_API FRefineryState
{
	GENERATED_BODY()
	UPROPERTY() TArray<FStillState> Stills;
	UPROPERTY() TArray<FRefineryCask> Waiting;
	UPROPERTY() int32 CasksDelivered = 0;
	UPROPERTY() float TankDieselL = 0.f;
	UPROPERTY() float TotalGasL = 0.f;
	UPROPERTY() float TotalTarL = 0.f;
	UPROPERTY() float SpilledL = 0.f;
	UPROPERTY() int32 CasksRefined = 0;
	UPROPERTY() int32 CasksLost = 0;
	UPROPERTY() int32 Spills = 0;
	UPROPERTY() int32 Pops = 0;
	UPROPERTY() int32 Flashes = 0;
	bool operator==(const FRefineryState& Other) const;
};

USTRUCT()
struct AIRLOCK_API FFloorResult
{
	GENERATED_BODY()
	UPROPERTY() int32 CasksTotal = 0;
	UPROPERTY() int32 CasksRefined = 0;
	UPROPERTY() int32 CasksUnrefined = 0;
	UPROPERTY() int32 CasksLost = 0;
	UPROPERTY() float DieselL = 0.f;
	UPROPERTY() float GasL = 0.f;
	UPROPERTY() float TarL = 0.f;
	UPROPERTY() int32 Spills = 0;
	UPROPERTY() int32 Pops = 0;
	UPROPERTY() int32 Flashes = 0;
	UPROPERTY() int32 Criticals = 0;
	UPROPERTY() float RefuelUnits = 0.f;
	UPROPERTY() float SurplusL = 0.f;
	UPROPERTY() int32 BaseFuelTonnes = 0;
	UPROPERTY() int32 Pay = 0;
	FString Summary() const;
};

/** Pure batch refinery. Precise litres stay in State; the owner applies tick events and room ppm. */
class AIRLOCK_API FRefineryModel
{
public:
	FRefineryRates Rates;
	void Build(int32 StillCount, const TArray<FRefineryCask>& Casks);
	void Rebuild();
	const FRefineryState& State() const { return Floor; }
	const FStillState* FindStill(FName Id) const;
	float BatchProgress(FName Id) const;
	bool StepKnob(FName Still, ERefineryKnob Knob, int32 Steps);
	bool SetKnob(FName Still, ERefineryKnob Knob, int32 Steps);
	bool LoadHopper(FName Still, FName Cask);
	bool ClearPop(FName Still);
	bool Purge(FName Still);
	bool Scrape(FName Still);
	bool Repair(FName Still);
	bool Calibrate(FName Still);
	// Resets OutRoomPpm, appends to OutEvents. RoomPpm is a fixed input throughout all slices.
	bool Tick(float DeltaSeconds, float RoomPpm, float& OutRoomPpm, TArray<FRefineryEvent>& OutEvents);
	FFloorResult Tally(int32 Criticals, float ShipFuelUnitsMissing) const;
	FString Summary() const;
	FString StillReadout(FName Still) const;
private:
	FRefineryState Floor;
	int32 BuildStillCount = 0;
	TArray<FRefineryCask> BuildCasks;
	FStillState* MutableStill(FName Id);
	void TickStill(FStillState& Still, float Dt, float RoomPpm, float& OutRoomPpm, TArray<FRefineryEvent>& OutEvents);
};
