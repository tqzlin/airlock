// Copyright Airlock project. All Rights Reserved.
#include "UI/PromptWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UPromptWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FrameColour);
		Frame->SetPadding(FMargin(12, 6));
		Prompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Prompt"));
		Prompt->SetColorAndOpacity(FSlateColor(TextColour));
		Prompt->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Prompt->GetFont(); Font.Size = FontSize; Prompt->SetFont(Font);
		Frame->SetContent(Prompt);
		UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame);
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.f));
		FrameSlot->SetPosition(FVector2D(0, BelowCentre));
		FrameSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}

void UPromptWidget::UpdatePrompt(const FText& InPrompt)
{
	if (Prompt) { Prompt->SetText(InPrompt); }
	SetVisibility(InPrompt.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
