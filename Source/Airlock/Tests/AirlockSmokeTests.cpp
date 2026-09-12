// Copyright Airlock project. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockPlayerController.h"
#include "Core/Interactable.h"
#include "Ship/AirlockDoor.h"
#include "Station/VoyageBoard.h"
#include "UI/VoyageBoardWidget.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockVoyageOffersTest, "Airlock.Voyage.OffersCoverEveryType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockVoyageOffersTest::RunTest(const FString& Parameters)
{
	// The board shows one offer per type, in a stable order, and the description names the captain.
	TArray<FVoyageSpec> Offers;
	Offers.Add(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Mining, 7));
	Offers.Add(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Pirates, 7));
	Offers.Add(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Derelict, 7));
	TestEqual(TEXT("Three offers"), Offers.Num(), 3);
	TestTrue(TEXT("Mining first"), Offers[0].Type == EVoyageType::Mining);
	TestTrue(TEXT("Pirates second"), Offers[1].Type == EVoyageType::Pirates);
	TestTrue(TEXT("Derelict third"), Offers[2].Type == EVoyageType::Derelict);

	const FString Line = UVoyageBoardWidget::DescribeOffer(Offers[0]).ToString();
	TestTrue(TEXT("Description names the captain"), Line.Contains(Offers[0].Captain.ToString()));
	TestTrue(TEXT("Description names the ship"), Line.Contains(Offers[0].ShipClassId.ToString()));
	TestTrue(TEXT("Day seed is a day count, not ticks"), UVoyageSubsystem::TodaySeed() > 20000 && UVoyageSubsystem::TodaySeed() < 40000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAirlockStationInteractablesTest, "Airlock.Station.BoardAndDoorDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAirlockStationInteractablesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Voyage board is interactable"), AVoyageBoard::StaticClass()->ImplementsInterface(UInteractable::StaticClass()));
	TestFalse(TEXT("Airlock door is not something you 'use' - you walk through it"), AAirlockDoor::StaticClass()->ImplementsInterface(UInteractable::StaticClass()));

	const AAirlockDoor* Door = GetDefault<AAirlockDoor>();
	if (TestNotNull(TEXT("AirlockDoor CDO"), Door))
	{
		TestTrue(TEXT("Leaf lifts the kit distance"), FMath::IsNearlyEqual(Door->OpenDistance, 268.f));
		TestTrue(TEXT("Door replicates"), Door->GetIsReplicated());
		TestTrue(TEXT("Kit doorway mesh referenced softly"), Door->DoorwayMesh.ToSoftObjectPath().ToString().Contains(TEXT("SM_Doorway_Small_A")));
		TestFalse(TEXT("Plain doors are not gated by default"), Door->bRequiresBooking || Door->bAlwaysLocked);
		TestFalse(TEXT("Plain doors start unlocked"), Door->IsLocked());
	}
	TestTrue(TEXT("Hub section id"), AAirlockGameMode::HubSectionId == FName(TEXT("StationHub")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
