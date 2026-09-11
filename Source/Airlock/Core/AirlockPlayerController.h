// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AirlockPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

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

protected:
	virtual void BeginPlay() override;
};
