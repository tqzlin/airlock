// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PromptWidget.generated.h"
class UTextBlock;

/** The line under the crosshair (M5-5): what E does to the thing in reach, from `AAirlockCharacter::InteractPromptFor`. Collapsed when empty. */
UCLASS()
class AIRLOCK_API UPromptWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void UpdatePrompt(const FText& InPrompt);
	UPROPERTY(EditDefaultsOnly, Category="Prompt|Visual") FLinearColor FrameColour = FLinearColor(0.02f, 0.03f, 0.05f, 0.85f);
	UPROPERTY(EditDefaultsOnly, Category="Prompt|Visual") FLinearColor TextColour = FLinearColor(0.85f, 0.95f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category="Prompt|Visual") int32 FontSize = 16;
	/** Pixels below the screen centre, clear of the crosshair. */
	UPROPERTY(EditDefaultsOnly, Category="Prompt|Visual") float BelowCentre = 56.f;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Prompt;
};
