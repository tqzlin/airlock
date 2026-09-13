// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "Ship/FloorFixture.h"
#include "Still.generated.h"
class UPointLightComponent;
class UParticleSystemComponent;

/** The existing floor fixture with walkway gauges fed only by the replicated refinery mirror. */
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
