// Copyright Airlock project. All Rights Reserved.

#include "UI/SAirlockTransition.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor CardBlack(0.f, 0.f, 0.f, 1.f);
	const FLinearColor Accent(0.55f, 0.85f, 1.0f, 1.f);
	const FLinearColor Dim(0.7f, 0.75f, 0.8f, 1.f);
}

void SAirlockTransition::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.ColorAndOpacity(CardBlack)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 10.f)
			[
				SAssignNew(TitleText, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 36))
				.ColorAndOpacity(FSlateColor(Accent))
				.Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SAssignNew(SubtitleText, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
				.ColorAndOpacity(FSlateColor(Dim))
				.Justification(ETextJustify::Center)
			]
		]
	];

	SetVisibility(EVisibility::HitTestInvisible);
	SetRenderOpacity(0.f);
}

void SAirlockTransition::SetTexts(const FText& Title, const FText& Subtitle)
{
	if (TitleText.IsValid())
	{
		TitleText->SetText(Title);
	}
	if (SubtitleText.IsValid())
	{
		SubtitleText->SetText(Subtitle);
	}
}

void SAirlockTransition::FadeTo(float Target, float Seconds, TFunction<void()> OnDone)
{
	StartOpacity = CurrentOpacity;
	TargetOpacity = FMath::Clamp(Target, 0.f, 1.f);
	Duration = FMath::Max(Seconds, 0.01f);
	Elapsed = 0.f;
	DoneCallback = MoveTemp(OnDone);
	if (!bAnimating)
	{
		bAnimating = true;
		TimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAirlockTransition::Animate));
	}
}

EActiveTimerReturnType SAirlockTransition::Animate(double InCurrentTime, float InDeltaTime)
{
	Elapsed += InDeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	CurrentOpacity = FMath::Lerp(StartOpacity, TargetOpacity, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f));
	SetRenderOpacity(CurrentOpacity);

	if (Alpha >= 1.f)
	{
		bAnimating = false;
		TimerHandle.Reset();
		if (DoneCallback)
		{
			TFunction<void()> Callback = MoveTemp(DoneCallback);
			DoneCallback = nullptr;
			Callback();
		}
		return EActiveTimerReturnType::Stop;
	}
	return EActiveTimerReturnType::Continue;
}
