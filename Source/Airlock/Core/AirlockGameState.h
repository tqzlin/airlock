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

	/** Seed for today's departure board; every client derives the same three offers from it. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Voyage")
	int32 DaySeed;

	/** True once a voyage has been booked on this server (the dock airlock will let the crew through). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Airlock|Voyage")
	bool bVoyageBooked;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
