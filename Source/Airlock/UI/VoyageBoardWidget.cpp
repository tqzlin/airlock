// Copyright Airlock project. All Rights Reserved.

#include "UI/VoyageBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	const FLinearColor PanelColour(0.02f, 0.03f, 0.05f, 0.94f);
	const FLinearColor AccentColour(0.55f, 0.85f, 1.0f, 1.f);
	const FLinearColor ButtonColour(0.08f, 0.12f, 0.18f, 1.f);
	const FLinearColor ButtonHoverColour(0.15f, 0.30f, 0.45f, 1.f);
}

FText UVoyageBoardWidget::DescribeOffer(const FVoyageSpec& Spec)
{
	const UEnum* TypeEnum = StaticEnum<EVoyageType>();
	const FText TypeName = TypeEnum ? TypeEnum->GetDisplayNameTextByValue(static_cast<int64>(Spec.Type)) : FText::GetEmpty();
	return FText::Format(
		NSLOCTEXT("Airlock", "VoyageOfferLine", "{0}  -  {1} casks  -  Capt. {2}  -  crew {3}  -  {4} min"),
		TypeName, FText::AsNumber(Spec.CaskCount), FText::FromName(Spec.Captain),
		FText::AsNumber(Spec.CrewCapacity), FText::AsNumber(FMath::RoundToInt(Spec.DurationSeconds / 60.f)));
}

void UVoyageBoardWidget::SetOffers(const TArray<FVoyageSpec>& InOffers)
{
	Offers = InOffers;
	RefreshLabels();
}

FText UVoyageBoardWidget::DescribeResult(const FVoyageResult& Result)
{
	FText Base = FText::Format(NSLOCTEXT("Airlock", "LastVoyageResult", "Last run: {0}/{1} crates intact - hull {2}% - pay {3} cr"),
		FText::AsNumber(Result.CratesIntact), FText::AsNumber(Result.CratesTotal),
		FText::AsNumber(FMath::RoundToInt(Result.ShipCondition)), FText::AsNumber(Result.Pay));
	if (Result.DockPpm >= 0)
	{
		Base = Result.bCleanAir ? FText::Format(NSLOCTEXT("Airlock", "LastRunCleanAir", "{0} - air clean"), Base)
			: FText::Format(NSLOCTEXT("Airlock", "LastRunDirtyAir", "{0} - air {1} ppm"), Base, FText::AsNumber(FMath::FloorToInt(Result.DockPpm)));
	}
	if (Result.CratesBurned > 0) { Base = FText::Format(NSLOCTEXT("Airlock", "LastRunBurned", "{0} - {1} burned"), Base, FText::AsNumber(Result.CratesBurned)); }
	if (Result.bLateDock) { Base = FText::Format(NSLOCTEXT("Airlock", "LastRunLate", "{0} - late"), Base); }
	if (Result.bHasFloor) { Base = FText::Format(NSLOCTEXT("Airlock", "LastRunFloor", "{0} - floor: {1}/{2} refined"), Base, FText::AsNumber(Result.FloorCasksRefined), FText::AsNumber(Result.FloorCasksTotal)); }
	return Base;
}
FText UVoyageBoardWidget::DescribeBase(int32 Tonnes, int32 Runs, int32 Surplus, bool bHasResult)
{
	const FText Base = FText::Format(NSLOCTEXT("Airlock", "BaseFuelLine", "Base fuel {0} t  -  runs {1}"), FText::AsNumber(Tonnes), FText::AsNumber(Runs));
	return bHasResult ? FText::Format(NSLOCTEXT("Airlock", "BaseFuelLineRun", "{0}  (+{1} this run)"), Base, FText::AsNumber(Surplus)) : Base;
}
void UVoyageBoardWidget::SetBaseFuel(int32 Tonnes, int32 Runs, int32 InSurplusThisRun, bool bHasResult)
{
	BaseFuelTonnes = Tonnes; RunsCompleted = Runs; SurplusThisRun = InSurplusThisRun; bHasBase = true; bHasLastResult = bHasResult || bHasLastResult;
	RefreshLabels();
}
void UVoyageBoardWidget::SetLastVoyageResult(const FVoyageResult& Result, bool bHasResult)
{
	LastResult = Result;
	bHasLastResult = bHasResult;
	RefreshLabels();
}

void UVoyageBoardWidget::SetStatus(const FText& InStatus)
{
	if (Status)
	{
		Status->SetText(InStatus);
	}
}

UButton* UVoyageBoardWidget::MakeButton(const FText& Label, UTextBlock*& OutLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style = Button->GetStyle();
	Style.Normal.TintColor = FSlateColor(ButtonColour);
	Style.Hovered.TintColor = FSlateColor(ButtonHoverColour);
	Style.Pressed.TintColor = FSlateColor(AccentColour);
	Button->SetStyle(Style);

	OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OutLabel->SetText(Label);
	OutLabel->SetJustification(ETextJustify::Left);
	FSlateFontInfo Font = OutLabel->GetFont();
	Font.Size = 16;
	OutLabel->SetFont(Font);

	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->AddChild(OutLabel)))
	{
		ButtonSlot->SetPadding(FMargin(18.f, 12.f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	if (UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(Button))
	{
		BoxSlot->SetPadding(FMargin(0.f, 6.f));
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Button;
}

TSharedRef<SWidget> UVoyageBoardWidget::RebuildWidget()
{
	if (!Frame && WidgetTree)
	{
		Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrushColor(PanelColour);
		Frame->SetPadding(FMargin(28.f, 24.f));

		Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
		Frame->SetContent(Column);

		Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetText(NSLOCTEXT("Airlock", "VoyageBoardTitle", "VELL EXTRACTOR  -  DOCK B DEPARTURES"));
		Title->SetColorAndOpacity(FSlateColor(AccentColour));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 28;
		Title->SetFont(TitleFont);
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Title)) { S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f)); }

		BaseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BaseFuel"));
		BaseLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.75f, 0.45f)));
		FSlateFontInfo BaseFont = BaseLabel->GetFont(); BaseFont.Size = 16; BaseLabel->SetFont(BaseFont);
		Column->AddChildToVerticalBox(BaseLabel)->SetPadding(FMargin(0, 0, 0, 4));

		ResultLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LastRun"));
		ResultLabel->SetColorAndOpacity(FSlateColor(AccentColour));
		FSlateFontInfo ResultFont = ResultLabel->GetFont(); ResultFont.Size = 16; ResultLabel->SetFont(ResultFont);
		Column->AddChildToVerticalBox(ResultLabel)->SetPadding(FMargin(0, 0, 0, 10));

		Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
		Status->SetText(NSLOCTEXT("Airlock", "VoyageBoardHint", "Pick a contract, then proceed to Dock B."));
		FSlateFontInfo StatusFont = Status->GetFont();
		StatusFont.Size = 13;
		Status->SetFont(StatusFont);
		Status->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.75f, 0.8f)));
		if (UVerticalBoxSlot* S = Column->AddChildToVerticalBox(Status)) { S->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f)); }

		OfferButtons.Reset();
		OfferLabels.Reset();
		for (int32 i = 0; i < 3; ++i)
		{
			UTextBlock* Label = nullptr;
			UButton* Button = MakeButton(NSLOCTEXT("Airlock", "VoyageOfferPending", "..."), Label);
			switch (i)
			{
			case 0: Button->OnClicked.AddDynamic(this, &UVoyageBoardWidget::HandleOffer0); break;
			case 1: Button->OnClicked.AddDynamic(this, &UVoyageBoardWidget::HandleOffer1); break;
			default: Button->OnClicked.AddDynamic(this, &UVoyageBoardWidget::HandleOffer2); break;
			}
			OfferButtons.Add(Button);
			OfferLabels.Add(Label);
		}

		UTextBlock* CloseLabel = nullptr;
		CloseButton = MakeButton(NSLOCTEXT("Airlock", "VoyageBoardClose", "Close"), CloseLabel);
		CloseLabel->SetJustification(ETextJustify::Center);
		CloseButton->OnClicked.AddDynamic(this, &UVoyageBoardWidget::HandleClose);

		WidgetTree->RootWidget = Frame;
		RefreshLabels();
	}
	return Super::RebuildWidget();
}

void UVoyageBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshLabels();
}

void UVoyageBoardWidget::RefreshLabels()
{
	if (BaseLabel)
	{
		BaseLabel->SetText(bHasBase ? DescribeBase(BaseFuelTonnes, RunsCompleted, SurplusThisRun, bHasLastResult) : FText::GetEmpty());
		BaseLabel->SetVisibility(bHasBase ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ResultLabel)
	{
		ResultLabel->SetText(bHasLastResult ? DescribeResult(LastResult) : FText::GetEmpty());
		ResultLabel->SetVisibility(bHasLastResult ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	for (int32 i = 0; i < OfferLabels.Num(); ++i)
	{
		const bool bHasOffer = Offers.IsValidIndex(i);
		if (OfferLabels[i])
		{
			OfferLabels[i]->SetText(bHasOffer ? DescribeOffer(Offers[i]) : NSLOCTEXT("Airlock", "VoyageNoOffer", "(no departure)"));
		}
		if (OfferButtons.IsValidIndex(i) && OfferButtons[i])
		{
			OfferButtons[i]->SetIsEnabled(bHasOffer);
		}
	}
}

void UVoyageBoardWidget::Choose(int32 Index)
{
	if (Offers.IsValidIndex(Index))
	{
		OnVoyageChosen.Broadcast(Offers[Index].Type);
	}
}

void UVoyageBoardWidget::HandleOffer0() { Choose(0); }
void UVoyageBoardWidget::HandleOffer1() { Choose(1); }
void UVoyageBoardWidget::HandleOffer2() { Choose(2); }
void UVoyageBoardWidget::HandleClose() { OnClosed.Broadcast(); }
