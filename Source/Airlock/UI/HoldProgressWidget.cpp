// Copyright Airlock project. All Rights Reserved.
#include "UI/HoldProgressWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Core/Interactable.h"

TSharedRef<SWidget> UHoldProgressWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FrameColour);
		Frame->SetPadding(FMargin(16, 10));
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Contents"));
		Prompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Prompt"));
		Prompt->SetColorAndOpacity(FSlateColor(TextColour));
		Prompt->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Prompt->GetFont(); Font.Size = FontSize; Prompt->SetFont(Font);
		Box->AddChildToVerticalBox(Prompt)->SetPadding(FMargin(0, 0, 0, 8));
		Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress"));
		Bar->SetFillColorAndOpacity(BarColour);
		Box->AddChildToVerticalBox(Bar)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Frame->SetContent(Box);
		UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame);
		FrameSlot->SetAnchors(FAnchors(0.5f, 1.f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 1.f));
		FrameSlot->SetPosition(FVector2D(0, -BottomOffset));
		FrameSlot->SetSize(FrameSize);
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}
void UHoldProgressWidget::UpdateHold(AActor* Target, float Progress)
{
	const bool bHolding = IsValid(Target) && Target->Implements<UInteractable>();
	if (Prompt) { Prompt->SetText(bHolding ? IInteractable::Execute_GetInteractPrompt(Target) : FText::GetEmpty()); }
	if (Bar) { Bar->SetPercent(FMath::Clamp(Progress, 0.f, 1.f)); }
	SetVisibility(bHolding ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
