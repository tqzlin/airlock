// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;

/**
 * Full-screen black card shown while an airlock cycles / a level travels: title + subtitle, fades in and out.
 * Pure Slate and owned by the GameInstance so it survives the map change (UMG widgets do not).
 */
class SAirlockTransition : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAirlockTransition) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetTexts(const FText& Title, const FText& Subtitle);

	/** Animates render opacity to Target over Seconds; OnDone fires once when it lands. */
	void FadeTo(float Target, float Seconds, TFunction<void()> OnDone = nullptr);

	float GetOpacity() const { return CurrentOpacity; }

private:
	EActiveTimerReturnType Animate(double InCurrentTime, float InDeltaTime);

	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> SubtitleText;

	float CurrentOpacity = 0.f;
	float StartOpacity = 0.f;
	float TargetOpacity = 0.f;
	float Duration = 0.f;
	float Elapsed = 0.f;
	bool bAnimating = false;
	TFunction<void()> DoneCallback;
	TSharedPtr<FActiveTimerHandle> TimerHandle;
};
