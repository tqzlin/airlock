// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "Ship/FloorFixture.h"
#include "Still.generated.h"

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
protected:
	virtual void Refresh() override;
};
