// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AirlockCharacter.generated.h"

class UCameraComponent;
struct FInputActionValue;
class UHoldProgressWidget;
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

	UPROPERTY(ReplicatedUsing = OnRep_SeatedStation) TObjectPtr<AControlStation> SeatedStation;
	UFUNCTION() void OnRep_SeatedStation();
	/** Server: releases the seat (if any) and restores walking; puts the pawn at the seat's exit point when bToExit. */
	void LeaveSeat(bool bToExit);
	void ApplySeatedMovement(bool bSeated);
};
