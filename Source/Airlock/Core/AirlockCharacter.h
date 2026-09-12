// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AirlockCharacter.generated.h"

class UCameraComponent;
struct FInputActionValue;
class UHoldProgressWidget;

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

};
