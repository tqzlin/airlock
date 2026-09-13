// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/Interactable.h"
#include "Ship/ShipLayouts.h"
#include "ControlStation.generated.h"

class UBoxComponent;
class UPointLightComponent;
class USceneComponent;
class UTextRenderComponent;

/**
 * The control-room seat (design doc 8.3: roles are seats, not classes). Spawned by the game mode from the ship
 * layout at the bridge terminal; press E to take it, E again to stand. One occupant at a time, server-owned and
 * replicated; the NPC captain "has the conn" while nobody sits. Sitting opens the control board (the full ship
 * readout) on the occupant's client. Origin = the floor point of the seat; the actor's forward = where the seated
 * crew member looks.
 */
UCLASS()
class AIRLOCK_API AControlStation : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AControlStation();
	void Initialize(const FControlStationSpec& InSpec, FName InCaptain);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FName GetStationId() const { return Spec.Id; }
	APawn* GetOccupant() const { return Occupant; }
	FName GetCaptain() const { return Captain; }
	/** Floor point and facing for a seated pawn; where a standing pawn is put down. */
	FVector GetSeatLocation() const { return GetActorLocation(); }
	FRotator GetSeatRotation() const { return GetActorRotation(); }
	FVector GetExitLocation() const { return GetActorLocation() - GetActorForwardVector() * Spec.ExitDistance; }

	/** Authority only. Claims the seat for the pawn; false when someone else holds it. */
	bool ServerSeat(APawn* Pawn);
	/** Authority only. Releases the seat if the pawn holds it. */
	void ServerVacate(APawn* Pawn);

	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> StationRoot;
	/** Blocks the interact trace only; never blocks pawns. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Marker;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> SeatLight;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FVector MarkerExtent = FVector(40.f, 60.f, 70.f);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float MarkerHeight = 90.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LabelSize = 22.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LabelHeight = 200.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FColor LabelColour = FColor(180, 220, 255);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LightHeight = 150.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LightRadius = 220.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") float LightIntensity = 120.f;
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FLinearColor FreeColour = FLinearColor(0.2f, 0.6f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Ship|Visual") FLinearColor OccupiedColour = FLinearColor(1.f, 0.6f, 0.1f);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FControlStationSpec Spec;
	UPROPERTY(Replicated) FName Captain;
	UPROPERTY(ReplicatedUsing=OnRep_Occupant) TObjectPtr<APawn> Occupant;
	UFUNCTION() void OnRep_Spec();
	UFUNCTION() void OnRep_Occupant();
	void Refresh();
	bool bVisualsReady = false;
};
