// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ship/RefineryLayouts.h"
#include "FloorFixture.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USceneComponent;

/**
 * A refinery-floor fixture (M5-2): a kit mesh with the cube fallback and a two-line label through the M3-8 policy.
 * The game mode spawns one per FFloorFixtureSpec (folder Floor). Stills are placeholders until M5-3's AStill.
 */
UCLASS()
class AIRLOCK_API AFloorFixture : public AActor
{
	GENERATED_BODY()
public:
	AFloorFixture();
	void Initialize(const FFloorFixtureSpec& InSpec);
	FName GetFixtureId() const { return Spec.Id; }
	EFloorFixtureKind GetKind() const { return Spec.Kind; }
	/** Server side: the label's second line (the tank's litres, a still's state). */
	void SetLine2(const FString& Line2);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> FixtureRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditDefaultsOnly, Category="Floor|Visual") float LabelSize = 22.f;
	UPROPERTY(EditDefaultsOnly, Category="Floor|Visual") FVector FallbackScale = FVector(1, 1, 1.5);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FFloorFixtureSpec Spec;
	UPROPERTY(ReplicatedUsing=OnRep_Line2) FString Line2Override;
	UFUNCTION() void OnRep_Spec();
	UFUNCTION() void OnRep_Line2();
	void Refresh();
	bool bVisualsReady = false;
};
