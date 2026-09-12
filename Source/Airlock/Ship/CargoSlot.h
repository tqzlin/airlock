// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interactable.h"
#include "Ship/CargoLayouts.h"
#include "CargoSlot.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class AAirlockGameState;

UCLASS()
class AIRLOCK_API ACargoSlot : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ACargoSlot();
	void Initialize(const FCargoSlotSpec& InSpec);
	FName GetSlotId() const { return Spec.Id; }
	FName GetCrateId() const;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* Pawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> CargoRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pad;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditDefaultsOnly, Category="Cargo|Placement") FVector CrateRestOffset = FVector(0, 0, 10);
	UPROPERTY(EditDefaultsOnly, Category="Cargo|Visual") FVector FallbackScale = FVector(1, 1, .1);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FCargoSlotSpec Spec;
	UFUNCTION() void OnRep_Spec();
	void BindCargo();
	void Refresh();
	bool bVisualsReady = false;
	TWeakObjectPtr<AAirlockGameState> CargoState;
	FDelegateHandle CargoChangedHandle;
	FTimerHandle BindTimer;
};
