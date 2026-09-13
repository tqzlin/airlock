// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Voyage/VoyageTypes.h"
#include "CargoModel.generated.h"

UENUM()
/** Loaded (M5-3a): in a still's hopper on the refinery floor - off the deck like Burned, but crude that will be refined. */
enum class ECargoState : uint8 { Loose, Carried, Secured, Racked, Vented, Burned, Loaded };

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
	UPROPERTY() EVoyageType Type = EVoyageType::MilkRun;
	UPROPERTY() int32 Seed = 0;
	UPROPERTY() int32 CratesTotal = 0;
	UPROPERTY() int32 CratesIntact = 0;
	UPROPERTY() int32 CratesVented = 0;
	UPROPERTY() int32 CratesLoose = 0;
	UPROPERTY() int32 CratesBurned = 0;
	UPROPERTY() float ShipCondition = 0.f;
	UPROPERTY() int32 Pay = 0;
	UPROPERTY() float DockPpm = -1.f;
	UPROPERTY() int32 Blackouts = 0;
	UPROPERTY() bool bCleanAir = false;
	/** Refined methane the refinery credits for the intact casks, and what is left after the trip home (M3-7). */
	UPROPERTY() int32 FuelCredit = 0;
	UPROPERTY() int32 FuelSurplus = 0;
	UPROPERTY() bool bLateDock = false;
	/** Preserves pre-fuel summary strings when Tally is called without fuel data. */
	UPROPERTY() bool bHasFuelData = false;
	/** The ship's fuel units at the dock (M5-3a; -1 without fuel data): the floor's diesel refuels what is missing first. */
	UPROPERTY() float FuelUnitsAtDock = -1.f;
	/** The intact casks by id in manifest order, and which of them are light (M5-2): what the refinery floor receives. */
	UPROPERTY() TArray<FName> IntactCasks;
	UPROPERTY() TArray<FName> IntactLightCasks;
	/** The floor leg (M5-2; M5-3 fills these from the refinery model's FFloorResult). */
	UPROPERTY() bool bHasFloor = false;
	UPROPERTY() int32 FloorCasksTotal = 0;
	UPROPERTY() int32 FloorCasksRefined = 0;
	UPROPERTY() int32 FloorCasksUnrefined = 0;
	UPROPERTY() int32 FloorDieselL = 0;
	UPROPERTY() int32 FloorPay = 0;
	FString Summary() const;
};

struct FCargoLayout;
struct FAtmosphereState;
struct FFuelState;
class FShipGraph;

/** Pure manifest and temperature model. World transforms and carrier identity belong to server actors. */
class AIRLOCK_API FCargoModel
{
public:
	explicit FCargoModel(const FCargoRates& InRates = FCargoRates()) : Rates(InRates) {}
	static constexpr int32 PayPerIntactCrate = 100;
	static constexpr int32 CleanAirBonus = 150;
	static constexpr int32 BlackoutDock = 50;
	static constexpr int32 SootFine = 40;
	static constexpr int32 LateDockFine = 100;
	static constexpr int32 CleanManifestBonus = 150;
	static constexpr float HullBonusPerPoint = 2.f;
	static constexpr int32 FuelCreditPerIntactCask = 15;
	static constexpr int32 SurplusPerIntactCask = 10;
	void Build(const FCargoLayout& Layout);
	bool Tick(float DeltaSeconds, double VoyageElapsedSeconds);
	void SetRackCoolFactor(float Factor);
	bool PickUp(FName CrateId);
	bool PutDown(FName CrateId);
	bool Consume(FName CrateId);
	/** A cask into a still's hopper (M5-3a): Loose or Carried -> Loaded. */
	bool Load(FName CrateId);
	/** Burned or Loaded: gone from the deck, inert, hidden by the crate actor. */
	static bool IsConsumed(ECargoState State) { return State == ECargoState::Burned || State == ECargoState::Loaded; }
	bool Place(FName CrateId, FName SlotId);
	bool Release(FName SlotId);
	bool SetTemperature(FName CrateId, float Temperature);
	const FCargoCrateState* FindCrate(FName Id) const;
	const TArray<FCargoCrateState>& Crates() const { return CrateStates; }
	const TArray<FCargoSlotState>& Slots() const { return SlotStates; }
	FString Summary() const;
	static const TCHAR* StateName(ECargoState State);
	static FVoyageResult Tally(const FCargoModel& Cargo, const FShipGraph& Ship, const FVoyageSpec& Spec,
		const FAtmosphereState* Air = nullptr, int32 Blackouts = 0, const FFuelState* Fuel = nullptr);
private:
	FCargoRates Rates;
	float RackCoolFactor = 1.f;
	TArray<FCargoCrateState> CrateStates;
	TArray<FCargoSlotState> SlotStates;
	FCargoCrateState* MutableCrate(FName Id);
	FCargoSlotState* MutableSlot(FName Id);
};
