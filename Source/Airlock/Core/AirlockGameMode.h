// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Voyage/VoyageSchedule.h"
#include "AirlockGameMode.generated.h"

class AAirlockDoor;

/**
 * Server-authoritative game mode for every Airlock level (station hub and ship sections alike).
 * Wires the project's pawn / controller / game state classes and enables seamless travel so
 * airlock transitions between sections don't drop the connection.
 */
UCLASS()
class AIRLOCK_API AAirlockGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAirlockGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Section id of the station hub; a door targeting it always travels (and ends the pending voyage). */
	static const FName HubSectionId;

	/** Server-side console helper; only adjusts the voyage captured when this section started. */
	void SetVoyageSecondsRemaining(float SecondsRemaining);

protected:
	/** Bound to every AAirlockDoor in the level. Decides whether the crossing becomes a level travel. */
	UFUNCTION()
	void HandleTransitionRequested(AAirlockDoor* Door, APawn* Pawn, FName TargetSectionId);

	/** Shared card, arrival flag and delayed travel path for door crossings and automatic docking. */
	void TravelToSection(const TSoftObjectPtr<UWorld>& Level, FName SectionId, const FText& Title, const FText& Subtitle);

	/** Seconds between telling clients to fade out and actually travelling. */
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|Transition")
	float TravelFadeDelay = 0.6f;

private:
	bool bTravelPending = false;
	/** True for a level entered through an airlock: arrivals spawn at the section PlayerStart, never at a PIE start. */
	bool bArrivedByAirlock = false;
	FTimerHandle TravelTimer;

	FVoyageSpec ActiveVoyage;
	TArray<FVoyageAnnouncement> CaptainSchedule;
	int32 NextCaptainEntry = 0;
	FTimerHandle VoyageTimer;
	TSoftObjectPtr<UWorld> HubLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/StationHub.StationHub")));

	void ScheduleNextVoyageEvent();
	void TickVoyageClock();
	void ReturnToHub();
};
