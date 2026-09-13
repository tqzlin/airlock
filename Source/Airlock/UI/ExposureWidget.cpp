// Copyright Airlock project. All Rights Reserved.
#include "UI/ExposureWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UExposureWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		Tint = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ExposureTint"));
		Tint->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		Tint->SetBrushColor(FLinearColor(.35f, .05f, .02f, 0.f));
		auto* TintSlot = Canvas->AddChildToCanvas(Tint);
		TintSlot->SetAnchors(FAnchors(0, 0, 1, 1)); TintSlot->SetOffsets(FMargin(0));
		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ExposureLabel"));
		FSlateFontInfo Font = Label->GetFont(); Font.Size = 22; Label->SetFont(Font);
		Label->SetJustification(ETextJustify::Right);
		auto* LabelSlot = Canvas->AddChildToCanvas(Label);
		LabelSlot->SetAnchors(FAnchors(1, 1)); LabelSlot->SetAlignment(FVector2D(1, 1));
		LabelSlot->SetPosition(FVector2D(-28, -32)); LabelSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Canvas;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}
void UExposureWidget::UpdateExposure(float Exposure, bool bBlackedOut, double EndServerTime, double ServerTime)
{
	const float Opacity = bBlackedOut ? .85f : FMath::Clamp((Exposure - 30.f) / 70.f, 0.f, 1.f) * .55f;
	if (Tint) { Tint->SetBrushColor(FLinearColor(.35f, .05f, .02f, Opacity)); }
	if (Label)
	{
		Label->SetVisibility(bBlackedOut || Exposure >= 30 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Label->SetText(FText::FromString(bBlackedOut
			? FString::Printf(TEXT("BLACKED OUT - %d s"), FMath::CeilToInt(FMath::Max(0.0, EndServerTime - ServerTime)))
			: FString::Printf(TEXT("EXPOSURE %d%%"), FMath::FloorToInt(Exposure))));
		Label->SetColorAndOpacity(FSlateColor(Exposure >= 60 ? FLinearColor(1.f, .2f, .1f) : FLinearColor(1.f, .85f, .65f)));
	}
}
