// Copyright Airlock project. All Rights Reserved.
#include "UI/CarryPromptWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Ship/CargoCrate.h"

TSharedRef<SWidget> UCarryPromptWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		auto* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FLinearColor(.02f, .03f, .05f, .95f));
		Frame->SetPadding(FMargin(16, 10));
		Prompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Prompt"));
		Prompt->SetColorAndOpacity(FSlateColor(FLinearColor(.8f, .95f, 1.f)));
		Prompt->SetJustification(ETextJustify::Center);
		Prompt->SetAutoWrapText(true);
		FSlateFontInfo Font = Prompt->GetFont(); Font.Size = 16; Prompt->SetFont(Font);
		Frame->SetContent(Prompt);
		auto* FrameSlot = Canvas->AddChildToCanvas(Frame);
		FrameSlot->SetAnchors(FAnchors(.5f, 1.f));
		FrameSlot->SetAlignment(FVector2D(.5f, 1.f));
		FrameSlot->SetPosition(FVector2D(0, -64));
		FrameSlot->SetSize(FVector2D(660, 70));
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}
void UCarryPromptWidget::UpdateCarry(ACargoCrate* Crate)
{
	if (Prompt)
	{
		Prompt->SetText(IsValid(Crate) ? FText::Format(NSLOCTEXT("Airlock", "CarryingPrompt",
			"Carrying {0} - E: put down, or place in a clamp / the cold rack"), FText::FromName(Crate->GetCargoId())) : FText::GetEmpty());
	}
	SetVisibility(IsValid(Crate) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
