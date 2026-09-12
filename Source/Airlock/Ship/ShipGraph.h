// Copyright Airlock project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ShipGraph.generated.h"

UENUM()
enum class EShipModuleType : uint8 { PowerCore, CoolantLoop, LifeSupport, CargoClamps, Engines, Nav, Galley };
UENUM()
enum class EShipModuleStatus : uint8 { Nominal, Degraded, Failed };
UENUM()
enum class EShipLineKind : uint8 { Coolant, Power };
UENUM()
enum class EShipLineFault : uint8 { None, Leak, Short };

USTRUCT()
struct AIRLOCK_API FShipModuleState
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() EShipModuleType Type = EShipModuleType::PowerCore;
	UPROPERTY() float Condition = 100.f;
	UPROPERTY() EShipModuleStatus Status = EShipModuleStatus::Nominal;
	bool operator==(const FShipModuleState& Other) const;
};

USTRUCT()
struct AIRLOCK_API FShipLineState
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() EShipLineKind Kind = EShipLineKind::Coolant;
	UPROPERTY() FName FromModule;
	UPROPERTY() FName ToModule;
	UPROPERTY() EShipLineFault Fault = EShipLineFault::None;
	bool operator==(const FShipLineState& Other) const;
};

USTRUCT()
struct AIRLOCK_API FShipGraphRates
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) float LeakDrain = 1.f;
	UPROPERTY(EditAnywhere) float ShortDrain = 1.f;
	UPROPERTY(EditAnywhere) float CoreDriftDegraded = 0.5f;
	UPROPERTY(EditAnywhere) float CoreDriftFailed = 1.5f;
	UPROPERTY(EditAnywhere) float BrownoutDegraded = 0.25f;
	UPROPERTY(EditAnywhere) float BrownoutFailed = 0.75f;
};

struct FShipLayout;

/** Pure model. Propagation uses statuses at the start of each tick; no implicit recovery. */
class AIRLOCK_API FShipGraph
{
public:
	explicit FShipGraph(const FShipGraphRates& InRates = FShipGraphRates()) : Rates(InRates) {}
	void Build(const FShipLayout& Layout);
	TArray<FName> Tick(float DeltaSeconds);
	bool SetLineFault(FName LineId, EShipLineFault Fault);
	bool DamageModule(FName Id, float Amount);
	bool RepairModule(FName Id, float Amount);
	const FShipModuleState* FindModule(FName Id) const;
	const FShipLineState* FindLine(FName Id) const;
	const TArray<FShipModuleState>& Modules() const { return ModuleStates; }
	const TArray<FShipLineState>& Lines() const { return LineStates; }
	FString Summary() const;
	static EShipModuleStatus StatusFor(float Condition);
	static const TCHAR* StatusName(EShipModuleStatus Status);
	static const TCHAR* FaultName(EShipLineFault Fault);
private:
	FShipGraphRates Rates;
	TArray<FShipModuleState> ModuleStates;
	TArray<FShipLineState> LineStates;
	bool AdjustModule(FName Id, float Amount, bool bRepair);
};
