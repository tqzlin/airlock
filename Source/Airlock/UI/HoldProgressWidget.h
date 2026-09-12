// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HoldProgressWidget.generated.h"
class UTextBlock;
class UProgressBar;

UCLASS()
class AIRLOCK_API UHoldProgressWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void UpdateHold(AActor* Target, float Progress);
	UPROPERTY(EditDefaultsOnly, Category="Hold|Visual") FLinearColor FrameColour = FLinearColor(0.02f, 0.03f, 0.05f, 0.95f);
	UPROPERTY(EditDefaultsOnly, Category="Hold|Visual") FLinearColor TextColour = FLinearColor(0.8f, 0.95f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Hold|Visual") FLinearColor BarColour = FLinearColor(0.1f, 0.8f, 0.7f);
	UPROPERTY(EditDefaultsOnly, Category="Hold|Visual") FVector2D FrameSize = FVector2D(420.f, 86.f);
	UPROPERTY(EditDefaultsOnly, Category="Hold|Visual") int32 FontSize = 18;
	UPROPERTY(EditDefaultsOnly, Category="Hold|Visual") float BottomOffset = 64.f;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Prompt;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> Bar;
};
