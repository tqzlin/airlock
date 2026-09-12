// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AirlockDoor.generated.h"

class AAirlockDoor;
class UBoxComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAirlockTransitionRequested, AAirlockDoor*, Door, APawn*, Pawn, FName, TargetSectionId);

/**
 * The boundary between two ship sections (or between the hub and a docked ship).
 *
 * Visuals follow the kit standard (Docs/KitStandard.md): the actor sits on the walking surface at the centre of
 * the opening. Doorway = SM_Doorway_Small_A (a 360-wide wall slot, offset -180 in Y so the opening is centred),
 * two SM_Doorway_Small_A3 frames on either face of the wall, and a SM_Door_Small_A leaf that slides up 268 cm.
 * Meshes are soft references so a clone without the Fab packs still gets a blocking primitive door.
 *
 * Behaviour: the server tracks pawns inside ProximityVolume and replicates bIsOpen; every machine animates the
 * leaf from that flag. Crossing the thin TransitionVolume at the threshold (server only) fires
 * OnTransitionRequested(Door, Pawn, TargetSectionId); the game mode turns that into a level travel.
 *
 * Locking: a dock door with bRequiresBooking stays shut (red indicator) until the game state says a voyage is booked;
 * bAlwaysLocked is a sealed hatch (the outer door of an airlock chamber). bIsLocked is server-computed and replicated.
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

	/** Fired on the server when a pawn crosses the threshold. */
	UPROPERTY(BlueprintAssignable, Category = "Airlock")
	FOnAirlockTransitionRequested OnTransitionRequested;

	/** Kit meshes (soft: missing packs degrade to a primitive door instead of failing to load). */
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|Kit")
	TSoftObjectPtr<UStaticMesh> DoorwayMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Airlock|Kit")
	TSoftObjectPtr<UStaticMesh> FrameMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Airlock|Kit")
	TSoftObjectPtr<UStaticMesh> LeafMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Airlock|Kit")
	TSoftObjectPtr<UStaticMesh> LeafGlassMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Airlock|Kit")
	TSoftObjectPtr<UStaticMesh> LeafPanelMesh;

	/** How far the leaf rises when open (kit: 268 cm). */
	UPROPERTY(EditAnywhere, Category = "Airlock|Motion", meta = (ClampMin = "0"))
	float OpenDistance = 268.f;

	/** Seconds for a full open or close. */
	UPROPERTY(EditAnywhere, Category = "Airlock|Motion", meta = (ClampMin = "0.05"))
	float OpenTime = 0.6f;

	/** Dock doors: stay locked until the crew has booked a voyage (AAirlockGameState::bVoyageBooked). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock|Lock")
	bool bRequiresBooking = false;

	/** Sealed hatch: never opens (e.g. the outer door of an airlock chamber before the ship is docked). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock|Lock")
	bool bAlwaysLocked = false;

	UPROPERTY(EditAnywhere, Category = "Airlock|Lock")
	FLinearColor LockedColour = FLinearColor(1.f, 0.04f, 0.02f);

	UPROPERTY(EditAnywhere, Category = "Airlock|Lock")
	FLinearColor UnlockedColour = FLinearColor(0.15f, 1.f, 0.35f);

	UFUNCTION(BlueprintPure, Category = "Airlock")
	bool IsOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintPure, Category = "Airlock")
	bool IsLocked() const { return bIsLocked; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<USceneComponent> DoorRoot;

	/** Wide box either side of the wall; a pawn inside keeps the door open. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UBoxComponent> ProximityVolume;

	/** Thin box in the wall plane; entering it requests the section transition. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UBoxComponent> TransitionVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> Doorway;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> FrameFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> FrameBack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> Leaf;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> LeafGlass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UStaticMeshComponent> LeafPanel;

	/** Small light above the opening: red = locked, green = free to pass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Airlock")
	TObjectPtr<UPointLightComponent> IndicatorLight;

	UPROPERTY(ReplicatedUsing = OnRep_IsOpen, VisibleInstanceOnly, Category = "Airlock")
	bool bIsOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsLocked, VisibleInstanceOnly, Category = "Airlock")
	bool bIsLocked = false;

	UFUNCTION()
	void OnRep_IsOpen();

	UFUNCTION()
	void OnRep_IsLocked();

	UFUNCTION()
	void OnProximityBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProximityEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnTransitionBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void BeginPlay() override;

private:
	/** Server-side bookkeeping of pawns inside ProximityVolume (weak: a destroyed pawn must not hold the door open). */
	TArray<TWeakObjectPtr<APawn>> PawnsNearby;

	/** 0 = closed, 1 = fully open. Cosmetic; driven by bIsOpen on every machine. */
	float SlideAlpha = 0.f;

	FVector LeafClosedLocation = FVector::ZeroVector;
	bool bUsingFallbackMeshes = false;

	FTimerHandle LockPollTimer;

	void ApplyMeshes();
	void SetOpen(bool bOpen);
	void UpdateLeaf();
	void RefreshLock();      // server: derive bIsLocked from the flags + game state
	void UpdateIndicator();  // everyone: colour the light from bIsLocked
};
