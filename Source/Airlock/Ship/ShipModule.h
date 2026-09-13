// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ship/ShipLayouts.h"
#include "Core/Interactable.h"
#include "ShipModule.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class USceneComponent;
class AAirlockGameState;

UCLASS()
class AIRLOCK_API AShipModule : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AShipModule();
	void Initialize(const FShipModuleSpec& InSpec);
	FName GetModuleId() const { return Spec.Id; }
	EShipModuleType GetModuleType() const { return Spec.Type; }
	virtual void Interact_Implementation(APawn* Pawn) override;
	virtual float GetInteractHoldSeconds_Implementation() const override;
	virtual void OnHoldComplete_Implementation(APawn* Pawn) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ShipRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LabelSize = 22.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LabelHeight = 210.f;

	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FColor LabelColour = FColor::White;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FVector FallbackScale = FVector(1, 1, 1.5);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LightHeight = 120.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LightRadius = 100.f;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> StatusLight;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FLinearColor NominalColour = FLinearColor(0.05f, 1.f, 0.15f);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FLinearColor DegradedColour = FLinearColor(1.f, 0.5f, 0.02f);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FLinearColor FailedColour = FLinearColor(1.f, 0.02f, 0.01f);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LightIntensity = 12.f;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FShipModuleSpec Spec;
	UFUNCTION() void OnRep_Spec();
	void BindGraph();
	void Refresh();
	bool bVisualsReady = false;
	TWeakObjectPtr<AAirlockGameState> GraphState;
	FDelegateHandle GraphChangedHandle;
	FTimerHandle BindTimer;
};
