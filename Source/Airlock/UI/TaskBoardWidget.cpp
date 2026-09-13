// Copyright Airlock project. All Rights Reserved.
#include "UI/TaskBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/AirlockGameState.h"
#include "Ship/FuelLayouts.h"

TArray<FTaskBoardRow> UTaskBoardWidget::BuildRows(const TArray<FShipModuleState>& Modules, const TArray<FShipLineState>& Lines,
	const TArray<FCargoCrateState>& Cargo, const FAtmosphereState* Air, const FFuelState* Fuel, bool bSummaries)
{
	TArray<FTaskBoardRow> Rows;
	const FFuelRates FuelRates;
	if (Fuel && Fuel->bFeedLeak)
	{ Rows.Add({ NSLOCTEXT("Airlock", "FuelLeakTask", "Fuel leak - shut the feed valve"), true }); }
	else if (Fuel && Fuel->Units < FuelRates.Capacity * FuelRates.LowFuelPercent / 100.f && Fuel->BurningCrate.IsNone())
	{
		const FText Pattern = Fuel->bBypassOpen
			? NSLOCTEXT("Airlock", "FuelIntakeTask", "Fuel {0}% - feed casks to the intake")
			: NSLOCTEXT("Airlock", "FuelBypassTask", "Fuel {0}% - open the bypass and feed casks");
		Rows.Add({ FText::Format(Pattern, FText::AsNumber(FMath::FloorToInt(Fuel->Units))), Fuel->Units < FuelRates.Capacity * FuelRates.CriticalFuelPercent / 100.f });
	}
	if (Air && Air->Ppm >= FAtmosphereRates().WarnPpm)
	{
		const FText Action = Air->bScrubDisabled ? NSLOCTEXT("Airlock", "AirTaskDisabled", "repair life support")
			: Air->ScrubMode == EScrubMode::Hazard ? NSLOCTEXT("Airlock", "AirTaskScrubbing", "scrubbing") : NSLOCTEXT("Airlock", "AirTaskScrub", "scrub");
		Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "AirTask", "Hold air {0} ppm - {1}"),
			FText::AsNumber(FMath::FloorToInt(Air->Ppm)), Action), Air->Ppm >= FAtmosphereRates().DangerPpm });
	}
	if (Fuel && Fuel->bInjectorsFouled) { Rows.Add({ NSLOCTEXT("Airlock", "InjectorTask", "Clear the injectors"), false }); }
	const float Warn = FCargoRates().WarnTemperature;
	TArray<FCargoCrateState> Heating;
	for (const auto& Crate : Cargo)
	{
		if (Crate.bVolatile && Crate.Temperature >= Warn && Crate.State != ECargoState::Racked && Crate.State != ECargoState::Vented && Crate.State != ECargoState::Burned)
		{
			Heating.Add(Crate);
		}
	}
	Heating.StableSort([](const auto& A, const auto& B) { return A.Temperature > B.Temperature; });
	if (bSummaries && Heating.Num() > 2)
	{
		const auto& Hottest = Heating[0];
		Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "TaskCoolSummary", "Cool {0} casks - hottest {1} {2}C"),
			FText::AsNumber(Heating.Num()), FText::FromName(Hottest.Id),
			FText::AsNumber(FMath::FloorToInt(Hottest.Temperature))), Hottest.Temperature >= 80.f });
	}
	else for (const auto& Crate : Heating)
	{
		Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "TaskCoolCargo", "Cool crate {0} - {1}C"),
			FText::FromName(Crate.Id), FText::AsNumber(FMath::FloorToInt(Crate.Temperature))), Crate.Temperature >= 80.f });
	}
	auto AddModules = [&](EShipModuleStatus Status)
	{
		for (const FShipModuleState& Module : Modules)
		{
			if (Module.Status != Status) { continue; }
			const FText StatusText = Status == EShipModuleStatus::Failed ? NSLOCTEXT("Airlock", "TaskFailed", "Failed") : NSLOCTEXT("Airlock", "TaskDegraded", "Degraded");
			Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "TaskModule", "{0} - {1} {2}%"), FText::FromName(Module.Id),
				StatusText, FText::AsNumber(FMath::RoundToInt(Module.Condition))), Status == EShipModuleStatus::Failed });
		}
	};
	AddModules(EShipModuleStatus::Failed);
	for (const FShipLineState& Line : Lines)
	{
		if (Line.Fault == EShipLineFault::None) { continue; }
		const FText Pattern = Line.Fault == EShipLineFault::Leak ? NSLOCTEXT("Airlock", "TaskLeak", "Tape leak - {0}") : NSLOCTEXT("Airlock", "TaskShort", "Patch short - {0}");
		Rows.Add({ FText::Format(Pattern, FText::FromName(Line.Id)), false });
	}
	AddModules(EShipModuleStatus::Degraded);
	int32 LooseCount = 0;
	if (bSummaries)
	{
		for (const auto& Crate : Cargo)
		{
			LooseCount += Crate.State == ECargoState::Loose && (!Crate.bVolatile || Crate.Temperature < Warn);
		}
	}
	if (LooseCount > 4)
	{
		Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "TaskSecureSummary", "Secure {0} loose casks"),
			FText::AsNumber(LooseCount)), false });
	}
	else for (const auto& Crate : Cargo)
	{
		if (Crate.State == ECargoState::Loose && (!Crate.bVolatile || Crate.Temperature < Warn))
		{
			Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "TaskSecureCargo", "Secure crate {0}"), FText::FromName(Crate.Id)), false });
		}
	}
	if (Rows.Num() > MaxTaskRows)
	{
		const int32 More = Rows.Num() - MaxTaskRows;
		Rows.SetNum(MaxTaskRows);
		Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "TaskMore", "+{0} more"), FText::AsNumber(More)), false });
	}
	if (Rows.IsEmpty()) { Rows.Add({ NSLOCTEXT("Airlock", "TasksNominal", "All systems nominal"), false }); }
	return Rows;
}

FText UTaskBoardWidget::BuildReadout(const FAtmosphereState* Air, const FFuelState* Fuel, int32 SecondsToDock)
{
	TArray<FString> Parts;
	if (Air) { Parts.Add(FString::Printf(TEXT("AIR %d ppm"), FMath::FloorToInt(Air->Ppm))); }
	if (Fuel) { Parts.Add(FString::Printf(TEXT("FUEL %d%%"), FMath::FloorToInt(Fuel->Units / FFuelRates().Capacity * 100))); }
	if (SecondsToDock >= 0)
	{
		Parts.Add(FString::Printf(TEXT("%02d:%02d to dock"), SecondsToDock / 60, SecondsToDock % 60));
	}
	return FText::FromString(FString::Join(Parts, TEXT("  |  ")));
}

TSharedRef<SWidget> UTaskBoardWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(FrameColour);
		Frame->SetPadding(FMargin(16.f, 12.f));
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Width"));
		Width->SetWidthOverride(PanelWidth);
		UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Contents"));
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Header"));
		Header->SetText(NSLOCTEXT("Airlock", "TaskHeader", "SHIP TASKS"));
		Header->SetColorAndOpacity(FSlateColor(FLinearColor(.7f, .9f, 1.f)));
		FSlateFontInfo Font = Header->GetFont(); Font.Size = FontSize + 2; Header->SetFont(Font);
		Contents->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 4));
		ReadoutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Readout"));
		ReadoutText->SetColorAndOpacity(FSlateColor(TextColour));
		Font.Size = FontSize - 4; ReadoutText->SetFont(Font);
		Contents->AddChildToVerticalBox(ReadoutText)->SetPadding(FMargin(0, 0, 0, 10));
		TaskRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Tasks"));
		Contents->AddChildToVerticalBox(TaskRows);
		Width->SetContent(Contents);
		Frame->SetContent(Width);
		UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame);
		FrameSlot->SetPosition(PanelOffset);
		FrameSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Canvas;
	}
	RefreshRows();
	LastSeconds = INDEX_NONE;
	return Super::RebuildWidget();
}

void UTaskBoardWidget::UnbindState()
{
	if (AAirlockGameState* State = ObservedState.Get()) { State->OnShipGraphChanged.Remove(GraphChangedHandle); }
	ObservedState.Reset();
	GraphChangedHandle.Reset();
}

void UTaskBoardWidget::UpdateState(AAirlockGameState* State)
{
	if (State != ObservedState.Get())
	{
		UnbindState();
		ObservedState = State;
		if (State) { GraphChangedHandle = State->OnShipGraphChanged.AddUObject(this, &UTaskBoardWidget::RefreshRows); }
		RefreshRows();
		LastSeconds = INDEX_NONE;
	}
	const bool bVisible = State && State->bVoyageActive && !State->CurrentSectionId.IsNone() && State->CurrentSectionId != TEXT("StationHub");
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible && ReadoutText)
	{
		const int32 Seconds = FMath::CeilToInt(State->GetVoyageSecondsRemaining());
		if (Seconds != LastSeconds)
		{
			RefreshReadout();
		}
	}
}

void UTaskBoardWidget::RefreshReadout()
{
	if (!ReadoutText) { return; }
	const AAirlockGameState* State = ObservedState.Get();
	LastSeconds = State && State->bVoyageActive ? FMath::CeilToInt(State->GetVoyageSecondsRemaining()) : INDEX_NONE;
	ReadoutText->SetText(BuildReadout(State && State->AirHasAtmosphere() ? &State->Atmosphere : nullptr,
		State && FuelLayouts::Find(State->CurrentSectionId) ? &State->Fuel : nullptr, LastSeconds));
}

void UTaskBoardWidget::RefreshRows()
{
	RefreshReadout();
	if (!TaskRows || !WidgetTree) { return; }
	TaskRows->ClearChildren();
	const AAirlockGameState* State = ObservedState.Get();
	if (!State) { return; }
	for (const FTaskBoardRow& Row : BuildRows(State->ShipModules, State->ShipLines, State->Cargo, State->AirHasAtmosphere() ? &State->Atmosphere : nullptr, FuelLayouts::Find(State->CurrentSectionId) ? &State->Fuel : nullptr, true))
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(Row.Text);
		Text->SetColorAndOpacity(FSlateColor(Row.bFailed ? FailedColour : TextColour));
		Text->SetAutoWrapText(true);
		FSlateFontInfo Font = Text->GetFont(); Font.Size = FontSize; Text->SetFont(Font);
		TaskRows->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, 3));
	}
}

void UTaskBoardWidget::NativeDestruct()
{
	UnbindState();
	Super::NativeDestruct();
}
