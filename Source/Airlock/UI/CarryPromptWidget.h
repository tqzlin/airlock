// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CarryPromptWidget.generated.h"
class UTextBlock;
class ACargoCrate;

UCLASS()
class AIRLOCK_API UCarryPromptWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void UpdateCarry(ACargoCrate* Crate);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Prompt;
};
