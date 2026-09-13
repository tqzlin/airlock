// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Ship/ShipVisuals.h"
#include "Ship/ShipModule.h"
#include "Ship/CargoLayouts.h"
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FLegibilityTestWorld
{
	UWorld* World;
	AAirlockGameMode* Mode;
	AAirlockGameState* State;
	explicit FLegibilityTestWorld(FName Section = TEXT("Hauler_Mk1"))
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World));
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>();
		State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = Section;
		World->BeginPlay();
	}
	~FLegibilityTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	template<class T> T* Find(FName Id) const
	{
		for (TActorIterator<T> It(World); It; ++It) { if (It->GetFName() == Id) { return *It; } }
		return nullptr;
	}
	AAirlockCharacter* CrewAt(FVector Location) const
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Crew = World->SpawnActor<AAirlockCharacter>(Params);
		Crew->SetActorLocation(Location);
		Crew->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		return Crew;
	}
	void AddFloor() const
	{
		auto* Floor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Floor);
		Floor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(1000, 1000, 10));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0, 0, -10));
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConfigureLabelTest, "Airlock.Legibility.ConfigureLabel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FConfigureLabelTest::RunTest(const FString& Parameters)
{
    FLegibilityTestWorld Env(TEXT("StationHub"));
    auto* Actor = Env.World->SpawnActor<AActor>();
    auto* Root = NewObject<USceneComponent>(Actor);
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();
    Actor->SetActorRotation(FRotator(0, 35, 0));
    auto* Label = NewObject<UTextRenderComponent>(Actor);
    Label->SetupAttachment(Root);
    Label->RegisterComponent();
    const FVector Offset(4, 5, 210);
    const FLinearColor Colour(1.f, .6f, .1f);
    ShipVisuals::ConfigureLabel(Label, 22.f, -90.f, Colour, Offset);
    TestEqual(TEXT("World size"), Label->WorldSize, 22.f);
    TestTrue(TEXT("Horizontal centre"), Label->HorizontalAlignment == EHTA_Center);
    TestTrue(TEXT("Vertical text centre"), Label->VerticalAlignment == EVRTA_TextCenter);
    TestEqual(TEXT("Offset unchanged"), Label->GetRelativeLocation(), Offset);
    TestTrue(TEXT("Yaw relative to current owner"), FMath::IsNearlyEqual(Label->GetRelativeRotation().Yaw, -125.0));
    TestTrue(TEXT("World facing"), FMath::IsNearlyEqual(Label->GetComponentRotation().Yaw, -90.0));
    TestEqual(TEXT("Linear colour converted to sRGB"), Label->TextRenderColor, Colour.ToFColor(true));
    TestTrue(TEXT("No collision"), Label->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    Actor->SetActorRotation(FRotator(0, -50, 0));
    ShipVisuals::ConfigureLabel(Label, 22.f, -90.f, Colour, Offset);
    TestTrue(TEXT("Reapply uses new owner yaw"), FMath::IsNearlyEqual(Label->GetRelativeRotation().Yaw, -40.0));
    auto* Ownerless = NewObject<UTextRenderComponent>();
    ShipVisuals::ConfigureLabel(Ownerless, 34.f, 90.f, FLinearColor::White, FVector::ZeroVector);
    TestTrue(TEXT("Ownerless yaw"), FMath::IsNearlyEqual(Ownerless->GetRelativeRotation().Yaw, 90.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegibilityLifeSupportTest, "Airlock.Legibility.LifeSupportKeepsScrubberLine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLegibilityLifeSupportTest::RunTest(const FString& Parameters)
{
    FLegibilityTestWorld Env;
    auto* Life = Env.Find<AShipModule>(TEXT("LifeSupport"));
    if (!TestNotNull(TEXT("Life support spawned"), Life)) { return false; }
    TestEqual(TEXT("Id, state and scrubber on separate lines"), Life->Label->Text.ToString(), FString(TEXT("LifeSupport\nNominal  100%\nScrubbers Normal")));
    Env.Mode->AirSetScrubMode(EScrubMode::Hazard);
    TestEqual(TEXT("Hazard mode still updates third line"), Life->Label->Text.ToString(), FString(TEXT("LifeSupport\nNominal  100%\nScrubbers Hazard")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoLabelYawTest, "Airlock.Legibility.CargoLabelsFaceWalkway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoLabelYawTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("North faces south"), CargoLayouts::LabelYawFor(1560.001), -90.f);
	TestEqual(TEXT("Centre faces north"), CargoLayouts::LabelYawFor(1560), 90.f);
	TestEqual(TEXT("South faces north"), CargoLayouts::LabelYawFor(1559.999), 90.f);
	TestEqual(TEXT("Hand-built crate default"), FCargoCrateSpec().LabelYaw, 180.f);
	TestEqual(TEXT("Hand-built slot default"), FCargoSlotSpec().LabelYaw, 180.f);
	const auto Layout = CargoLayouts::Build(TEXT("Hauler_Mk1"), 12, 6);
	TestEqual(TEXT("All twelve crate spots"), Layout.Crates.Num(), 12);
	TestEqual(TEXT("All ten slot spots"), Layout.Slots.Num(), 10);
	for (const auto& Crate : Layout.Crates)
	{
		TestEqual(*Crate.Id.ToString(), Crate.LabelYaw, Crate.Transform.GetLocation().Y > 1560 ? -90.f : 90.f);
	}
	for (const auto& Slot : Layout.Slots)
	{
		TestEqual(*Slot.Id.ToString(), Slot.LabelYaw, Slot.Transform.GetLocation().Y > 1560 ? -90.f : 90.f);
		if (Slot.bRack) { TestEqual(TEXT("Rack faces north"), Slot.LabelYaw, 90.f); }
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrateLabelRulesTest, "Airlock.Legibility.CarriedDroppedRackedAndTerminalLabels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCrateLabelRulesTest::RunTest(const FString& Parameters)
{
	FLegibilityTestWorld Env;
	auto* Crate = Env.Find<ACargoCrate>(TEXT("C2"));
	auto* Rack = Env.Find<ACargoSlot>(TEXT("ColdRack"));
	if (!TestNotNull(TEXT("Cask spawned"), Crate) || !TestNotNull(TEXT("Rack spawned"), Rack)) { return false; }
	TestEqual(TEXT("White id on first line"), Crate->Label->Text.ToString(), FString(TEXT("C2")));
	TestEqual(TEXT("Volatile state on second line"), Crate->StateLabel->Text.ToString(), FString(TEXT("Loose  0C")));
	TestEqual(TEXT("Id height retained"), Crate->Label->GetRelativeLocation().Z, 95.0);
	for (float Temperature : { 39.f, 40.f, 79.f, 80.f })
	{
		Env.Mode->CargoSetTemperature(TEXT("C2"), Temperature);
		const FLinearColor Colour = Temperature >= 80 ? FLinearColor(1.f, .15f, .1f)
			: Temperature >= 40 ? FLinearColor(1.f, .6f, .1f) : FLinearColor::White;
		TestEqual(TEXT("Heat colour on state only"), Crate->StateLabel->TextRenderColor, Colour.ToFColor(true));
		TestEqual(TEXT("Id stays white"), Crate->Label->TextRenderColor, FColor::White);
	}
	auto* Crew = Env.CrewAt(Crate->GetActorLocation() + FVector(-150, 0, 96));
	Crew->SetActorRotation(FRotator(0, 37, 0));
	Crew->ServerPickUp(Crate);
	TestTrue(TEXT("Picked up through server verb"), Crew->CarriedCrate == Crate);
	TestFalse(TEXT("Carried id hidden"), Crate->Label->IsVisible());
	TestFalse(TEXT("Carried state hidden"), Crate->StateLabel->IsVisible());
	Crew->ServerPutDown();
	TestTrue(TEXT("Drop restores both lines"), Crate->Label->IsVisible() && Crate->StateLabel->IsVisible());
	TestTrue(TEXT("Drop uses pawn yaw"), FMath::IsNearlyEqual(Crate->GetActorRotation().Yaw, 37.0));
	TestTrue(TEXT("Drop retains world label facing"), FMath::IsNearlyEqual(Crate->Label->GetComponentRotation().Yaw, -90.0));
	Crew->ServerPickUp(Crate);
	Crew->SetActorLocation(Rack->GetActorLocation() + FVector(0, 150, 96));
	Crew->ServerPlace(Rack);
	TestTrue(TEXT("Placed through server verb"), Crate->GetCargoState()->State == ECargoState::Racked);
	TestTrue(TEXT("Rack restores both lines"), Crate->Label->IsVisible() && Crate->StateLabel->IsVisible());
	TestTrue(TEXT("Rack takes rack yaw"), FMath::IsNearlyEqual(Crate->GetActorRotation().Yaw, 90.0));
	TestTrue(TEXT("Rack keeps id world facing"), FMath::IsNearlyEqual(Crate->Label->GetComponentRotation().Yaw, -90.0));
	TestTrue(TEXT("Rack keeps state world facing"), FMath::IsNearlyEqual(Crate->StateLabel->GetComponentRotation().Yaw, -90.0));
	// Replicated terminal snapshots exercise visuals without changing any model or gameplay helper.
	auto Cargo = Env.State->Cargo;
	auto* State = Cargo.FindByPredicate([](const auto& C) { return C.Id == TEXT("C2"); });
	State->State = ECargoState::Vented; State->Temperature = 100;
	Env.State->PublishCargo(Cargo);
	TestTrue(TEXT("Vented label remains"), Crate->Label->IsVisible() && Crate->StateLabel->IsVisible());
	TestEqual(TEXT("Vented state red"), Crate->StateLabel->TextRenderColor, FLinearColor(1.f, .15f, .1f).ToFColor(true));
	State->State = ECargoState::Burned;
	Env.State->PublishCargo(Cargo);
	TestTrue(TEXT("Burned actor hidden"), Crate->IsHidden());
	TestFalse(TEXT("Burned id hidden"), Crate->Label->IsVisible());
	TestFalse(TEXT("Burned state hidden"), Crate->StateLabel->IsVisible());
	return true;
}
#endif
