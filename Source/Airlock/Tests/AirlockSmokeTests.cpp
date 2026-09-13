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
	const FVoyageSpec A = UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Standard, 42);
	const FVoyageSpec B = UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Standard, 42);
	const FVoyageSpec C = UVoyageSubsystem::MakeVoyageSpec(EVoyageType::MilkRun, 42);

	TestTrue(TEXT("Same (type, seed) gives the same captain"), A.Captain == B.Captain);
	TestTrue(TEXT("Same (type, seed) gives the same crew capacity"), A.CrewCapacity == B.CrewCapacity);
	TestTrue(TEXT("Type is preserved"), A.Type == EVoyageType::Standard);
	TestTrue(TEXT("Voyage lasts 20 minutes"), FMath::IsNearlyEqual(A.DurationSeconds, 1200.f));
	// M3-7: the tiers size the crew and the manifest; every tier flies the hauler.
	TestTrue(TEXT("Standard run seats four"), A.CrewCapacity == 4 && C.CrewCapacity == 2);
	TestTrue(TEXT("Every tier flies the hauler"), A.ShipClassId == C.ShipClassId && A.ShipClassId == FName(TEXT("Hauler_Mk1")));
	TestTrue(TEXT("Tiers size the manifest"), A.CaskCount == 8 && C.CaskCount == 4);
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
	Offers.Add(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::MilkRun, 7));
	Offers.Add(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Standard, 7));
	Offers.Add(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::Priority, 7));
	TestEqual(TEXT("Three offers"), Offers.Num(), 3);
	TestTrue(TEXT("Milk run first"), Offers[0].Type == EVoyageType::MilkRun);
	TestTrue(TEXT("Standard second"), Offers[1].Type == EVoyageType::Standard);
	TestTrue(TEXT("Priority third"), Offers[2].Type == EVoyageType::Priority);

	const FString Line = UVoyageBoardWidget::DescribeOffer(Offers[0]).ToString();
	TestTrue(TEXT("Description names the captain"), Line.Contains(Offers[0].Captain.ToString()));
	TestTrue(TEXT("Description names the tier and the casks"), UVoyageBoardWidget::DescribeOffer(Offers[2]).ToString().Contains(TEXT("12 casks")) && UVoyageBoardWidget::DescribeOffer(Offers[2]).ToString().Contains(TEXT("Priority run")));
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
