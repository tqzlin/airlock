// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interactable.h"
#include "Ship/RefineryLayouts.h"
#include "StillControl.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class AAirlockGameState;

UCLASS()
class AIRLOCK_API AStillControl : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AStillControl();
	void Initialize(const FStillControlSpec& InSpec);
	FName GetStillId() const { return Spec.StillId; }
	EStillControl GetControlKind() const { return Spec.Kind; }
	const FStillControlSpec& GetSpec() const { return Spec; }
	const FStillState* GetStillState() const;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* Pawn) override;
	virtual float GetInteractHoldSeconds_Implementation() const override;
	virtual bool IsRepeatingHold_Implementation() const override;
	virtual FText GetHoldRefusal_Implementation() const override;
	virtual void OnHoldComplete_Implementation(APawn* Pawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ControlRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Marker;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FStillControlSpec Spec;
	UFUNCTION() void OnRep_Spec();
	void BindState();
	void Refresh();
	bool KnobAction(ERefineryKnob& Knob, int32& Step) const;
	bool LeverAction(ERefineryLever& Lever) const;
	int32 KnobValue(const FStillState& State) const;
	bool bVisualsReady = false;
	TWeakObjectPtr<AAirlockGameState> ObservedState;
	FDelegateHandle ChangedHandle;
	FTimerHandle BindTimer;
};
