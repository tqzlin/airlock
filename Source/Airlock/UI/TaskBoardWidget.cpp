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
	const TArray<FCargoCrateState>& Cargo, const FAtmosphereState* Air, const FFuelState* Fuel, bool bSummaries, const FRefineryState* Refinery)
{
	TArray<FTaskBoardRow> Rows;
	const FFuelRates FuelRates;
	const FRefineryRates RefineryRates;
	// The floor (M5-3a): a still that has stopped or is in the warning band comes before everything else.
	if (Refinery)
	{
		for (const FStillState& Still : Refinery->Stills)
		{
			const FText Id = FText::FromName(Still.Id);
			if (Still.Status == EStillStatus::Failed) { Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "StillFailedTask", "{0} failed - repair"), Id), true }); }
			else if (Still.Status == EStillStatus::Flash) { Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "StillFlashTask", "{0} flash - purge"), Id), true }); }
			else if (Still.Status == EStillStatus::Popped) { Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "StillPoppedTask", "{0} popped - reset the trap"), Id), true }); }
			else if (Still.PressureBar >= RefineryRates.WarnBar)
			{
				Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "StillPressureTask", "{0} {1} bar - open the vent"), Id,
					FText::FromString(FString::Printf(TEXT("%.1f"), Still.PressureBar))), true });
			}
		}
	}
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
	if (Refinery)
	{
		for (const FStillState& Still : Refinery->Stills)
		{
			if (Still.Status == EStillStatus::Failed) { continue; }
			const FText Id = FText::FromName(Still.Id);
			if (Still.bFouled) { Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "StillFouledTask", "{0} fouled - scrape when empty"), Id), false }); }
			if (Still.Calibration < RefineryRates.CalWarn)
			{
				Rows.Add({ FText::Format(NSLOCTEXT("Airlock", "StillDriftTask", "{0} drifted {1}% - calibrate"), Id,
					FText::AsNumber(FMath::FloorToInt(Still.Calibration * 100.f))), false });
			}
		}
		const bool bIdleStill = Refinery->Stills.ContainsByPredicate([](const FStillState& S) { return S.Status == EStillStatus::Idle && S.Hopper.IsEmpty(); });
		if (!Refinery->Waiting.IsEmpty() && bIdleStill)
		{
			Rows.Add({ Refinery->Waiting.Num() == 1 ? NSLOCTEXT("Airlock", "LoadStillTaskOne", "Load a still - 1 cask waiting")
				: FText::Format(NSLOCTEXT("Airlock", "LoadStillTask", "Load a still - {0} casks waiting"), FText::AsNumber(Refinery->Waiting.Num())), false });
		}
	}
	const float Warn = FCargoRates().WarnTemperature;
	TArray<FCargoCrateState> Heating;
	for (const auto& Crate : Cargo)
	{
		if (Crate.bVolatile && Crate.Temperature >= Warn && Crate.State != ECargoState::Racked && Crate.State != ECargoState::Vented && !FCargoModel::IsConsumed(Crate.State))
		{
			Heating.Add(Crate);
		}
	}
	Heating.StableSort([](const auto& A, const auto& B) { return A.Temperature > B.Temperature; });
	if (bSummaries && Heating.Num() > 2)
	{
		const auto& Hottest = Heating[0];
		Rows.Add({ FText::Format(Refinery ? NSLOCTEXT("Airlock", "TaskLoadSummary", "Load {0} casks - hottest {1} {2}C")
			: NSLOCTEXT("Airlock", "TaskCoolSummary", "Cool {0} casks - hottest {1} {2}C"),
			FText::AsNumber(Heating.Num()), FText::FromName(Hottest.Id),
			FText::AsNumber(FMath::FloorToInt(Hottest.Temperature))), Hottest.Temperature >= 80.f });
	}
	else for (const auto& Crate : Heating)
	{
		// On the floor there is no rack: the only way to stop a warm cask puffing is a still's hopper.
		Rows.Add({ FText::Format(Refinery ? NSLOCTEXT("Airlock", "TaskLoadCargo", "Load crate {0} - {1}C") : NSLOCTEXT("Airlock", "TaskCoolCargo", "Cool crate {0} - {1}C"),
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
	// The floor has no clamps: a cask on a pallet is where it belongs until it goes into a still.
	if (!Refinery)
	{
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

FText UTaskBoardWidget::BuildReadout(const FAtmosphereState* Air, const FFuelState* Fuel, int32 SecondsToDock, bool bFloor)
{
	TArray<FString> Parts;
	if (Air) { Parts.Add(FString::Printf(TEXT("AIR %d ppm"), FMath::FloorToInt(Air->Ppm))); }
	if (Fuel) { Parts.Add(FString::Printf(TEXT("FUEL %d%%"), FMath::FloorToInt(Fuel->Units / FFuelRates().Capacity * 100))); }
	if (SecondsToDock >= 0)
	{
		Parts.Add(FString::Printf(TEXT("%02d:%02d to %s"), SecondsToDock / 60, SecondsToDock % 60, bFloor ? TEXT("horn") : TEXT("dock")));
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
		HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Header"));
		HeaderText->SetText(NSLOCTEXT("Airlock", "TaskHeader", "SHIP TASKS"));
		HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(.7f, .9f, 1.f)));
		FSlateFontInfo Font = HeaderText->GetFont(); Font.Size = FontSize + 2; HeaderText->SetFont(Font);
		Contents->AddChildToVerticalBox(HeaderText)->SetPadding(FMargin(0, 0, 0, 4));
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
	const bool bFloor = State && State->RefineryHasFloor();
	LastSeconds = State && State->bVoyageActive ? FMath::CeilToInt(State->GetVoyageSecondsRemaining()) : INDEX_NONE;
	if (HeaderText) { HeaderText->SetText(bFloor ? NSLOCTEXT("Airlock", "FloorTaskHeader", "FLOOR TASKS") : NSLOCTEXT("Airlock", "TaskHeader", "SHIP TASKS")); }
	ReadoutText->SetText(BuildReadout(State && State->AirHasAtmosphere() ? &State->Atmosphere : nullptr,
		State && FuelLayouts::Find(State->CurrentSectionId) ? &State->Fuel : nullptr, LastSeconds, bFloor));
}

void UTaskBoardWidget::RefreshRows()
{
	RefreshReadout();
	if (!TaskRows || !WidgetTree) { return; }
	TaskRows->ClearChildren();
	const AAirlockGameState* State = ObservedState.Get();
	if (!State) { return; }
	for (const FTaskBoardRow& Row : BuildRows(State->ShipModules, State->ShipLines, State->Cargo, State->AirHasAtmosphere() ? &State->Atmosphere : nullptr,
		FuelLayouts::Find(State->CurrentSectionId) ? &State->Fuel : nullptr, true, State->RefineryHasFloor() ? &State->Refinery : nullptr))
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
