// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AirlockSaveGame.generated.h"

/** The Rig's persistent state in the MVP: one slot per machine, replaced by a server-side store in M4. */
UCLASS()
class AIRLOCK_API UAirlockSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	static constexpr int32 DefaultBaseFuelTonnes = 200;
	UPROPERTY() int32 BaseFuelTonnes = DefaultBaseFuelTonnes;
	UPROPERTY() int32 RunsCompleted = 0;
};
