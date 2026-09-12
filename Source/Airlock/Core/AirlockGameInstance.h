// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AirlockGameInstance.generated.h"

class SAirlockTransition;

/**
 * Owns what must outlive a level: the airlock transition card (a Slate overlay on the game viewport, so it stays up
 * across ServerTravel) and, through subsystems, the current voyage.
 */
UCLASS()
class AIRLOCK_API UAirlockGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** Fades the black card in with a title/subtitle. Safe to call repeatedly (retexts). */
	UFUNCTION(BlueprintCallable, Category = "Airlock|Transition")
	void ShowTransition(const FText& Title, const FText& Subtitle, float FadeSeconds = 0.45f);

	/** Fades the card out. */
	UFUNCTION(BlueprintCallable, Category = "Airlock|Transition")
	void HideTransition(float FadeSeconds = 0.9f);

	UFUNCTION(BlueprintPure, Category = "Airlock|Transition")
	bool IsTransitionShown() const { return bTransitionShown; }

	virtual void Shutdown() override;

private:
	TSharedPtr<SAirlockTransition> Overlay;
	bool bTransitionShown = false;

	bool EnsureOverlay();
};
