// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ship/ShipLayouts.h"
#include "Core/Interactable.h"
#include "ShipLine.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UParticleSystemComponent;
class USceneComponent;
class AAirlockGameState;

UCLASS()
class AIRLOCK_API AShipLine : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AShipLine();
	void Initialize(const FShipLineSpec& InSpec);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ShipRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LabelSize = 30.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LabelHeight = 210.f;

	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FColor LabelColour = FColor::White;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FVector FallbackScale = FVector(1, 1, 1.5);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LightHeight = 120.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LightRadius = 250.f;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UParticleSystemComponent> LeakFX;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> ShortLight;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FSoftObjectPath SteamTemplate = FSoftObjectPath(TEXT("/Game/ModSciInteriors/Particles/PS_SteamDoor.PS_SteamDoor"));
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FLinearColor ShortColour = FLinearColor(1.f, 0.25f, 0.02f);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float ShortIntensity = 400.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float FlickerSeconds = 0.15f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Interaction") float LeakHoldSeconds = 3.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Interaction") float ShortHoldSeconds = 4.f;
	virtual void Interact_Implementation(APawn* Pawn) override {}
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual float GetInteractHoldSeconds_Implementation() const override;
	virtual void OnHoldComplete_Implementation(APawn* Pawn) override;
	EShipLineFault GetFault() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FShipLineSpec Spec;
	UFUNCTION() void OnRep_Spec();
	void BindGraph();
	void Refresh();
	bool bVisualsReady = false;
	TWeakObjectPtr<AAirlockGameState> GraphState;
	FDelegateHandle GraphChangedHandle;
	FTimerHandle BindTimer;
	FTimerHandle LeakRestartTimer;
	FTimerHandle FlickerTimer;
	bool bShortLit = false;
	EShipLineFault DisplayedFault = EShipLineFault::None;
	void Flicker();
	UFUNCTION() void OnLeakFinished(UParticleSystemComponent* FinishedSystem);
	void RestartLeak();
};
