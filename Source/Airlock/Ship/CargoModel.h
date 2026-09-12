// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Voyage/VoyageTypes.h"
#include "CargoModel.generated.h"

UENUM()
enum class ECargoState : uint8 { Loose, Carried, Secured, Racked, Vented };

USTRUCT()
struct AIRLOCK_API FCargoCrateState
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() bool bVolatile = false;
	UPROPERTY() ECargoState State = ECargoState::Loose;
	UPROPERTY() float Temperature = 0.f;
	UPROPERTY() FName SlotId;
	bool operator==(const FCargoCrateState& Other) const;
};

USTRUCT()
struct AIRLOCK_API FCargoSlotState
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() bool bRack = false;
	UPROPERTY() FName CrateId;
	bool operator==(const FCargoSlotState& Other) const;
};

USTRUCT()
struct AIRLOCK_API FCargoRates
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) float LooseHeat = 1.f;
	UPROPERTY(EditAnywhere) float CarriedHeat = 0.5f;
	UPROPERTY(EditAnywhere) float RackCool = 4.f;
	UPROPERTY(EditAnywhere) float WarnTemperature = 40.f;
	UPROPERTY(EditAnywhere) float VentTemperature = 100.f;
};

USTRUCT()
struct AIRLOCK_API FVoyageResult
{
	GENERATED_BODY()
	UPROPERTY() EVoyageType Type = EVoyageType::Mining;
	UPROPERTY() int32 Seed = 0;
	UPROPERTY() int32 CratesTotal = 0;
	UPROPERTY() int32 CratesIntact = 0;
	UPROPERTY() int32 CratesVented = 0;
	UPROPERTY() int32 CratesLoose = 0;
	UPROPERTY() float ShipCondition = 0.f;
	UPROPERTY() int32 Pay = 0;
	FString Summary() const;
};

struct FCargoLayout;
class FShipGraph;

/** Pure manifest and temperature model. World transforms and carrier identity belong to server actors. */
class AIRLOCK_API FCargoModel
{
public:
	explicit FCargoModel(const FCargoRates& InRates = FCargoRates()) : Rates(InRates) {}
	static constexpr int32 PayPerIntactCrate = 100;
	static constexpr int32 CleanManifestBonus = 150;
	static constexpr float HullBonusPerPoint = 2.f;
	void Build(const FCargoLayout& Layout);
	bool Tick(float DeltaSeconds, double VoyageElapsedSeconds);
	bool PickUp(FName CrateId);
	bool PutDown(FName CrateId);
	bool Place(FName CrateId, FName SlotId);
	bool Release(FName SlotId);
	bool SetTemperature(FName CrateId, float Temperature);
	const FCargoCrateState* FindCrate(FName Id) const;
	const TArray<FCargoCrateState>& Crates() const { return CrateStates; }
	const TArray<FCargoSlotState>& Slots() const { return SlotStates; }
	FString Summary() const;
	static const TCHAR* StateName(ECargoState State);
	static FVoyageResult Tally(const FCargoModel& Cargo, const FShipGraph& Ship, const FVoyageSpec& Spec);
private:
	FCargoRates Rates;
	TArray<FCargoCrateState> CrateStates;
	TArray<FCargoSlotState> SlotStates;
	FCargoCrateState* MutableCrate(FName Id);
	FCargoSlotState* MutableSlot(FName Id);
};
