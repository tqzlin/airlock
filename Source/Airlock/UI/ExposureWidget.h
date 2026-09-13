// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExposureWidget.generated.h"
class UBorder;
class UTextBlock;

UCLASS()
class AIRLOCK_API UExposureWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void UpdateExposure(float Exposure, bool bBlackedOut, double EndServerTime, double ServerTime);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY(Transient) TObjectPtr<UBorder> Tint;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;
};
