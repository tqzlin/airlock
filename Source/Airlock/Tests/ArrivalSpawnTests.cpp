// Copyright Airlock project. All Rights Reserved.
// M4-1: crew arriving through one PlayerStart step aside instead of failing to spawn (found by the two-client proof).
#include "Misc/AutomationTest.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "Components/BoxComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FArrivalTestWorld
{
	UWorld* World; AAirlockGameMode* Mode;
	FArrivalTestWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World)); World->GetGameInstance()->Init();
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>();
		World->GetGameState<AAirlockGameState>()->CurrentSectionId = TEXT("StationHub"); World->BeginPlay();
	}
	~FArrivalTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->GetGameInstance()->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArrivalRowTest, "Airlock.Multiplayer.ArrivalsStepAsideAtOneStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArrivalRowTest::RunTest(const FString& Parameters)
{
	FArrivalTestWorld S;
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Yaw 0: the start's right axis is +Y, so the row forms along Y.
	APlayerStart* Start = S.World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(1000, 2000, 120), FRotator::ZeroRotator, Params);
	TestNotNull(TEXT("Start"), Start); if (!Start) { return false; }

	const double ExpectedY[] = { 2000.0, 2120.0, 1880.0, 2240.0, 1760.0 };
	for (int32 i = 0; i < 5; ++i)
	{
		auto* PC = S.World->SpawnActor<AAirlockPlayerController>(Params);
		S.Mode->RestartPlayer(PC);
		APawn* Pawn = PC->GetPawn();
		TestNotNull(*FString::Printf(TEXT("Arrival %d has a pawn"), i + 1), Pawn);
		if (!Pawn) { return false; }
		TestEqual(*FString::Printf(TEXT("Arrival %d X"), i + 1), Pawn->GetActorLocation().X, 1000.0, 1.0);
		TestEqual(*FString::Printf(TEXT("Arrival %d steps along the row"), i + 1), Pawn->GetActorLocation().Y, ExpectedY[i], 1.0);
	}

	// A sixth crew member finds no clear spot within two steps either side: still spawned, never a spectator.
	auto* Sixth = S.World->SpawnActor<AAirlockPlayerController>(Params);
	AddExpectedError(TEXT("forced at the start"), EAutomationExpectedErrorFlags::Contains, 1);
	S.Mode->RestartPlayer(Sixth);
	APawn* SixthPawn = Sixth->GetPawn();
	TestNotNull(TEXT("Sixth arrival still has a pawn"), SixthPawn);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArrivalBlockedStepTest, "Airlock.Multiplayer.ArrivalSkipsBlockedSideStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArrivalBlockedStepTest::RunTest(const FString& Parameters)
{
	FArrivalTestWorld S;
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerStart* Start = S.World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(1000, 2000, 120), FRotator::ZeroRotator, Params);
	TestNotNull(TEXT("Start"), Start); if (!Start) { return false; }
	// A wall where the first side step (+120 along +Y) would go: the second arrival takes the other side.
	AActor* Wall = S.World->SpawnActor<AActor>(AActor::StaticClass(), FVector(1000, 2120, 120), FRotator::ZeroRotator, Params);
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall, TEXT("Wall"));
	Box->SetBoxExtent(FVector(60, 60, 150));
	Box->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Wall->SetRootComponent(Box); Box->RegisterComponent(); Wall->SetActorLocation(FVector(1000, 2120, 120));

	auto* First = S.World->SpawnActor<AAirlockPlayerController>(Params); S.Mode->RestartPlayer(First);
	auto* Second = S.World->SpawnActor<AAirlockPlayerController>(Params); S.Mode->RestartPlayer(Second);
	APawn* FirstPawn = First->GetPawn(); APawn* SecondPawn = Second->GetPawn();
	TestNotNull(TEXT("First arrival has a pawn"), FirstPawn); TestNotNull(TEXT("Second arrival has a pawn"), SecondPawn);
	if (!FirstPawn || !SecondPawn) { return false; }
	TestEqual(TEXT("First at the start"), FirstPawn->GetActorLocation().Y, 2000.0, 1.0);
	TestEqual(TEXT("Second skips the walled side"), SecondPawn->GetActorLocation().Y, 1880.0, 1.0);
	return true;
}
#endif
