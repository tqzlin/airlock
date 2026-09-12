// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/Interactable.h"
#include "GameFramework/Actor.h"
#include "VoyageBoard.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * The hub's departure board. Interact (E) opens the voyage list on the interacting player's controller.
 * Visual: a kit wall panel on a simple stand (soft ref, primitive fallback like AAirlockDoor).
 * Origin = floor under the stand; the panel faces +X.
 */
UCLASS()
class AIRLOCK_API AVoyageBoard : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AVoyageBoard();

	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditDefaultsOnly, Category = "Airlock|Kit")
	TSoftObjectPtr<UStaticMesh> PanelMesh;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<USceneComponent> BoardRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> Stand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> Panel;

	virtual void BeginPlay() override;

private:
	void ApplyMeshes();
};
