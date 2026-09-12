// Copyright Airlock project. All Rights Reserved.
#include "UI/ControlBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/AirlockGameState.h"
#include "Ship/ControlStation.h"

namespace
{
const TCHAR* ModuleTypeName(EShipModuleType Type)
{
	switch (Type)
	{
	case EShipModuleType::PowerCore: return TEXT("power core");
	case EShipModuleType::CoolantLoop: return TEXT("coolant loop");
	case EShipModuleType::LifeSupport: return TEXT("life support");
	case EShipModuleType::CargoClamps: return TEXT("cargo clamps");
	case EShipModuleType::Engines: return TEXT("engines");
	case EShipModuleType::Nav: return TEXT("nav");
	case EShipModuleType::Galley: return TEXT("galley");
	default: return TEXT("?");
	}
}
}

TArray<FControlBoardRow> UControlBoardWidget::BuildRows(const TArray<FShipModuleState>& Modules, const TArray<FShipLineState>& Lines)
{
	TArray<FControlBoardRow> Out;
	Out.Add({ NSLOCTEXT("Airlock", "BoardModules", "MODULES"), EControlBoardTone::Header });
	for (const FShipModuleState& M : Modules)
	{
		const EControlBoardTone Tone = M.Status == EShipModuleStatus::Failed ? EControlBoardTone::Failed
			: M.Status == EShipModuleStatus::Degraded ? EControlBoardTone::Warning : EControlBoardTone::Normal;
		Out.Add({ FText::FromString(FString::Printf(TEXT("%-13s %-14s %-9s %3.0f%%"), *M.Id.ToString(), ModuleTypeName(M.Type),
			FShipGraph::StatusName(M.Status), M.Condition)), Tone });
	}
	Out.Add({ NSLOCTEXT("Airlock", "BoardLines", "LINES"), EControlBoardTone::Header });
	for (const FShipLineState& L : Lines)
	{
		const bool bFaulted = L.Fault != EShipLineFault::None;
		Out.Add({ FText::FromString(FString::Printf(TEXT("%-13s %-8s %s -> %s   %s"), *L.Id.ToString(),
			L.Kind == EShipLineKind::Coolant ? TEXT("coolant") : TEXT("power"), *L.FromModule.ToString(), *L.ToModule.ToString(),
			bFaulted ? FShipGraph::FaultName(L.Fault) : TEXT("OK"))), bFaulted ? EControlBoardTone::Warning : EControlBoardTone::Normal });
	}
	if (Modules.IsEmpty() && Lines.IsEmpty())
	{
		Out.Add({ NSLOCTEXT("Airlock", "BoardNoShip", "No ship systems on this section"), EControlBoardTone::Normal });
	}
	return Out;
}

TSharedRef<SWidget> UControlBoardWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FrameColour);
		Frame->SetPadding(FMargin(22.f, 16.f));
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Width"));
		Width->SetWidthOverride(PanelWidth);
		UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Contents"));
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		TitleText->SetColorAndOpacity(FSlateColor(HeaderColour));
		FSlateFontInfo TitleFont = TitleText->GetFont(); TitleFont.Size = FontSize + 6; TitleText->SetFont(TitleFont);
		Contents->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0, 0, 0, 4));
		ClockText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Clock"));
		ClockText->SetColorAndOpacity(FSlateColor(TextColour));
		FSlateFontInfo ClockFont = ClockText->GetFont(); ClockFont.Size = FontSize; ClockText->SetFont(ClockFont);
		Contents->AddChildToVerticalBox(ClockText)->SetPadding(FMargin(0, 0, 0, 12));
		Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Contents->AddChildToVerticalBox(Rows);
		UTextBlock* Footer = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Footer"));
		Footer->SetText(NSLOCTEXT("Airlock", "BoardFooter", "E - stand up"));
		Footer->SetColorAndOpacity(FSlateColor(HeaderColour));
		FSlateFontInfo FooterFont = Footer->GetFont(); FooterFont.Size = FontSize; Footer->SetFont(FooterFont);
		Contents->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0, 14, 0, 0));
		Width->SetContent(Contents);
		Frame->SetContent(Width);
		UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame);
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FrameSlot->SetPosition(FVector2D(0, 0));
		FrameSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Canvas;
	}
	RefreshRows();
	LastSeconds = INDEX_NONE;
	return Super::RebuildWidget();
}

void UControlBoardWidget::UnbindState()
{
	if (AAirlockGameState* State = ObservedState.Get()) { State->OnShipGraphChanged.Remove(GraphChangedHandle); }
	ObservedState.Reset();
	GraphChangedHandle.Reset();
}

void UControlBoardWidget::UpdateState(AAirlockGameState* State, AControlStation* Station)
{
	if (State != ObservedState.Get())
	{
		UnbindState();
		ObservedState = State;
		if (State) { GraphChangedHandle = State->OnShipGraphChanged.AddUObject(this, &UControlBoardWidget::RefreshRows); }
		RefreshRows();
		LastSeconds = INDEX_NONE;
	}
	if (Station != ObservedStation.Get())
	{
		ObservedStation = Station;
		if (TitleText)
		{
			TitleText->SetText(Station
				? FText::Format(NSLOCTEXT("Airlock", "BoardTitle", "CONTROL STATION  {0}"), FText::FromName(Station->GetStationId()))
				: FText::GetEmpty());
		}
		LastSeconds = INDEX_NONE;
	}
	const bool bVisible = IsValid(Station) && State;
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible && ClockText)
	{
		const int32 Seconds = State->bVoyageActive ? FMath::CeilToInt(State->GetVoyageSecondsRemaining()) : -1;
		if (Seconds != LastSeconds)
		{
			ClockText->SetText(Seconds >= 0
				? FText::Format(NSLOCTEXT("Airlock", "BoardClock", "{0}  |  {1} to dock"), FText::FromName(State->CurrentSectionId),
					FText::FromString(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60)))
				: FText::Format(NSLOCTEXT("Airlock", "BoardDocked", "{0}  |  docked"), FText::FromName(State->CurrentSectionId)));
			LastSeconds = Seconds;
		}
	}
}

void UControlBoardWidget::RefreshRows()
{
	if (!Rows || !WidgetTree) { return; }
	Rows->ClearChildren();
	const AAirlockGameState* State = ObservedState.Get();
	if (!State) { return; }
	for (const FControlBoardRow& Row : BuildRows(State->ShipModules, State->ShipLines))
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(Row.Text);
		const FLinearColor Colour = Row.Tone == EControlBoardTone::Failed ? FailedColour : Row.Tone == EControlBoardTone::Warning ? WarningColour
			: Row.Tone == EControlBoardTone::Header ? HeaderColour : TextColour;
		Text->SetColorAndOpacity(FSlateColor(Colour));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		if (Row.Tone != EControlBoardTone::Header) { Font.TypefaceFontName = TEXT("Mono"); }
		Text->SetFont(Font);
		Rows->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, Row.Tone == EControlBoardTone::Header ? 8 : 2, 0, 0));
	}
}

void UControlBoardWidget::NativeDestruct()
{
	UnbindState();
	Super::NativeDestruct();
}
