// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ship/RefineryLayouts.h"
#include "FloorFixture.generated.h"

class AAirlockGameState;
class UStaticMeshComponent;
class UTextRenderComponent;
class USceneComponent;

/**
 * A refinery-floor fixture (M5-2): a kit mesh with the cube fallback and a two-line label through the M3-8 policy.
 * The game mode spawns one per FFloorFixtureSpec (folder Floor). Since M5-3a a still's and the tank's second line
 * come from the replicated refinery state on both sides (`StateLine`); `SetLine2` remains for the other kinds.
 * Stills are placeholders until M5-3b's AStill.
 */
UCLASS()
class AIRLOCK_API AFloorFixture : public AActor
{
	GENERATED_BODY()
public:
	AFloorFixture();
	void Initialize(const FFloorFixtureSpec& InSpec);
	FName GetFixtureId() const { return Spec.Id; }
	const FFloorFixtureSpec& GetSpec() const { return Spec; }
	EFloorFixtureKind GetKind() const { return Spec.Kind; }
	/** Server side: the label's second line for fixtures the refinery state does not describe (the bunk, the drain). */
	void SetLine2(const FString& Line2);
	/** The still's or the tank's second line from a refinery state: `C1 42% 184K 1.4bar`, `POPPED`, `740 L`; empty for other kinds. */
	static FString StateLine(const FFloorFixtureSpec& Spec, const FRefineryState& Refinery);
	/** What the label shows now (tests read it back). */
	FString CurrentLabel() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> FixtureRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(EditDefaultsOnly, Category="Floor|Visual") float LabelSize = 22.f;
	UPROPERTY(EditDefaultsOnly, Category="Floor|Visual") FVector FallbackScale = FVector(1, 1, 1.5);

protected:
	virtual void Refresh();
	TWeakObjectPtr<AAirlockGameState> FloorState;
	bool bVisualsReady = false;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FFloorFixtureSpec Spec;
	UPROPERTY(ReplicatedUsing=OnRep_Line2) FString Line2Override;
	UFUNCTION() void OnRep_Spec();
	UFUNCTION() void OnRep_Line2();
	/** Binds the game state's change event (retried by timer until the state has replicated). */
	void BindState();
	FDelegateHandle StateChangedHandle;
	FTimerHandle BindTimer;
};
