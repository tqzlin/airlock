// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/Interactable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"
#include "Ship/ShipLine.h"
#include "Voyage/VoyageSubsystem.h"
#include "UI/VoyageBoardWidget.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FCargoTestWorld
{
	UWorld* World;
	AAirlockGameMode* Mode;
	AAirlockGameState* State;
	explicit FCargoTestWorld(FName Section = TEXT("Hauler_Mk1"))
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
	~FCargoTestWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoPublicationTest, "Airlock.Cargo.PublicationAndSectionSpawning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoPublicationTest::RunTest(const FString& Parameters)
{
	FCargoTestWorld Ship;
	TestEqual(TEXT("Four published crates"), Ship.State->Cargo.Num(), 4);
	int32 Crates = 0, Slots = 0;
	for (TActorIterator<ACargoCrate> It(Ship.World); It; ++It)
	{
		++Crates;
		TestTrue(TEXT("Crates replicate movement"), It->GetIsReplicated() && It->IsReplicatingMovement());
		TestTrue(TEXT("Crates do not block pawns"), It->Body->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);
	}
	for (TActorIterator<ACargoSlot> It(Ship.World); It; ++It) { ++Slots; }
	TestEqual(TEXT("Four crate actors"), Crates, 4);
	TestEqual(TEXT("Four slot actors"), Slots, 4);
	int32 Notifications = 0;
	const auto Handle = Ship.State->OnShipGraphChanged.AddLambda([&Notifications] { ++Notifications; });
	Ship.State->PublishCargo(Ship.State->Cargo);
	TestEqual(TEXT("Identical cargo no notification"), Notifications, 0);
	Ship.Mode->CargoSetTemperature(TEXT("C2"), 41.9f);
	TestEqual(TEXT("Change notifies once on authority"), Notifications, 1);
	TestEqual(TEXT("Temperature floored for replication"), Ship.State->Cargo[1].Temperature, 41.f);
	Ship.Mode->CargoSetTemperature(TEXT("C2"), 41.95f);
	TestEqual(TEXT("Same whole degree not republished"), Notifications, 1);
	Ship.Mode->CargoPickUp(TEXT("C2"));
	TestEqual(TEXT("New state notified"), Notifications, 2);
	Ship.State->OnShipGraphChanged.Remove(Handle);
	FCargoTestWorld Hub(TEXT("StationHub"));
	TestTrue(TEXT("Hub cargo empty"), Hub.State->Cargo.IsEmpty());
	TestNull(TEXT("Hub spawns no crates"), Hub.Find<ACargoCrate>(TEXT("C1")));
	TestEqual(TEXT("Hub summary"), Hub.Mode->CargoSummary(), FString(TEXT("Cargo empty")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoCarryRulesTest, "Airlock.Cargo.PickupRangeHandsSpeedAndHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoCarryRulesTest::RunTest(const FString& Parameters)
{
	FCargoTestWorld Ship;
	auto* Crate = Ship.Find<ACargoCrate>(TEXT("C1"));
	if (!TestNotNull(TEXT("Crate"), Crate)) { return false; }
	auto* Crew = Ship.CrewAt(FVector::ZeroVector);
	const float Base = Crew->GetCharacterMovement()->MaxWalkSpeed;
	Crew->ServerPickUp(Crate);
	TestNull(TEXT("Distant pickup rejected"), Crew->CarriedCrate.Get());
	Crew->SetActorLocation(Crate->GetActorLocation() + FVector(0, -150, 96));
	IInteractable::Execute_Interact(Crate, Crew);
	TestTrue(TEXT("Press picks up in range"), Crew->CarriedCrate == Crate);
	TestTrue(TEXT("Attached to default subobject socket"), Crate->GetRootComponent()->GetAttachParent() == Crew->CarrySocket);
	TestTrue(TEXT("Model carried"), Crate->GetCargoState()->State == ECargoState::Carried);
	TestTrue(TEXT("Carried prompt empty"), IInteractable::Execute_GetInteractPrompt(Crate).IsEmpty());
	for (int32 I = 0; I < 2; ++I) { Crew->OnRep_CarriedCrate(); }
	TestEqual(TEXT("Speed assigned, never compounded"), Crew->GetCharacterMovement()->MaxWalkSpeed, Base * Crew->CarrySpeedFactor);
	Crew->ServerPickUp(Ship.Find<ACargoCrate>(TEXT("C2")));
	TestTrue(TEXT("Hands full retains first crate"), Crew->CarriedCrate == Crate);
	auto* Second = Ship.CrewAt(Crew->GetActorLocation());
	Second->ServerPickUp(Crate);
	TestNull(TEXT("Other pawn cannot take carried crate"), Second->CarriedCrate.Get());
	auto* Line = Ship.Find<AShipLine>(TEXT("Coolant_1"));
	Ship.Mode->ShipSetLineFault(TEXT("Coolant_1"), EShipLineFault::Leak);
	Crew->ServerBeginHold(Line);
	TestNull(TEXT("Hands full refuses hold"), Crew->GetHoldTarget());
	Crew->ServerPutDown();
	Crew->OnRep_CarriedCrate(); Crew->OnRep_CarriedCrate();
	TestEqual(TEXT("Drop restores baseline"), Crew->GetCharacterMovement()->MaxWalkSpeed, Base);
	TestNull(TEXT("Drop clears carrier"), Crew->CarriedCrate.Get());
	TestNull(TEXT("Detached"), Crate->GetAttachParentActor());
	TestTrue(TEXT("Loose again"), Crate->GetCargoState()->State == ECargoState::Loose);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoPlacementTest, "Airlock.Cargo.PutDownFloorAndSlotPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoPlacementTest::RunTest(const FString& Parameters)
{
	FCargoTestWorld Ship;
	Ship.AddFloor();
	auto* Crate = Ship.Find<ACargoCrate>(TEXT("C1"));
	Crate->SetActorLocation(FVector(50, 0, 0));
	auto* Crew = Ship.CrewAt(FVector(0, 0, 150));
	Crew->ServerPickUp(Crate); Crew->ServerPutDown();
	TestTrue(TEXT("Drops 120 forward onto traced floor"), Crate->GetActorLocation().Equals(FVector(120, 0, 0), .1));
	auto* Pad = Ship.Find<ACargoSlot>(TEXT("Clamp_1"));
	Crew->ServerPickUp(Crate); Crew->ServerPlace(Pad);
	TestTrue(TEXT("Distant slot refuses place"), Crew->CarriedCrate == Crate);
	Crew->SetActorLocation(Pad->GetActorLocation() + FVector(0, 150, 96));
	IInteractable::Execute_Interact(Pad, Crew);
	TestNull(TEXT("Placed hands empty"), Crew->CarriedCrate.Get());
	TestEqual(TEXT("Placement restores speed"), Crew->GetCharacterMovement()->MaxWalkSpeed, 450.f);
	TestTrue(TEXT("Secured"), Crate->GetCargoState()->State == ECargoState::Secured);
	TestTrue(TEXT("Rests on slot offset"), Crate->GetActorLocation().Equals(Pad->GetActorTransform().TransformPosition(Pad->CrateRestOffset)));
	TestEqual(TEXT("Slot mirror derives occupancy"), Pad->GetCrateId(), FName(TEXT("C1")));
	TestTrue(TEXT("Occupied pad has no prompt"), IInteractable::Execute_GetInteractPrompt(Pad).IsEmpty());
	const FVector Rest = Crate->GetActorLocation();
	Ship.Mode->CargoRelease(Pad->GetSlotId());
	TestTrue(TEXT("Release leaves crate on pad"), Crate->GetActorLocation().Equals(Rest));
	Crew->ServerPickUp(Crate);
	Crew->SetActorLocation(FVector(3000, 3000, 196));
	Crew->ServerPutDown();
	TestTrue(TEXT("Missed trace uses pawn feet at forward spot"), Crate->GetActorLocation().Equals(FVector(3120, 3000, 100)));
	return true;
}

namespace
{
class FCargoVentCarrierCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	FCargoTestWorld Ship;
	AAirlockCharacter* Crew;
	ACargoCrate* Crate;
	int32 Step = 0;
public:
	explicit FCargoVentCarrierCommand(FAutomationTestBase* InTest) : Test(InTest)
	{
		Crate = Ship.Find<ACargoCrate>(TEXT("C2"));
		Crew = Ship.CrewAt(Crate->GetActorLocation() + FVector(0, -150, 96));
		Crew->ServerPickUp(Crate);
		Ship.Mode->CargoSetTemperature(TEXT("C2"), 100);
	}
	virtual bool Update() override
	{
		Ship.World->GetTimerManager().Tick(1.f);
		if (Step == 1)
		{
			Test->TestTrue(TEXT("No voyage means no cargo tick"), Crate->GetCargoState()->State == ECargoState::Carried);
			Ship.State->bVoyageActive = true;
		}
		if (Step == 2)
		{
			Test->TestTrue(TEXT("Active timer vents"), Crate->GetCargoState()->State == ECargoState::Vented);
			Test->TestNull(TEXT("Venting frees carrier in publication"), Crew->CarriedCrate.Get());
			Test->TestNull(TEXT("Vented crate detached"), Crate->GetAttachParentActor());
			Test->TestEqual(TEXT("Venting restores speed"), Crew->GetCharacterMovement()->MaxWalkSpeed, 450.f);
			Test->TestTrue(TEXT("Vented crate dropped nearby"), FVector::Dist(Crate->GetActorLocation(), Crew->GetActorLocation()) < 200);
			Crew->ServerPickUp(Crate);
			Test->TestNull(TEXT("Vented pickup refused"), Crew->CarriedCrate.Get());
			return true;
		}
		++Step; return false;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoVentCarrierTest, "Airlock.Cargo.VoyageTimerVentsAndFreesCarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoVentCarrierTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCargoVentCarrierCommand(this)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoTraceTest, "Airlock.Cargo.InteractTraceIgnoresCarriedCrate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoTraceTest::RunTest(const FString& Parameters)
{
	FCargoTestWorld Ship;
	auto* Crew = Ship.CrewAt(FVector(0, 0, 96));
	auto* Crate = Ship.Find<ACargoCrate>(TEXT("C1"));
	Crate->SetActorLocation(FVector(50, 0, 0));
	Crew->ServerPickUp(Crate);
	// Put a cube body directly in the sight line; the production trace must still find the empty pad beyond it.
	auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Crate->Body->SetStaticMesh(Cube);
	Crate->Body->SetRelativeLocation(FVector(0, 0, 84));
	auto* Pad = Ship.Find<ACargoSlot>(TEXT("Clamp_1"));
	Pad->Pad->SetStaticMesh(Cube);
	Pad->SetActorLocation(FVector(200, 0, 160));
	FHitResult RawHit;
	FCollisionQueryParams Params(TEXT("CargoTraceControl"), false, Crew);
	Ship.World->LineTraceSingleByChannel(RawHit, FVector(0, 0, 160), FVector(250, 0, 160), ECC_Visibility, Params);
	TestTrue(TEXT("Control trace hits carried crate"), RawHit.GetActor() == Crate);
	TestTrue(TEXT("Interaction trace finds pad"), Crew->TraceInteractTarget() == Pad);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoDestroyedCarrierTest, "Airlock.Cargo.DestroyedPawnDropsCargo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoDestroyedCarrierTest::RunTest(const FString& Parameters)
{
	FCargoTestWorld Ship;
	auto* Crate = Ship.Find<ACargoCrate>(TEXT("C1"));
	auto* Crew = Ship.CrewAt(Crate->GetActorLocation() + FVector(0, -150, 96));
	Crew->ServerPickUp(Crate);
	const FVector Before = Crew->GetActorLocation();
	Crew->Destroy();
	TestTrue(TEXT("Crate survives pawn destruction"), IsValid(Crate));
	TestTrue(TEXT("Destroyed carrier drops as loose"), Crate->GetCargoState()->State == ECargoState::Loose);
	TestNull(TEXT("Crate no longer attached"), Crate->GetAttachParentActor());
	TestTrue(TEXT("Dropped nearby"), FVector::Dist(Crate->GetActorLocation(), Before) < 200);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoUnbookedResultTest, "Airlock.Cargo.UnbookedSectionRecordsNothingAndResultText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCargoUnbookedResultTest::RunTest(const FString& Parameters)
{
	FCargoTestWorld Ship;
	Ship.World->GetGameInstance()->Init();
	Ship.Mode->RecordVoyageResult();
	FVoyageResult Result;
	TestFalse(TEXT("Ship entered without a clock records nothing"), Ship.World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>()->GetLastVoyageResult(Result));
	Result.CratesTotal = 4; Result.CratesIntact = 2; Result.ShipCondition = 99.7f; Result.Pay = 400;
	TestEqual(TEXT("Hub board rounds only display hull"), UVoyageBoardWidget::DescribeResult(Result).ToString(), FString(TEXT("Last run: 2/4 crates intact - hull 100% - pay 400 cr")));
	Ship.World->GetGameInstance()->Shutdown();
	return true;
}
#endif
