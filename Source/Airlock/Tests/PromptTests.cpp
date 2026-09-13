// Copyright Airlock project. All Rights Reserved.
// M5-5: the crosshair prompt's routing rule - what E would do to the thing in reach.
#include "Misc/AutomationTest.h"
#include "Core/AirlockCharacter.h"
#include "Station/VoyageBoard.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPromptRoutingTest, "Airlock.Prompt.RoutesByHoldCarryAndTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPromptRoutingTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	FURL URL; World->InitializeActorsForPlay(URL);
	AVoyageBoard* Board = World->SpawnActor<AVoyageBoard>();
	TestNotNull(TEXT("Board spawned"), Board);
	if (Board)
	{
		TestEqual(TEXT("A press target's own prompt"), AAirlockCharacter::InteractPromptFor(Board, false, false).ToString(), FString(TEXT("View departures")));
		TestTrue(TEXT("Nothing while a hold runs (the hold widget has the prompt)"), AAirlockCharacter::InteractPromptFor(Board, false, true).IsEmpty());
		TestTrue(TEXT("Nothing for a plain target while carrying (E would put the cask down)"), AAirlockCharacter::InteractPromptFor(Board, true, false).IsEmpty());
	}
	TestTrue(TEXT("Nothing in reach"), AAirlockCharacter::InteractPromptFor(nullptr, false, false).IsEmpty());
	AActor* Plain = World->SpawnActor<AActor>();
	TestTrue(TEXT("A non-interactable says nothing"), AAirlockCharacter::InteractPromptFor(Plain, false, false).IsEmpty());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
