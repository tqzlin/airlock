// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Ship/ShipAtmosphere.h"
#include "AirlockCharacter.generated.h"

class UCameraComponent;
struct FInputActionValue;
class UHoldProgressWidget;
class UCarryPromptWidget;
class UPromptWidget;
class UExposureWidget;
class ACargoCrate;
class ACargoSlot;
class ABypassValve;
class AFuelIntake;
class AStillControl;
class AAirlockGameState;
class AControlStation;

/**
 * First-person crew member. Capsule + camera only for now (no mesh); walks the station hub and ship sections.
 * Movement is server-authoritative through CharacterMovementComponent's built-in prediction.
 */
UCLASS()
class AIRLOCK_API AAirlockCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAirlockCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(Server, Reliable) void ServerBeginHold(AActor* Target);
	UFUNCTION(Server, Reliable) void ServerEndHold();
	AActor* GetHoldTarget() const { return HoldTarget; }
	float GetHoldProgress() const { return HoldProgress; }
	bool IsInHoldRange(const AActor* Target) const;
	UFUNCTION(Server, Reliable) void ServerSetScrubMode(EScrubMode Mode);
	bool SetScrubModeServer(EScrubMode Mode, bool bRequireReach);
	UPROPERTY(ReplicatedUsing=OnRep_Exposure) float Exposure = 0.f;
	UPROPERTY(ReplicatedUsing=OnRep_BlackedOut) bool bBlackedOut = false;
	UPROPERTY(Replicated) double BlackoutEndServerTime = 0.0;
	UFUNCTION() void OnRep_Exposure();
	UPROPERTY(ReplicatedUsing=OnRep_InSpill) FName InSpill;
	UFUNCTION() void OnRep_InSpill();
	UFUNCTION() void OnRep_BlackedOut();
	void ApplyWalkSpeed();
	void ApplyLocalMovement();
	bool SetExposureServer(float Percent);
	UFUNCTION(Server, Reliable) void ServerPickUp(ACargoCrate* Crate);
	UFUNCTION(Server, Reliable) void ServerPutDown();
	UFUNCTION(Server, Reliable) void ServerOpenBypass(ABypassValve* Valve);
	bool OpenBypassServer(ABypassValve* Valve, bool bRequireReach);
	UFUNCTION(Server, Reliable) void ServerFeedCask(AFuelIntake* Intake);
	UFUNCTION(Server, Reliable) void ServerLoadStill(AStillControl* Hopper);
	UFUNCTION(Server, Reliable) void ServerPlace(ACargoSlot* CargoSlot);
	UPROPERTY(ReplicatedUsing=OnRep_CarriedCrate) TObjectPtr<ACargoCrate> CarriedCrate;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> CarrySocket;
	UPROPERTY(EditDefaultsOnly, Category="Airlock|Carry") FVector CarryOffset = FVector(80, 0, -20);
	UPROPERTY(EditDefaultsOnly, Category="Airlock|Carry") float CarrySpeedFactor = .6f;
	/** Assigns from the cached baseline on authority and clients; safe to apply repeatedly. */
	UFUNCTION() void OnRep_CarriedCrate();
	AActor* TraceInteractTarget() const;
	/** The line under the crosshair (M5-5): what E does to Target now - nothing while a hold runs (the hold widget has
	 * the prompt), only a slot, the intake or a still's hopper while carrying (E on anything else puts the cask down),
	 * otherwise the target's own prompt. Pure. */
	static FText InteractPromptFor(AActor* Target, bool bCarrying, bool bHolding);

	// --- The control-room seat (Ship/ControlStation). Server-authoritative; E toggles, movement stops while seated. ---
	UFUNCTION(Server, Reliable) void ServerSitAt(AControlStation* Station);
	UFUNCTION(Server, Reliable) void ServerStandUp();
	AControlStation* GetSeatedStation() const { return SeatedStation; }
	bool IsSeated() const { return SeatedStation != nullptr; }

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** How far the interact trace reaches from the camera, in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Airlock|Interaction")
	float InteractReach;

protected:
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Interact(const FInputActionValue& Value);
	void EndInteract(const FInputActionValue& Value);

private:
	UPROPERTY(Replicated) TObjectPtr<AActor> HoldTarget;
	UPROPERTY(Replicated) float HoldProgress = 0.f;
	float HoldElapsed = 0.f;
	void ResetHold();
	UPROPERTY(Transient) TObjectPtr<UHoldProgressWidget> HoldWidget;
	UPROPERTY(Transient) TObjectPtr<UCarryPromptWidget> CarryWidget;
	UPROPERTY(Transient) TObjectPtr<UPromptWidget> PromptWidget;
	/** The last prompt shown, so a change logs `Prompt: <text>` once (proofs read it). */
	FString LastPrompt;
	UPROPERTY(Transient) TObjectPtr<UExposureWidget> ExposureWidget;
	FAtmosphereRates ExposureRates;
	float LastExposureForLog = 0.f;
	double LastCoughTime = -5.0;
	void TickCrewExposure(float DeltaSeconds);
	void TickCrewSpill(float DeltaSeconds);
	float SpillDwellSeconds = 0.f;
	float BaseWalkSpeed = 0.f;
	bool bCarrySpeedCached = false;
	void CacheCarrySpeed();
	FVector PutDownLocation() const;
	void DetachCarriedCrateAt(const FTransform& Transform);
	void CheckCarriedCargo();
	void BindCarriedCargo();
	TWeakObjectPtr<AAirlockGameState> ObservedCargoState;
	FDelegateHandle CargoChangedHandle;

	UPROPERTY(ReplicatedUsing = OnRep_SeatedStation) TObjectPtr<AControlStation> SeatedStation;
	UFUNCTION() void OnRep_SeatedStation();
	/** Server: releases the seat (if any) and restores walking; puts the pawn at the seat's exit point when bToExit. */
	void LeaveSeat(bool bToExit);
};
