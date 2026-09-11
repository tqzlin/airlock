// Copyright Airlock project. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockPlayerController.h"
#include "Voyage/VoyageSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockVoyageSpecDeterminismTest, "Airlock.Voyage.SpecIsDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockVoyageSpecDeterminismTest::RunTest(const FString& Parameters)
{
	const FVoyageSpec A = UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Pirates, 42);
	const FVoyageSpec B = UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Pirates, 42);
	const FVoyageSpec C = UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Mining, 42);

	TestTrue(TEXT("Same (type, seed) gives the same captain"), A.Captain == B.Captain);
	TestTrue(TEXT("Same (type, seed) gives the same crew capacity"), A.CrewCapacity == B.CrewCapacity);
	TestTrue(TEXT("Type is preserved"), A.Type == EVoyageType::Pirates);
	TestTrue(TEXT("Voyage lasts 20 minutes"), FMath::IsNearlyEqual(A.DurationSeconds, 1200.f));
	TestTrue(TEXT("Crew capacity is within the MVP band"), A.CrewCapacity >= 4 && A.CrewCapacity <= 8);
	TestTrue(TEXT("Different types pick different ships"), A.ShipClassId != C.ShipClassId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockGameModeDefaultsTest, "Airlock.Core.GameModeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockGameModeDefaultsTest::RunTest(const FString& Parameters)
{
	const AAirlockGameMode* GameMode = GetDefault<AAirlockGameMode>();
	if (!TestNotNull(TEXT("AirlockGameMode CDO"), GameMode))
	{
		return false;
	}

	TestTrue(TEXT("Default pawn is AirlockCharacter"), GameMode->DefaultPawnClass == AAirlockCharacter::StaticClass());
	TestTrue(TEXT("Player controller is AirlockPlayerController"), GameMode->PlayerControllerClass == AAirlockPlayerController::StaticClass());
	TestTrue(TEXT("Game state is AirlockGameState"), GameMode->GameStateClass == AAirlockGameState::StaticClass());
	TestTrue(TEXT("Seamless travel enabled for airlock transitions"), GameMode->bUseSeamlessTravel);

	const AAirlockGameState* GameState = GetDefault<AAirlockGameState>();
	TestTrue(TEXT("Fresh section has life-support capacity"), GameState && GameState->HasLifeSupportCapacity());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
