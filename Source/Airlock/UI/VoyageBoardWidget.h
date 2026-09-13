// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Voyage/VoyageTypes.h"
#include "Ship/CargoModel.h"
#include "VoyageBoardWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoyageChosen, EVoyageType, Type);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVoyageBoardClosed);

/**
 * The departure list. Built in C++ (no UMG asset): a dark panel with a title, one button per offered voyage and a
 * close button. The controller creates it, feeds it the offers, and listens to OnVoyageChosen / OnClosed.
 */
UCLASS()
class AIRLOCK_API UVoyageBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call before AddToViewport. */
	void SetOffers(const TArray<FVoyageSpec>& InOffers);

	/** Replaces the status line under the title (e.g. "Booked: Mining run - proceed to Dock B"). */
	void SetStatus(const FText& InStatus);
	void SetLastVoyageResult(const FVoyageResult& Result, bool bHasResult);
	static FText DescribeResult(const FVoyageResult& Result);
	/** The Rig's stock line above the offers (M3-7). */
	void SetBaseFuel(int32 Tonnes, int32 Runs, int32 SurplusThisRun, bool bHasResult);
	static FText DescribeBase(int32 Tonnes, int32 Runs, int32 SurplusThisRun, bool bHasResult);

	UPROPERTY(BlueprintAssignable, Category = "Airlock|Voyage")
	FOnVoyageChosen OnVoyageChosen;

	UPROPERTY(BlueprintAssignable, Category = "Airlock|Voyage")
	FOnVoyageBoardClosed OnClosed;

	static FText DescribeOffer(const FVoyageSpec& Spec);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Column;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BaseLabel;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> OfferButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OfferLabels;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient) TArray<FVoyageSpec> Offers;
	FVoyageResult LastResult;
	bool bHasLastResult = false;
	int32 BaseFuelTonnes = 0;
	int32 RunsCompleted = 0;
	int32 SurplusThisRun = 0;
	bool bHasBase = false;

	UFUNCTION() void HandleOffer0();
	UFUNCTION() void HandleOffer1();
	UFUNCTION() void HandleOffer2();
	UFUNCTION() void HandleClose();

private:
	void Choose(int32 Index);
	void RefreshLabels();
	UButton* MakeButton(const FText& Label, UTextBlock*& OutLabel);
};
