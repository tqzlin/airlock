// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ship/FloorHazards.h"
#include "Spill.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UParticleSystemComponent;
class AAirlockGameState;

/** A puddle's body; all litres, radius and location come from the replicated floor mirror. */
UCLASS()
class AIRLOCK_API ASpill : public AActor
{
	GENERATED_BODY()
public:
	ASpill();
	void Initialize(FName InId);
	FName GetSpillId() const { return Id; }
	const FFloorSpillState* GetSpillState() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SpillRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Marker;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UParticleSystemComponent> SteamFX;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Id) FName Id;
	UFUNCTION() void OnRep_Id();
	UFUNCTION() void OnSteamFinished(UParticleSystemComponent* Finished);
	void RestartSteam();
	void BindState();
	void Refresh();
	bool bPuddlePresent = false;
	TWeakObjectPtr<AAirlockGameState> ObservedState;
	FDelegateHandle ChangedHandle;
	FTimerHandle BindTimer;
	FTimerHandle SteamRestartTimer;
};
