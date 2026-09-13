// Copyright Airlock project. All Rights Reserved.

#include "Core/AgentConsoleSubsystem.h"
#include "Airlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool UAgentConsoleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if WITH_EDITOR
	return Super::ShouldCreateSubsystem(Outer);
#else
	return false;
#endif
}

FString UAgentConsoleSubsystem::CommandFilePath()
{
	return CommandFilePathFor(INDEX_NONE);
}

FString UAgentConsoleSubsystem::CommandFilePathFor(int32 PIEInstance)
{
	const FString File = PIEInstance > 0 ? FString::Printf(TEXT("commands.client%d.txt"), PIEInstance) : FString(TEXT("commands.txt"));
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AgentConsole"), File);
}

FString UAgentConsoleSubsystem::EchoPrefixFor(int32 PIEInstance)
{
	return PIEInstance > 0 ? FString::Printf(TEXT("AgentConsole client %d"), PIEInstance) : FString(TEXT("AgentConsole"));
}

void UAgentConsoleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const FWorldContext* Context = GetGameInstance() ? GetGameInstance()->GetWorldContext() : nullptr;
	Instance = Context ? Context->PIEInstance : INDEX_NONE;
	WatchedPath = CommandFilePathFor(Instance);
	Prefix = EchoPrefixFor(Instance);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAgentConsoleSubsystem::Poll), 0.5f);
	UE_LOG(LogAirlock, Log, TEXT("%s: watching %s"), *Prefix, *WatchedPath);
}

void UAgentConsoleSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Super::Deinitialize();
}

bool UAgentConsoleSubsystem::Poll(float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (World && PC && ReadyWorld.Get() != World)
	{
		ReadyWorld = World;
		UE_LOG(LogAirlock, Log, TEXT("%s: ready (map=%s)"), *Prefix, *World->GetMapName());
	}

	if (!IFileManager::Get().FileExists(*WatchedPath))
	{
		return true;
	}
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *WatchedPath))
	{
		return true; // still being written; try again next tick
	}
	IFileManager::Get().Delete(*WatchedPath);

	for (FString& Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}
		UE_LOG(LogAirlock, Log, TEXT("%s: %s"), *Prefix, *Line);
		if (PC)
		{
			PC->ConsoleCommand(Line);
		}
		else if (GEngine && World)
		{
			GEngine->Exec(World, *Line);
		}
	}
	return true;
}
