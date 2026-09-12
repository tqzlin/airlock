// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockGameState.h"
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
}
