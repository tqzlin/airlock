// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CaptainLineWidget.generated.h"

class UTextBlock;

/** A non-interactive captain transmission at the top centre of the player's viewport. No widget asset. */
UCLASS()
class AIRLOCK_API UCaptainLineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Replace the current line and restart its six-second display time. */
	void ShowLine(const FText& Line);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LineText;

	FText CurrentLine;
	FTimerHandle HideTimer;
	void HideLine();
};
