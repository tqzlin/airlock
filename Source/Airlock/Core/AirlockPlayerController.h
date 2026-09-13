// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Voyage/VoyageTypes.h"
#include "AirlockPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UVoyageBoardWidget;
class UCaptainLineWidget;
class UTaskBoardWidget;
class UControlBoardWidget;

/**
 * Player controller that owns the Enhanced Input setup. The mapping context and actions are built
 * in C++ (no input assets to maintain) and registered with the local player on BeginPlay.
 * The pawn binds handlers to these actions in SetupPlayerInputComponent.
 */
UCLASS()
class AIRLOCK_API AAirlockPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAirlockPlayerController();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 2D axis: X = strafe (A/D), Y = forward (W/S). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** 2D axis from the mouse: X = yaw, Y = pitch. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock|Input")
	TObjectPtr<UInputAction> LookAction;

	/** Boolean: E / gamepad face-bottom. Used for airlocks, consoles, voyage boards. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock|Input")
	TObjectPtr<UInputAction> InteractAction;

	// --- Voyage board (local UI; booking goes through the server) ---

	/** Shows the departure list built from the replicated day seed. Local controllers only. */
	UFUNCTION(BlueprintCallable, Category = "Airlock|Voyage")
	void OpenVoyageBoard();

	UFUNCTION(BlueprintCallable, Category = "Airlock|Voyage")
	void CloseVoyageBoard();

	/** Asks the server to book today's voyage of the given type. The server rebuilds the spec itself. */
	UFUNCTION(Server, Reliable)
	void ServerChooseVoyage(EVoyageType Type);

	/** Server's answer to ServerChooseVoyage. */
	UFUNCTION(Client, Reliable)
	void ClientVoyageBooked(const FVoyageSpec& Spec, bool bAccepted);

	// --- Airlock transition card (GameInstance-owned Slate overlay that survives the travel) ---

	/** Fade to the black card; the server travels once the fade has had time to finish. */
	UFUNCTION(Client, Reliable)
	void ClientBeginTransition(const FText& Title, const FText& Subtitle);

	/** Fade the card away (called automatically shortly after arriving in a new level). */
	UFUNCTION(Client, Reliable)
	void ClientEndTransition();

	/** Server-timed captain transmission, displayed only on the owning client. */
	UFUNCTION(Client, Reliable)
	void ClientCaptainSays(const FText& Line);

	// --- Console helpers (Exec: callable from the ~ console; used by the agent's PIE smoke tests) ---

	/** Books today's voyage by type index (0 mining, 1 pirates, 2 derelict) without the UI. */
	UFUNCTION(Exec)
	void AirlockBook(int32 TypeIndex);

	/** Teleports the pawn (server-side). */
	UFUNCTION(Exec)
	void AirlockWarp(float X, float Y, float Z);

	/** Opens the departure board without looking at one. */
	UFUNCTION(Exec)
	void AirlockBoard();

	/** Logs the map, pawn location/rotation and view location (LogAirlock). */
	UFUNCTION(Exec)
	void AirlockWhere();

	/** Moves the active voyage clock on the server (clamped to its original duration). */
	UFUNCTION(Exec)
	void AirlockVoyageClock(float SecondsRemaining);

	UFUNCTION(Server, Reliable)
	void ServerSetVoyageClock(float SecondsRemaining);

	UFUNCTION(Server, Reliable)
	void ServerWarp(FVector Location);

	// Dev console RPCs, like the existing warp/clock helpers; gate before M4 multiplayer.
	UFUNCTION(Exec) void AirlockFault(FName LineId, FString Fault);
	UFUNCTION(Exec) void AirlockDamage(FName ModuleId, float Amount);
	UFUNCTION(Exec) void AirlockRepairModule(FName ModuleId, float Amount);
	UFUNCTION(Exec) void AirlockShipStatus();
	UFUNCTION(Exec) void AirlockHold(FString TargetLabel);
	UFUNCTION(Exec) void AirlockRelease();
	UFUNCTION(Server, Reliable) void ServerShipFault(FName LineId, const FString& Fault);
	UFUNCTION(Server, Reliable) void ServerAdjustShipModule(FName ModuleId, float Amount, bool bRepair);
	UFUNCTION(Server, Reliable) void ServerShipStatus();
	UFUNCTION(Server, Reliable) void ServerHoldByLabel(const FString& TargetLabel);
	UFUNCTION(Server, Reliable) void ServerReleaseHold();

	/** Developer incident injection; omitted target selects the first available layout entry. Gate before M4. */
	UFUNCTION(Exec) void AirlockIncident(FString Type, FName TargetId = NAME_None, float Magnitude = 40.f);
	UFUNCTION(Exec) void AirlockIncidentSchedule();
	UFUNCTION(Server, Reliable) void ServerIncident(const FString& Type, FName TargetId, float Magnitude);
	UFUNCTION(Server, Reliable) void ServerIncidentSchedule();

	UFUNCTION(Exec) void AirlockCargoStatus();
	UFUNCTION(Exec) void AirlockCarry(FName CrateId);
	UFUNCTION(Exec) void AirlockDrop();
	UFUNCTION(Exec) void AirlockPlace(FName SlotId);
	UFUNCTION(Exec) void AirlockCrateHeat(FName CrateId, float Temperature);
	UFUNCTION(Exec) void AirlockTally();
	UFUNCTION(Server, Reliable) void ServerCargoStatus();
	UFUNCTION(Server, Reliable) void ServerCarryById(FName CrateId);
	UFUNCTION(Server, Reliable) void ServerDropCargo();
	UFUNCTION(Server, Reliable) void ServerPlaceById(FName SlotId);
	UFUNCTION(Server, Reliable) void ServerCrateHeat(FName CrateId, float Temperature);
	UFUNCTION(Server, Reliable) void ServerCargoTally();
	UFUNCTION(Exec) void AirlockFuel();
	UFUNCTION(Exec) void AirlockFuelSet(float Units);
	UFUNCTION(Exec) void AirlockFuelLeak();
	UFUNCTION(Exec) void AirlockBypass();
	UFUNCTION(Exec) void AirlockFeed(FName CrateId);
	UFUNCTION(Server, Reliable) void ServerFuelStatus();
	UFUNCTION(Server, Reliable) void ServerFuelSet(float Units);
	UFUNCTION(Server, Reliable) void ServerFuelLeak();
	UFUNCTION(Server, Reliable) void ServerBypassFromConsole();
	UFUNCTION(Server, Reliable) void ServerFeedById(FName CrateId);
	UFUNCTION(Exec) void AirlockAir();
	UFUNCTION(Exec) void AirlockScrub(FString Mode);
	UFUNCTION(Exec) void AirlockAirSet(float Ppm);
	UFUNCTION(Exec) void AirlockExpose(float Percent);
	/** The Rig's fuel stock (M3-7): read it, set it for a proof, or delete the save and start over. */
	UFUNCTION(Exec) void AirlockBaseFuel();
	UFUNCTION(Exec) void AirlockBaseFuelSet(int32 Tonnes);
	UFUNCTION(Exec) void AirlockSaveReset();
	UFUNCTION(Server, Reliable) void ServerBaseFuel(int32 Mode, int32 Tonnes);
	UFUNCTION(Server, Reliable) void ServerAirStatus();
	UFUNCTION(Server, Reliable) void ServerScrubFromConsole(const FString& Mode);
	UFUNCTION(Server, Reliable) void ServerAirSet(float Ppm);
	UFUNCTION(Server, Reliable) void ServerExpose(float Percent);
	// The refinery floor's stills (M5-3a; the console until M5-3b's actors): `AirlockRefinery` (one line per still and
	// the tank), `AirlockKnob <Still> <Heat|Feed|Vent> <0..20>`, `AirlockKnobStep <Still> <Knob> <delta>`,
	// `AirlockStillLoad <Still> <Cask>` (no reach), `AirlockStill <Still> <Reset|Purge|Scrape|Repair|Calibrate>`.
	UFUNCTION(Exec) void AirlockRefinery();
	UFUNCTION(Exec) void AirlockKnob(FName StillId, FString Knob, int32 Steps);
	UFUNCTION(Exec) void AirlockKnobStep(FName StillId, FString Knob, int32 Delta);
	UFUNCTION(Exec) void AirlockStillLoad(FName StillId, FName CaskId);
	UFUNCTION(Exec) void AirlockStill(FName StillId, FString Lever);
	UFUNCTION(Server, Reliable) void ServerRefineryStatus();
	UFUNCTION(Server, Reliable) void ServerKnob(FName StillId, const FString& Knob, int32 Steps, bool bStep);
	UFUNCTION(Server, Reliable) void ServerStillLoad(FName StillId, FName CaskId);
	UFUNCTION(Server, Reliable) void ServerStillLever(FName StillId, const FString& Lever);

	/** Control-room seat by station id (same server path as the key; must be within reach) / stand up. */
	UFUNCTION(Exec) void AirlockSit(FName StationId);
	UFUNCTION(Exec) void AirlockStand();
	/** Turns the local view (control rotation) to a world yaw/pitch so a capture after a warp looks at something known. */
	UFUNCTION(Exec) void AirlockLook(float Yaw, float Pitch = 0.f);
	UFUNCTION(Server, Reliable) void ServerSitByStationId(FName StationId);
	UFUNCTION(Server, Reliable) void ServerStandUpFromConsole();

	/** M4-1: local capture of this instance's own game viewport (Slate and UMG included) to Saved/AgentCaptures/<Label>.png; not in Shipping. */
	UFUNCTION(Exec) void AirlockCapture(FString Label);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(Transient)
	TObjectPtr<UVoyageBoardWidget> BoardWidget;

	UPROPERTY(Transient)
	TObjectPtr<UCaptainLineWidget> CaptainWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTaskBoardWidget> TaskBoardWidget;

	/** The full ship readout, shown while the local pawn sits at a control station. */
	UPROPERTY(Transient)
	TObjectPtr<UControlBoardWidget> ControlBoardWidget;

	FTimerHandle ArrivalFadeTimer;

	/** Server side (M4-1). True while the game mode allows cheats (`airlock.DevCheats`); otherwise logs `Cheat <Helper> refused` and returns false. */
	bool CheatAllowed(const TCHAR* Helper);

	UFUNCTION()
	void HandleVoyageChosen(EVoyageType Type);

	UFUNCTION()
	void HandleBoardClosed();
};
