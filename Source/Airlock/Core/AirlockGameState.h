// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "AirlockGameState.generated.h"

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
