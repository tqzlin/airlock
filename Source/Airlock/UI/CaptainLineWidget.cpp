// Copyright Airlock project. All Rights Reserved.

#include "UI/CaptainLineWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

TSharedRef<SWidget> UCaptainLineWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.92f));
		Frame->SetPadding(FMargin(20.f, 12.f));
		LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CaptainLine"));
		LineText->SetText(CurrentLine);
		LineText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 1.f)));
		LineText->SetJustification(ETextJustify::Center);
		LineText->SetWrapTextAt(620.f);
		FSlateFontInfo Font = LineText->GetFont();
		Font.Size = 18;
		LineText->SetFont(Font);
		Frame->SetContent(LineText);
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Frame);
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.f));
		CanvasSlot->SetPosition(FVector2D(0.f, 32.f));
		CanvasSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Canvas;
	}
	return Super::RebuildWidget();
}

void UCaptainLineWidget::ShowLine(const FText& Line)
{
	CurrentLine = Line;
	if (LineText)
	{
		LineText->SetText(Line);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HideTimer, this, &UCaptainLineWidget::HideLine, 6.f, false);
	}
}

void UCaptainLineWidget::HideLine()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UCaptainLineWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimer);
	}
	Super::NativeDestruct();
}
