// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockGameState.h"
#include "Ship/CargoLayouts.h"
#include "Ship/RefineryLayouts.h"
#include "Net/UnrealNetwork.h"

AAirlockGameState::AAirlockGameState()
{
	CurrentSectionId = TEXT("StationHub");
	SectionPopulationCap = 32;
	SectionPopulation = 0;
	DaySeed = 0;
	bVoyageBooked = false;
}

double AAirlockGameState::GetVoyageSecondsRemaining() const
{
	return bVoyageActive ? FMath::Max(0.0, VoyageEndServerTime - GetServerWorldTimeSeconds()) : 0.0;
}

void AAirlockGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAirlockGameState, CurrentSectionId);
	DOREPLIFETIME(AAirlockGameState, SectionPopulationCap);
	DOREPLIFETIME(AAirlockGameState, SectionPopulation);
	DOREPLIFETIME(AAirlockGameState, DaySeed);
	DOREPLIFETIME(AAirlockGameState, bVoyageBooked);
	DOREPLIFETIME(AAirlockGameState, VoyageEndServerTime);
	DOREPLIFETIME(AAirlockGameState, bVoyageActive);
	DOREPLIFETIME(AAirlockGameState, ShipModules);
	DOREPLIFETIME(AAirlockGameState, ShipLines);
	DOREPLIFETIME(AAirlockGameState, Cargo);
	DOREPLIFETIME(AAirlockGameState, Fuel);
	DOREPLIFETIME(AAirlockGameState, Refinery);
	DOREPLIFETIME(AAirlockGameState, FloorHazards);
	DOREPLIFETIME(AAirlockGameState, FloorCriticals);
	DOREPLIFETIME(AAirlockGameState, Atmosphere);
	DOREPLIFETIME(AAirlockGameState, PuffingCrates);
	DOREPLIFETIME(AAirlockGameState, LastVoyageResult);
	DOREPLIFETIME(AAirlockGameState, bHasLastVoyageResult);
	DOREPLIFETIME(AAirlockGameState, BaseFuelTonnes);
	DOREPLIFETIME(AAirlockGameState, RunsCompleted);
}

void AAirlockGameState::PublishShipGraph(const FShipGraph& Graph)
{
	if (!HasAuthority()) { return; }
	const bool bModulesChanged = ShipModules != Graph.Modules();
	const bool bLinesChanged = ShipLines != Graph.Lines();
	if (!bModulesChanged && !bLinesChanged) { return; }
	if (bModulesChanged) { ShipModules = Graph.Modules(); }
	if (bLinesChanged) { ShipLines = Graph.Lines(); }
	ForceNetUpdate();
	OnRep_ShipGraph();
}

void AAirlockGameState::OnRep_ShipGraph()
{
	OnShipGraphChanged.Broadcast();
}

void AAirlockGameState::PublishCargo(const TArray<FCargoCrateState>& InCargo)
{
	if (!HasAuthority() || Cargo == InCargo) { return; }
	Cargo = InCargo;
	ForceNetUpdate();
	OnRep_Cargo();
}

void AAirlockGameState::OnRep_Cargo() { OnShipGraphChanged.Broadcast(); }

bool AAirlockGameState::AirHasAtmosphere() const { return CargoLayouts::Find(CurrentSectionId) != nullptr; }

void AAirlockGameState::PublishAtmosphere(const FAtmosphereState& Air, const TArray<FName>& Puffing)
{
	if (!HasAuthority()) { return; }
	const bool bAirChanged = !(Atmosphere == Air);
	const bool bPuffsChanged = PuffingCrates != Puffing;
	if (!bAirChanged && !bPuffsChanged) { return; }
	if (bAirChanged) { Atmosphere = Air; }
	if (bPuffsChanged) { PuffingCrates = Puffing; }
	ForceNetUpdate();
	OnRep_Atmosphere();
}
void AAirlockGameState::OnRep_Atmosphere() { OnShipGraphChanged.Broadcast(); }

void AAirlockGameState::PublishFuel(const FFuelState& InFuel)
{
	if (!HasAuthority() || Fuel == InFuel) { return; }
	Fuel = InFuel; ForceNetUpdate(); OnRep_Fuel();
}
void AAirlockGameState::OnRep_Fuel() { OnShipGraphChanged.Broadcast(); }

void AAirlockGameState::PublishRefinery(const FRefineryState& InRefinery)
{
	if (!HasAuthority() || Refinery == InRefinery) { return; }
	Refinery = InRefinery; ForceNetUpdate(); OnRep_Refinery();
}
void AAirlockGameState::OnRep_Refinery() { OnShipGraphChanged.Broadcast(); }
void AAirlockGameState::PublishFloorHazards(const FFloorHazardState& InHazards)
{
	if (!HasAuthority() || FloorHazards == InHazards) { return; }
	FloorHazards = InHazards; ForceNetUpdate(); OnRep_FloorHazards();
}
void AAirlockGameState::OnRep_FloorHazards() { OnShipGraphChanged.Broadcast(); }
bool AAirlockGameState::RefineryHasFloor() const { return RefineryLayouts::Find(CurrentSectionId) != nullptr; }
