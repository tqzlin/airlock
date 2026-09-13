// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Voyage/VoyageSchedule.h"
#include "Voyage/VoyageSetPieces.h"
#include "Ship/ShipGraph.h"
#include "Ship/CargoModel.h"
#include "Ship/ShipAtmosphere.h"
#include "Ship/CargoLayouts.h"
#include "Ship/FuelSystem.h"
#include "Ship/RefineryModel.h"
#include "Ship/RefineryLayouts.h"
#include "AirlockGameMode.generated.h"

class AAirlockDoor;
class UIncidentDirectorComponent;

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

	/** M4-1: every console helper's Server RPC asks this first. True while `airlock.DevCheats` is non-zero. */
	virtual bool AllowCheats(APlayerController* Player) override;

	/** M4-1: a start that already has a pawn on it sends the next arrival `ArrivalSpacing` cm to the side (alternating), never a spectator. */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	/** Sideways step between crew arriving at one PlayerStart (a capsule is 84 cm across). */
	static constexpr float ArrivalSpacing = 120.f;

	/** Section id of the station hub; a door targeting it always travels (and ends the pending voyage). */
	static const FName HubSectionId;

	/** Server-side console helper; only adjusts the voyage captured when this section started. */
	void SetVoyageSecondsRemaining(float SecondsRemaining);

	bool ShipSetLineFault(FName Id, EShipLineFault Fault);
	bool ShipRepairLine(FName Id);
	bool ShipDamageModule(FName Id, float Amount);
	bool ShipRepairModule(FName Id, float Amount);
	FString ShipSummary() const;
	bool CargoPickUp(FName Id);
	bool CargoPutDown(FName Id);
	bool CargoPlace(FName Crate, FName Slot);
	bool CargoRelease(FName Slot);
	bool CargoSetTemperature(FName Id, float Temperature);
	FName CargoFirstFilledClamp() const;
	FVoyageResult CargoTally() const;
	FString CargoSummary() const;
	void RecordVoyageResult();
	bool FuelHasSystem() const;
	bool FuelOpenLeak();
	bool FuelCloseLeak();
	bool FuelOpenBypass();
	bool FuelFeedCask(FName Id);
	bool FuelClearInjectors();
	bool FuelSet(float Units);
	FString FuelSummary() const;
	void FuelLeakConsole();
	bool AirHasAtmosphere() const;
	bool AirSetScrubMode(EScrubMode Mode);
	bool AirSetPpm(float Ppm);
	EScrubMode AirGetScrubMode() const;
	FString AirSummary() const;
	void NotifyBlackout(APawn* Pawn);
	// --- The refinery model on the floor (M5-3a): server API for the console helpers and M5-3b's actors ---
	/** True on the section whose fixture table has stills (the floor). */
	bool RefineryHasFloor() const;
	/** A cask on the floor (Loose or Carried) into a still's hopper: the crate becomes Loaded and hides. */
	bool RefineryLoadHopper(FName Still, FName Cask);
	bool RefinerySetKnob(FName Still, ERefineryKnob Knob, int32 Steps);
	bool RefineryStepKnob(FName Still, ERefineryKnob Knob, int32 Delta);
	/** A lever hold completing: Reset (a popped still), Purge (a flashed one), Scrape, Repair, Calibrate. */
	bool RefineryLever(FName Still, ERefineryLever Lever);
	/** The floor tally now (records nothing): blackouts on the floor are its criticals, the ship result's fuel gap is refuelled first. */
	FFloorResult RefineryTally() const;
	FString RefinerySummary() const;
	FString RefineryStillReadout(FName Still) const;
	const FRefineryModel& GetRefineryModel() const { return RefineryModel; }
	int32 GetBlackoutCount() const { return BlackoutCount; }
	UIncidentDirectorComponent* GetIncidentDirector() const { return IncidentDirector; }

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
	UPROPERTY(Transient)
	TObjectPtr<UIncidentDirectorComponent> IncidentDirector;

	FShipGraph ShipGraph;
	FCargoModel CargoModel;
	FShipAtmosphere AtmosphereModel;
	FFuelSystem FuelModel;
	void BuildFuel();
	void TickFuel();
	void PublishFuel();
	bool UpdateAtmosphereFlags();
	void PublishAtmosphere();
	TMap<FName, double> AirCaptainLastSpoken;
	int32 BlackoutCount = 0;
	TMap<TWeakObjectPtr<APawn>, double> CountedBlackouts;
	void SpeakAirCaptain(FName Cue, const FText& Pattern, APawn* Pawn = nullptr);
	void BuildCargo();
	void PublishCargo();
	/** The manifest this section spawned: the booked tier's (M3-7) or the four-cask default. */
	FCargoLayout ActiveCargoLayout;
	void BuildStationSigns();
	FTimerHandle ShipGraphTimer;
	void BuildShipGraph();
	void TickShipGraph();
	void PublishShipGraph();

	bool bTravelPending = false;
	/** True for a level entered through an airlock: arrivals spawn at the section PlayerStart, never at a PIE start. */
	bool bArrivedByAirlock = false;
	FTimerHandle TravelTimer;

	FVoyageSpec ActiveVoyage;
	bool bStartedVoyageClock = false;
	bool bRecordedVoyageResult = false;
	TArray<FVoyageAnnouncement> CaptainSchedule;
	int32 NextCaptainEntry = 0;
	TArray<FVoyageSetPiece> SetPieces;
	int32 NextSetPiece = 0;
	FTimerHandle FuelFollowupTimer;
	bool bFuelFollowupPending = false;
	/** The captain calls for crude once per voyage, the first tick the fuel goes short (review decision, M3-6). */
	bool bFuelShortSpoken = false;
	void FireDueSetPieces(double Elapsed);
	void FireSetPiece(const FVoyageSetPiece& Entry, double Elapsed);
	void FuelFollowup();
	void SpeakVoyageLine(const FText& Line);
	FTimerHandle VoyageTimer;
	TSoftObjectPtr<UWorld> HubLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/StationHub.StationHub")));

	void ScheduleNextVoyageEvent();
	void TickVoyageClock();
	void ReturnToHub();

	// --- The refinery floor (M5-2) ---
	/** The ship section of the pending voyage (its ShipClassId): docking there goes to the floor, not the hub. */
	bool IsShipSection() const;
	bool IsFloorSection() const;
	/** The ship's docking: keep the ship tally on the subsystem and travel to the floor with the pending voyage intact. */
	void DockAtFloor();
	void RecordShipResult();
	void BuildFloorFixtures();
	/** `Captain <Name>` on the ship, `Okafor` on the floor: the log prefix and the caption speaker. */
	FString SpeakerName() const;
	// --- The refinery model on the floor (M5-3a) ---
	FRefineryModel RefineryModel;
	bool bFirstBatchSpoken = false;
	void BuildRefinery();
	void TickRefinery();
	void PublishRefinery();
	void ApplyRefineryEvents(const TArray<FRefineryEvent>& Events);
	/** Okafor's event lines with a 20 s cooldown per cue (the same map as the air lines). */
	void SpeakFloorLine(FName Cue, const FText& Line);
	TSoftObjectPtr<UWorld> FloorLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Maps/RefineryFloor.RefineryFloor")));
};
