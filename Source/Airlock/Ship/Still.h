// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "Ship/FloorFixture.h"
#include "Still.generated.h"
class UPointLightComponent;
class UParticleSystemComponent;

/**
 * The existing floor fixture with walkway gauges fed only by the replicated refinery mirror. Since M5-5 the base
 * label is the still's name alone (`STILL 2`) and the three gauge lines live in world centimetres under it
 * (`GaugeWorldOffsets`, `GaugeWorldSize`), clear of the tank's 170 cm rim and of each other - the M5-3b stack
 * inherited the tank's 0.8 scale and ran into the base label's second line.
 */
UCLASS()
class AIRLOCK_API AStill : public AFloorFixture
{
	GENERATED_BODY()
public:
	AStill();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> BathGauge;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> KnobGauge;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> BatchGauge;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> PopLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UParticleSystemComponent> PopFX;
	/** Where the three gauge lines sit, in world centimetres from the still's origin on the deck: bath, knobs, batch. */
	static const FVector GaugeWorldOffsets[3];
	/** The gauge text height in world centimetres. */
	static constexpr float GaugeWorldSize = 18.f;
	/** The base label's height in world centimetres (over the spec's, which the M5-3b table set for a scaled label). */
	static constexpr float NameWorldHeight = 215.f;
protected:
	virtual void Refresh() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	EStillStatus LastStatus = EStillStatus::Idle;
	bool bHasLastStatus = false;
	bool bPopVisualsReady = false;
	FTimerHandle PopTimer;
	void EndPopBurst();
};
