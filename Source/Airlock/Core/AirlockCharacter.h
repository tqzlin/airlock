// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AirlockCharacter.generated.h"

class UCameraComponent;
struct FInputActionValue;

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
};
