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
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AgentConsole"), TEXT("commands.txt"));
}

void UAgentConsoleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAgentConsoleSubsystem::Poll), 0.5f);
	UE_LOG(LogAirlock, Log, TEXT("AgentConsole: watching %s"), *CommandFilePath());
}

void UAgentConsoleSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Super::Deinitialize();
}

bool UAgentConsoleSubsystem::Poll(float /*DeltaTime*/)
{
	const FString Path = CommandFilePath();
	if (!IFileManager::Get().FileExists(*Path))
	{
		return true;
	}
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		return true; // still being written; try again next tick
	}
	IFileManager::Get().Delete(*Path);

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	for (FString& Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}
		UE_LOG(LogAirlock, Log, TEXT("AgentConsole: %s"), *Line);
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
