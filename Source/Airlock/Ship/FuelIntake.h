// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interactable.h"
#include "Ship/FuelLayouts.h"
#include "Ship/FuelSystem.h"
#include "FuelIntake.generated.h"
class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UParticleSystemComponent;
class AAirlockGameState;

UCLASS()
class AIRLOCK_API AFuelIntake : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AFuelIntake();
	void Initialize(const FFuelFixtureSpec& InSpec);
	const FFuelState* GetFuelState() const;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* Pawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> FuelRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FFuelFixtureSpec Spec;
	UFUNCTION() void OnRep_Spec();
	void BindFuel();
	void Refresh();
	bool bVisualsReady = false;
	TWeakObjectPtr<AAirlockGameState> ObservedState;
	FDelegateHandle ChangedHandle;
	FTimerHandle BindTimer;
};
