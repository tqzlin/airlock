// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the crew can use with the Interact key (E): voyage boards, consoles, salvage. The character
 * line-traces from the camera and calls Interact on the first actor that implements this. Interact runs on the
 * machine that pressed the key; implementations that change game state must route through a server RPC.
 */
class AIRLOCK_API IInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Airlock|Interaction")
	void Interact(APawn* InstigatorPawn);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Airlock|Interaction")
	FText GetInteractPrompt() const;
};
