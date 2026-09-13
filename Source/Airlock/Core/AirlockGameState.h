// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Ship/ShipGraph.h"
#include "Ship/CargoModel.h"
#include "Ship/ShipAtmosphere.h"
#include "Ship/FuelSystem.h"
#include "AirlockGameState.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnShipGraphChanged);

/**
 * Replicated state for the ship section (or station hub) this server instance is simulating.
 * Life support caps how many players a section can hold; the airlock refuses entry when it is full.
 */
UCLASS()
class AIRLOCK_API AAirlockGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AAirlockGameState();

	/** Identifier of the section this world represents, e.g. "StationHub" or "Hauler_Mk1/CargoBay". */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Section")
	FName CurrentSectionId;

	/** Maximum players life support can sustain in this section. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Section")
	int32 SectionPopulationCap;

	/** Players currently in this section (server-maintained). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Section")
	int32 SectionPopulation;

	UFUNCTION(BlueprintPure, Category = "Airlock|Section")
	bool HasLifeSupportCapacity() const { return SectionPopulation < SectionPopulationCap; }

	/** Seed for today's departure board; every client derives the same three offers from it. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Voyage")
	int32 DaySeed;

	/** True once a voyage has been booked on this server (the dock airlock will let the crew through). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Voyage")
	bool bVoyageBooked;

	/** Absolute server world time of docking; zero outside an active voyage. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Voyage")
	double VoyageEndServerTime = 0.0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Voyage")
	bool bVoyageActive = false;

	/** Clients derive the countdown from the replicated deadline and synchronized server time. */
	UFUNCTION(BlueprintPure, Category = "Airlock|Voyage")
	double GetVoyageSecondsRemaining() const;


	UPROPERTY(ReplicatedUsing = OnRep_ShipGraph)
	TArray<FShipModuleState> ShipModules;

	UPROPERTY(ReplicatedUsing = OnRep_ShipGraph)
	TArray<FShipLineState> ShipLines;

	FOnShipGraphChanged OnShipGraphChanged;

	UPROPERTY(ReplicatedUsing=OnRep_Cargo) TArray<FCargoCrateState> Cargo;
	UPROPERTY(Replicated) FVoyageResult LastVoyageResult;
	UPROPERTY(Replicated) bool bHasLastVoyageResult = false;
	void PublishCargo(const TArray<FCargoCrateState>& InCargo);
	UFUNCTION() void OnRep_Cargo();
	UPROPERTY(ReplicatedUsing=OnRep_Atmosphere) FAtmosphereState Atmosphere;
	UPROPERTY(ReplicatedUsing=OnRep_Atmosphere) TArray<FName> PuffingCrates;
	void PublishAtmosphere(const FAtmosphereState& Air, const TArray<FName>& Puffing);
	UFUNCTION() void OnRep_Atmosphere();
	bool AirHasAtmosphere() const;
	UPROPERTY(ReplicatedUsing=OnRep_Fuel) FFuelState Fuel;
	void PublishFuel(const FFuelState& InFuel);
	UFUNCTION() void OnRep_Fuel();

	/** Authority-only mirror; unchanged arrays are never rewritten. */
	void PublishShipGraph(const FShipGraph& Graph);

	UFUNCTION()
	void OnRep_ShipGraph();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
