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

	FTimerHandle ArrivalFadeTimer;

	UFUNCTION()
	void HandleVoyageChosen(EVoyageType Type);

	UFUNCTION()
	void HandleBoardClosed();
};
