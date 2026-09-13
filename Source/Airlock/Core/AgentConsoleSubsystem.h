// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AgentConsoleSubsystem.generated.h"

/**
 * Headless console for agents (editor builds only). Every 0.5 s it looks for its command file under
 * Saved/AgentConsole, runs each non-empty line as a console command on the first local player controller (or the
 * engine when there is no controller yet), logs it as "AgentConsole: <line>" and deletes the file. No Slate, no
 * keyboard focus: the editor can sit minimized or behind other windows while a script drives PIE. Lines starting
 * with '#' are ignored.
 *
 * One file per game instance (M4-1): PIE instance 0 (the server, or the only player) and any non-PIE game poll
 * commands.txt; PIE client N polls commands.clientN.txt and echoes as "AgentConsole client N: <line>". Every
 * instance logs "<prefix>: watching <path>" when it starts and "<prefix>: ready (map=<world>)" each time it has a
 * player controller in a new world, so a script can wait for a client before driving it.
 */
UCLASS()
class AIRLOCK_API UAgentConsoleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Saved/AgentConsole/commands.txt under the project: the primary file (PIE instance 0 or a non-PIE game). */
	static FString CommandFilePath();

	/** The file a game instance polls: commands.txt for PIE instance 0 or none, commands.client<N>.txt for client N. */
	static FString CommandFilePathFor(int32 PIEInstance);

	/** The log prefix of an instance: "AgentConsole" or "AgentConsole client <N>". */
	static FString EchoPrefixFor(int32 PIEInstance);

private:
	bool Poll(float DeltaTime);
	FTSTicker::FDelegateHandle TickerHandle;
	int32 Instance = INDEX_NONE;
	FString WatchedPath;
	FString Prefix;
	TWeakObjectPtr<UWorld> ReadyWorld;
};
