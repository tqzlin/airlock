// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ship/ShipGraph.h"
#include "Ship/CargoModel.h"
#include "Ship/FuelSystem.h"
#include "TaskBoardWidget.generated.h"

class AAirlockGameState;
class UTextBlock;
class UVerticalBox;

struct FTaskBoardRow
{
	FText Text;
	bool bFailed = false;
};

/** Passive local HUD: task rows come exclusively from the replicated ship graph. */
UCLASS()
class AIRLOCK_API UTaskBoardWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxTaskRows = 8;
	static TArray<FTaskBoardRow> BuildRows(const TArray<FShipModuleState>& Modules, const TArray<FShipLineState>& Lines,
		const TArray<FCargoCrateState>& Cargo = {}, const FAtmosphereState* Air = nullptr, const FFuelState* Fuel = nullptr);
	/** Called by the local controller, including while collapsed; handles late replication and seamless travel. */
	void UpdateState(AAirlockGameState* State);

	UPROPERTY(EditDefaultsOnly, Category = "Airlock|TaskBoard") FLinearColor FrameColour = FLinearColor(.02f, .03f, .05f, .94f);
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|TaskBoard") FLinearColor TextColour = FLinearColor(.9f, .95f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|TaskBoard") FLinearColor FailedColour = FLinearColor(1.f, .22f, .18f);
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|TaskBoard") int32 FontSize = 16;
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|TaskBoard") float PanelWidth = 290.f;
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|TaskBoard") FVector2D PanelOffset = FVector2D(24.f, 120.f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TaskRows;
	TWeakObjectPtr<AAirlockGameState> ObservedState;
	FDelegateHandle GraphChangedHandle;
	int32 LastSeconds = INDEX_NONE;
	void RefreshRows();
	void UnbindState();
};
