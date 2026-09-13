// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ship/ShipGraph.h"
#include "ControlBoardWidget.generated.h"

class AAirlockGameState;
class AControlStation;
class UTextBlock;
class UVerticalBox;

enum class EControlBoardTone : uint8 { Normal, Warning, Failed, Header };

struct FControlBoardRow
{
	FText Text;
	EControlBoardTone Tone = EControlBoardTone::Normal;
};

/**
 * The control room's view of the ship (design 8.1 lever 1: the seat sees the state, the field sees the thing).
 * Every module and every line, not just the open tasks. Local, passive: rows come only from the replicated ship
 * graph on the game state; shown while the local pawn sits at a control station.
 */
UCLASS()
class AIRLOCK_API UControlBoardWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static TArray<FControlBoardRow> BuildRows(const TArray<FShipModuleState>& Modules, const TArray<FShipLineState>& Lines);
	/** Called every tick by the local controller; Station null hides the board. */
	void UpdateState(AAirlockGameState* State, AControlStation* Station);

	UPROPERTY(EditDefaultsOnly, Category = "Airlock|ControlBoard") FLinearColor FrameColour = FLinearColor(.01f, .03f, .06f, .96f);
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|ControlBoard") FLinearColor TextColour = FLinearColor(.85f, .95f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|ControlBoard") FLinearColor HeaderColour = FLinearColor(.45f, .8f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|ControlBoard") FLinearColor WarningColour = FLinearColor(1.f, .7f, .2f);
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|ControlBoard") FLinearColor FailedColour = FLinearColor(1.f, .25f, .2f);
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|ControlBoard") int32 FontSize = 15;
	UPROPERTY(EditDefaultsOnly, Category = "Airlock|ControlBoard") float PanelWidth = 640.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AirText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Rows;
	TWeakObjectPtr<AAirlockGameState> ObservedState;
	TWeakObjectPtr<AControlStation> ObservedStation;
	FDelegateHandle GraphChangedHandle;
	int32 LastSeconds = INDEX_NONE;
	void RefreshRows();
	void UnbindState();
};
