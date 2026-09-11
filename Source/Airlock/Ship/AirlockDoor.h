// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AirlockDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAirlockTransitionRequested, APawn*, Pawn, FName, TargetSectionId);

/**
 * The boundary between two ship sections (or between the hub and a docked ship). Walking into the
 * trigger on the server requests a transition to TargetSectionId; the actual level travel and the
 * loading-screen hand-off are wired in a later step. Refuses entry when the target has no life support left.
 */
UCLASS()
class AIRLOCK_API AAirlockDoor : public AActor
{
	GENERATED_BODY()

public:
	AAirlockDoor();

	/** Section on the other side of this door. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	FName TargetSectionId;

	/** Level that hosts the target section (soft so sections stream independently). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	TSoftObjectPtr<UWorld> TargetLevel;

	/** Fired on the server when a pawn steps into the airlock. */
	UPROPERTY(BlueprintAssignable, Category = "Airlock")
	FOnAirlockTransitionRequested OnTransitionRequested;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UBoxComponent> TriggerVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> Frame;

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
};
