// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockGameState.h"
#include "Net/UnrealNetwork.h"

AAirlockGameState::AAirlockGameState()
{
	CurrentSectionId = TEXT("StationHub");
	SectionPopulationCap = 32;
	SectionPopulation = 0;
}

void AAirlockGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAirlockGameState, CurrentSectionId);
	DOREPLIFETIME(AAirlockGameState, SectionPopulationCap);
	DOREPLIFETIME(AAirlockGameState, SectionPopulation);
}
