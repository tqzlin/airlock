// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interactable.h"
#include "Ship/CargoLayouts.h"
#include "Ship/CargoModel.h"
#include "CargoCrate.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UPointLightComponent;
class UParticleSystemComponent;
class AAirlockGameState;

UCLASS()
class AIRLOCK_API ACargoCrate : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ACargoCrate();
	void Initialize(const FCargoCrateSpec& InSpec);
	FName GetCargoId() const { return Spec.Id; }
	const FCargoCrateState* GetCargoState() const;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> CargoRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> HeatLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UParticleSystemComponent> VentFX;
	UPROPERTY(EditDefaultsOnly, Category="Cargo|Visual") FVector FallbackScale = FVector(.8, .8, .8);
	UPROPERTY(EditDefaultsOnly, Category="Cargo|Visual") float LabelHeight = 95.f;
	UPROPERTY(EditDefaultsOnly, Category="Cargo|Visual") float LabelSize = 22.f;
	UPROPERTY(EditDefaultsOnly, Category="Cargo|Visual") float HeatIntensity = 300.f;
	UPROPERTY(EditDefaultsOnly, Category="Cargo|Visual") FSoftObjectPath SteamTemplate = FSoftObjectPath(TEXT("/Game/ModSci_Engineer/Particles/PS_SteamDoor.PS_SteamDoor"));
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FCargoCrateSpec Spec;
	UFUNCTION() void OnRep_Spec();
	UFUNCTION() void OnVentFinished(UParticleSystemComponent* FinishedSystem);
	void RestartVent();
	void BindCargo();
	void Refresh();
	bool bVisualsReady = false;
	bool bVenting = false;
	TWeakObjectPtr<AAirlockGameState> CargoState;
	FDelegateHandle CargoChangedHandle;
	FTimerHandle BindTimer;
	FTimerHandle VentRestartTimer;
};
