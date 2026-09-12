// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AgentConsoleSubsystem.generated.h"

/**
 * Headless console for agents (editor builds only). Every 0.5 s it looks for Saved/AgentConsole/commands.txt,
 * runs each non-empty line as a console command on the first local player controller (or the engine when there is
 * no controller yet), logs it as "AgentConsole: <line>" and deletes the file. No Slate, no keyboard focus: the
 * editor can sit minimized or behind other windows while a script drives PIE. Lines starting with '#' are ignored.
 */
UCLASS()
class AIRLOCK_API UAgentConsoleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Saved/AgentConsole/commands.txt under the project. */
	static FString CommandFilePath();

private:
	bool Poll(float DeltaTime);
	FTSTicker::FDelegateHandle TickerHandle;
};
