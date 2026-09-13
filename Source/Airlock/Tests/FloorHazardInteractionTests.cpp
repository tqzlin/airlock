// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockPlayerController.h"
#include "Ship/Spill.h"
#include "Ship/Still.h"
#include "Ship/CargoCrate.h"
#include "UI/TaskBoardWidget.h"
#include "Voyage/VoyageSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// FFloorTestWorld's driver, named separately for unity builds; StillTests' gravity-free crew helpers.
struct FHazardTestWorld
{
	UWorld* World; AAirlockGameMode* Mode; AAirlockGameState* State;
	explicit FHazardTestWorld(FName Section = RefineryLayouts::FloorSectionId())
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World)); World->GetGameInstance()->Init();
		auto* Voyages = World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>();
		FGuid Id; Voyages->RequestVoyage(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::MilkRun, 1), Id);
		FVoyageResult Ship; Ship.IntactCasks = { TEXT("C1"), TEXT("C2"), TEXT("C3") }; Ship.IntactLightCasks = { TEXT("C2") };
		Voyages->SetShipResult(Ship);
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>(); State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = Section; World->BeginPlay();
	}
	~FHazardTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->GetGameInstance()->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
	template<class T> int32 Count() const { int32 N = 0; for (TActorIterator<T> It(World); It; ++It) { ++N; } return N; }
	template<class T> T* Find(FName Name) const { for (TActorIterator<T> It(World); It; ++It) { if (It->GetFName() == Name) { return *It; } } return nullptr; }
	void Tick(float Seconds) const { for (int32 I = 0, N = FMath::RoundToInt(Seconds * 4.f); I < N; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, .25f); } }
	AAirlockCharacter* CrewAt(FVector Location) const
	{
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Crew = World->SpawnActor<AAirlockCharacter>(P); Crew->SetActorLocation(Location);
		Crew->GetCharacterMovement()->GravityScale = 0.f; return Crew;
	}
	ACargoCrate* Carry(AAirlockCharacter* Crew, FName Id)
	{
		auto* Crate = Find<ACargoCrate>(Id); Crew->SetActorLocation(Crate->GetActorLocation() + FVector(-150, 0, 96)); Crew->ServerPickUp(Crate); return Crate;
	}
	FVector SpillPoint(int32 Index = 1) const { return RefineryLayouts::SpillPoint(RefineryLayouts::Find(RefineryLayouts::FloorSectionId())->Fixtures[Index]); }
};
class FHazardLogCapture : public FOutputDevice
{
public:
	FHazardLogCapture() { GLog->AddOutputDevice(this); }
	~FHazardLogCapture() { GLog->RemoveOutputDevice(this); }
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{ if (Category == TEXT("LogAirlock")) { FScopeLock Lock(&Mutex); Lines += V; Lines += TEXT("\n"); } }
	FString Text() const { GLog->Flush(); FScopeLock Lock(&Mutex); return Lines; }
	int32 Count(const FString& Prefix) const
	{ TArray<FString> Out; Text().ParseIntoArrayLines(Out); int32 N = 0; for (const auto& L : Out) { if (L.StartsWith(Prefix)) { ++N; } } return N; }
private:
	mutable FCriticalSection Mutex; FString Lines;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardEventBodyTest, "Airlock.Floor.HazardInteraction.FloodEventSpawnsMirrorBody", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardEventBodyTest::RunTest(const FString& Parameters)
{
	FHazardTestWorld S; FHazardLogCapture Log;
	S.Mode->RefinerySetKnob(TEXT("Still_2"), ERefineryKnob::Heat, 6);
	S.Mode->RefinerySetKnob(TEXT("Still_2"), ERefineryKnob::Feed, 20);
	S.Mode->RefineryLoadHopper(TEXT("Still_2"), TEXT("C2"));
	for (int32 I = 0; I < 4 && S.State->Refinery.Stills[1].Status != EStillStatus::Running; ++I) { S.Tick(.25f); }
	TestEqual(TEXT("C2 running"), S.State->Refinery.Stills[1].Status, EStillStatus::Running);
	S.Tick(6.f);
	TestEqual(TEXT("One model flood"), S.State->Refinery.Spills, 1);
	TestEqual(TEXT("One hazard event"), S.State->FloorHazards.SpillsTotal, 1);
	TestEqual(TEXT("The full flood reaches hazards"), S.State->FloorHazards.LitresSpilled, 200.f);
	TestEqual(TEXT("One puddle body"), S.Count<ASpill>(), 1);
	auto* Spill = S.Find<ASpill>(TEXT("S1")); if (!TestNotNull(TEXT("Named S1"), Spill)) { return false; }
	TestTrue(TEXT("Unscaled spill point"), Spill->GetActorLocation().Equals(FVector(-5400, 1670, -65)));
	TestTrue(TEXT("Actor replicated"), Spill->GetIsReplicated());
	TestTrue(TEXT("Cylinder"), Spill->Body->GetStaticMesh()->GetPathName().Contains(TEXT("BasicShapes/Cylinder")));
	TestTrue(TEXT("Dynamic material"), Spill->Body->GetMaterial(0)->IsA<UMaterialInstanceDynamic>());
	TestEqual(TEXT("Deck clearance"), Spill->Body->GetRelativeLocation().Z, 1.0);
	TestEqual(TEXT("Disc thickness scale"), Spill->Body->GetRelativeScale3D().Z, .02, .0001);
	TestEqual(TEXT("Marker height"), Spill->Marker->GetUnscaledBoxExtent().Z, 30.0);
	TestEqual(TEXT("Marker ignores pawns"), Spill->Marker->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestEqual(TEXT("Disc ignores pawns"), Spill->Body->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestEqual(TEXT("Marker takes the E trace"), Spill->Marker->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);
	TestEqual(TEXT("Event logged once"), Log.Count(TEXT("Refinery Spill Still_2 C2 litres=200")), 1);
	TestEqual(TEXT("Puddle born at the full size"), Log.Count(TEXT("Spill S1 by Still_2: 200 L (r 200 cm)")), 1);
	TestEqual(TEXT("Flood cue"), Log.Count(TEXT("Okafor: Flood on Still 2")), 1);
	const auto& Mirror = S.State->FloorHazards.Spills[0];
	TestEqual(TEXT("Whole litres"), Mirror.Litres, FMath::FloorToFloat(Mirror.Litres));
	TestEqual(TEXT("Whole radius"), Mirror.RadiusCm, FMath::FloorToFloat(Mirror.RadiusCm));
	TestTrue(TEXT("Evaporation already shrinking"), Mirror.Litres < 200.f && Mirror.Litres > 190.f);
	// The actor must follow a mirror change independently of the game mode's precise model.
	FFloorHazardState Changed = S.State->FloorHazards; Changed.Spills[0].Litres = 42; Changed.Spills[0].RadiusCm = 81;
	S.State->PublishFloorHazards(Changed);
	TestEqual(TEXT("Mirror label"), Spill->Label->Text.ToString(), FString(TEXT("SPILL\n42 L")));
	TestEqual(TEXT("Mirror marker"), Spill->Marker->GetUnscaledBoxExtent().X, 81.0);
	TestEqual(TEXT("Mirror disc"), Spill->Body->GetRelativeScale3D().X, 81.0 / 50.0, .0001);
	FFloorFixtureSpec Rotated = RefineryLayouts::Find(RefineryLayouts::FloorSectionId())->Fixtures[1];
	Rotated.Transform = FTransform(FRotator(0, 90, 0), FVector(100, 200, 300), FVector(.8));
	TestTrue(TEXT("Offset rotates without scale"), RefineryLayouts::SpillPoint(Rotated).Equals(FVector(330, 200, 300)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardLifecycleTest, "Airlock.Floor.HazardInteraction.MergeEvaporationFumesAndSectionGates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardLifecycleTest::RunTest(const FString& Parameters)
{
	{
		FHazardTestWorld S; FHazardLogCapture Log;
		TestEqual(TEXT("New spill"), S.Mode->HazardSpill(TEXT("Still_2"), 200), FName(TEXT("S1")));
		TestEqual(TEXT("Merged id"), S.Mode->HazardSpill(TEXT("Still_2"), 200), FName(TEXT("S1")));
		TestEqual(TEXT("One body after merge"), S.Count<ASpill>(), 1);
		TestEqual(TEXT("Merge logs total"), Log.Count(TEXT("Spill S1 merged: 400 L")), 1);
		TestEqual(TEXT("Adjacent still never merges"), S.Mode->HazardSpill(TEXT("Still_1"), 200), FName(TEXT("S2")));
		TestEqual(TEXT("Two bodies"), S.Count<ASpill>(), 2);
		TestTrue(TEXT("Unknown still refused"), S.Mode->HazardSpill(TEXT("Tank"), 200).IsNone());
		TestTrue(TEXT("Invalid litres refused"), S.Mode->HazardSpill(TEXT("Still_2"), -1).IsNone());
		TestEqual(TEXT("Console puddles do not count in refinery tally"), S.Mode->RefineryTally().Spills, 0);
		// 2.5 ppm is away from the atmosphere mirror's float/whole-ppm boundary.
		S.Mode->AirSetScrubMode(EScrubMode::Off); S.Tick(25.f);
		TestEqual(TEXT("Two evaporating puddles fume the room"), S.State->Atmosphere.Ppm, 2.f);
		TestEqual(TEXT("25 L off each"), S.State->FloorHazards.Spills[0].Litres, 375.f);
		S.State->bVoyageActive = false; const auto Before = S.State->FloorHazards; S.Tick(10.f);
		TestTrue(TEXT("Stopped slot does not evaporate"), S.State->FloorHazards == Before);
	}
	{
		FHazardTestWorld S;
		S.Mode->HazardSpill(TEXT("Still_2"), 1); S.Tick(1.25f);
		TestTrue(TEXT("Evaporated mirror removed"), S.State->FloorHazards.Spills.IsEmpty());
		TestEqual(TEXT("Evaporated actor removed"), S.Count<ASpill>(), 0);
		TestEqual(TEXT("Historical totals retained"), S.State->FloorHazards.SpillsTotal, 1);
	}
	{
		FHazardTestWorld S(TEXT("Hauler_Mk1"));
		TestTrue(TEXT("No spill on ship"), S.Mode->HazardSpill(TEXT("Still_2"), 200).IsNone());
		TestEqual(TEXT("No ship puddle"), S.Count<ASpill>(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardCrewTest, "Airlock.Floor.HazardInteraction.SlowCompositionStumbleAndDwellReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardCrewTest::RunTest(const FString& Parameters)
{
	FHazardTestWorld S; FHazardLogCapture Log;
	S.Mode->HazardSpill(TEXT("Still_2"), 200); const FVector Point = S.SpillPoint();
	auto* Crew = S.CrewAt(Point + FVector(0, 0, 96)); S.Tick(.25f);
	TestEqual(TEXT("Feet select S1"), Crew->InSpill, FName(TEXT("S1")));
	TestEqual(TEXT("Slowed to 60 percent"), Crew->GetCharacterMovement()->MaxWalkSpeed, 270.f);
	auto* Crate = S.Carry(Crew, TEXT("C3")); Crew->SetActorLocation(Point + FVector(0, 0, 96));
	Crew->SetExposureServer(80); Crew->OnRep_InSpill(); Crew->OnRep_InSpill();
	TestEqual(TEXT("Carry, spill and exposure multiply without compounding"), Crew->GetCharacterMovement()->MaxWalkSpeed, 450.f * .6f * .6f * .7f, .001f);
	Crew->SetExposureServer(0); S.Tick(3.75f); TestTrue(TEXT("Still carried before four seconds"), Crew->CarriedCrate == Crate);
	S.Tick(.5f);
	TestNull(TEXT("4.25 seconds drops the cask"), Crew->CarriedCrate.Get());
	TestEqual(TEXT("Stumble total"), S.State->FloorHazards.Stumbles, 1);
	TestEqual(TEXT("Dropped loose"), Crate->GetCargoState()->State, ECargoState::Loose);
	TestEqual(TEXT("Drop remains in crude"), S.Mode->HazardContains(Crate->GetActorLocation()), FName(TEXT("S1")));
	TestEqual(TEXT("Carry factor removed, spill remains"), Crew->GetCharacterMovement()->MaxWalkSpeed, 270.f);
	TestTrue(TEXT("Stumble names cask and spill"), Log.Text().Contains(TEXT("drops C3 in S1")));
	TestEqual(TEXT("Drop verb once"), Log.Count(TEXT("PutDown C3 by")), 1);
	TestEqual(TEXT("Okafor stumble cue"), Log.Count(TEXT("Okafor: Cask down -")), 1);
	Crew->ServerPickUp(Crate); S.Tick(2.f);
	Crew->SetActorLocation(Point + FVector(0, -400, 96)); S.Tick(.25f);
	TestTrue(TEXT("Dry deck clears spill"), Crew->InSpill.IsNone());
	TestEqual(TEXT("Dry carrying speed"), Crew->GetCharacterMovement()->MaxWalkSpeed, 270.f);
	Crew->SetActorLocation(Point + FVector(0, 0, 96)); S.Tick(2.25f);
	TestTrue(TEXT("Leaving reset dwell"), Crew->CarriedCrate == Crate);
	Crew->ServerPutDown(); Crew->ServerPickUp(Crate); S.Tick(2.25f);
	TestTrue(TEXT("A manual drop also reset dwell"), Crew->CarriedCrate == Crate);
	S.State->bVoyageActive = false; S.Tick(.25f);
	TestTrue(TEXT("Stopped slot clears spill"), Crew->InSpill.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardHoldTest, "Airlock.Floor.HazardInteraction.SqueegeeInsidePartialAndEvaporatedTarget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardHoldTest::RunTest(const FString& Parameters)
{
	FHazardTestWorld S; FHazardLogCapture Log;
	S.Mode->HazardSpill(TEXT("Still_2"), 200); auto* Spill = S.Find<ASpill>(TEXT("S1"));
	if (!Spill) { return false; }
	auto* Crew = S.CrewAt(S.SpillPoint() + FVector(0, 0, 96)); S.Tick(.25f);
	TestEqual(TEXT("Six second hold"), IInteractable::Execute_GetInteractHoldSeconds(Spill), 6.f);
	TestEqual(TEXT("Prompt"), IInteractable::Execute_GetInteractPrompt(Spill).ToString(), FString(TEXT("Squeegee the spill (hold E)")));
	Crew->ServerBeginHold(Spill); TestTrue(TEXT("Inside puddle allowed"), Crew->GetHoldTarget() == Spill);
	S.Tick(5.75f); TestEqual(TEXT("Not cleared early"), S.State->FloorHazards.Spills.Num(), 1);
	S.Tick(.5f); TestTrue(TEXT("6.25 seconds cleared mirror"), S.State->FloorHazards.Spills.IsEmpty());
	S.Tick(.25f); TestEqual(TEXT("Actor gone after another tick"), S.Count<ASpill>(), 0);
	TestNull(TEXT("Destroy during completion clears hold safely"), Crew->GetHoldTarget());
	TestEqual(TEXT("Walk speed recovered"), Crew->GetCharacterMovement()->MaxWalkSpeed, 450.f);
	TestEqual(TEXT("Clear logged"), Log.Count(TEXT("Spill S1 cleared by")), 1);
	TestEqual(TEXT("Completion logged"), Log.Count(TEXT("Hold S1 complete by")), 1);
	TestEqual(TEXT("Gone logged"), Log.Count(TEXT("Spill S1 gone")), 1);
	S.Mode->HazardSpill(TEXT("Still_2"), 400); Spill = S.Find<ASpill>(TEXT("S2"));
	Crew->ServerBeginHold(Spill); S.Tick(6.25f);
	TestEqual(TEXT("One hold leaves merged puddle"), S.Count<ASpill>(), 1);
	TestEqual(TEXT("A partial pass says what is left"), Log.Count(TEXT("Spill S2 squeegeed by")), 1);
	TestTrue(TEXT("Only 300 L plus evaporation removed"), S.State->FloorHazards.Spills[0].Litres > 90.f && S.State->FloorHazards.Spills[0].Litres < 100.f);
	Crew->ServerBeginHold(Spill); S.Tick(6.25f); S.Tick(.25f);
	TestEqual(TEXT("Second hold clears remainder"), S.Count<ASpill>(), 0);
	S.Mode->HazardSpill(TEXT("Still_2"), 1); Spill = S.Find<ASpill>(TEXT("S3"));
	Crew->ServerBeginHold(Spill); S.Tick(1.25f);
	TestNull(TEXT("Evaporation drops pending hold safely"), Crew->GetHoldTarget());
	TestEqual(TEXT("Evaporation never completes squeegee"), Log.Count(TEXT("Hold S3 complete by")), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardGeometryTest, "Airlock.Floor.HazardInteraction.LiteralHoldReachAndDropSurface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardGeometryTest::RunTest(const FString& Parameters)
{
	FHazardTestWorld S; S.Mode->HazardSpill(TEXT("Still_2"), 300);
	auto* Spill = S.Find<ASpill>(TEXT("S1")); if (!Spill) { return false; }
	const FVector Point = S.SpillPoint(); auto* Crew = S.CrewAt(Point + FVector(0, -300, 96));
	TestTrue(TEXT("300 cm is dry"), S.Mode->HazardContains(Point + FVector(0, -300, 0)).IsNone());
	TestFalse(TEXT("300 cm horizontal is outside the 312.5 cm spherical hold reach"), Crew->IsInHoldRange(Spill));
	Crew->SetActorLocation(Point + FVector(0, -290, 96));
	TestTrue(TEXT("290 cm horizontal is in the dry squeegee ring"), Crew->IsInHoldRange(Spill));
	const float DryRing = FMath::Sqrt(FMath::Square(Crew->InteractReach * 1.25f) - FMath::Square(96.f)) - 250.f;
	TestEqual(TEXT("Literal dry ring is about 47 cm, not the brief's 62 cm"), DryRing, 47.388f, .01f);
	auto* Crate = S.Carry(Crew, TEXT("C3")); Crew->SetActorLocation(Point + FVector(0, 0, 96)); Crew->SetActorRotation(FRotator::ZeroRotator);
	Crew->ServerPutDown();
	TestEqual(TEXT("120 cm forward drop retained"), Crate->GetActorLocation().X, Point.X + 120, .1);
	// M5-5: the put-down trace looks through invisible interaction markers, so the cask rests on the crude's disc (up to
	// 2 cm over the deck; the test world has no deck mesh under it), not on the marker box 30 cm up.
	TestTrue(TEXT("Cask lands on the crude, not on the marker"), Crate->GetActorLocation().Z <= Point.Z + 2.1 && Crate->GetActorLocation().Z >= Point.Z - .1);
	// A drop whose trace starts inside the marker box (the live case: the crew's origin inside a control's marker) is
	// looked through as well instead of landing at the trace start.
	Crew->ServerPickUp(Crate); Crew->SetActorLocation(Point + FVector(0, 0, 10)); Crew->ServerPutDown();
	TestTrue(TEXT("A drop starting inside the marker still lands on the crude"), Crate->GetActorLocation().Z <= Point.Z + 2.1 && Crate->GetActorLocation().Z >= Point.Z - .1);
	AddInfo(FString::Printf(TEXT("Literal geometry: dry ring %.3f cm; put-down Z %.3f at deck Z %.3f"), DryRing, Crate->GetActorLocation().Z, Point.Z));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardCriticalConsoleTest, "Airlock.Floor.HazardInteraction.CriticalsBunkAndConsoleGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardCriticalConsoleTest::RunTest(const FString& Parameters)
{
	FHazardTestWorld S; FHazardLogCapture Log;
	auto* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("airlock.DevCheats"));
	if (!Var) { return false; }
	struct FHazardRestoreCheats { IConsoleVariable* Var; int32 Value; ~FHazardRestoreCheats() { Var->Set(Value, ECVF_SetByConsole); } } Restore{Var, Var->GetInt()};
	auto* Crew = S.CrewAt(FVector(-6400, 1560, 118));
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* PC = S.World->SpawnActor<AAirlockPlayerController>(P); PC->Possess(Crew);
	Var->Set(0, ECVF_SetByConsole); PC->AirlockSpill(TEXT("Still_2")); PC->AirlockFloor();
	TestEqual(TEXT("Spill helper refused"), S.Count<ASpill>(), 0); TestEqual(TEXT("Both helper refusals"), Log.Count(TEXT("Cheat Airlock")), 2);
	Var->Set(1, ECVF_SetByConsole); PC->AirlockSpill(TEXT("Still_2"));
	TestEqual(TEXT("Console defaults to 200 L"), S.State->FloorHazards.LitresSpilled, 200.f);
	TestEqual(TEXT("Injected spill excluded from paid result"), S.Mode->RefineryTally().Spills, 0);
	Crew->SetExposureServer(100); S.Tick(.25f); S.Mode->NotifyBlackout(Crew);
	TestEqual(TEXT("Blackout counted only once"), S.State->FloorCriticals, 1);
	TestEqual(TEXT("Tally uses the same total"), S.Mode->RefineryTally().Criticals, 1);
	auto* Bunk = S.Find<AFloorFixture>(TEXT("Bunk")); if (!Bunk) { return false; }
	TestEqual(TEXT("Bunk reads medical count"), Bunk->CurrentLabel(), FString(TEXT("BUNK\n1 critical")));
	PC->AirlockFloor(); TestEqual(TEXT("Console total"), Log.Count(TEXT("FloorCriticals 1")), 1);
	S.Tick(31.f); TestFalse(TEXT("Crew recovers under existing rules"), Crew->bBlackedOut);
	TestEqual(TEXT("Medical total persists after recovery"), S.State->FloorCriticals, 1);
	const auto Rows = UTaskBoardWidget::BuildRows({}, {}, {}, nullptr, nullptr, false, &S.State->Refinery, &S.State->FloorHazards, S.State->FloorCriticals);
	TestEqual(TEXT("Critical leads live floor rows"), Rows[0].Text.ToString(), FString(TEXT("1 critical - 200 cr medical")));
	TestTrue(TEXT("Critical is red"), Rows[0].bFailed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardPopTest, "Airlock.Floor.HazardInteraction.PopSpillsOnceBurstsAndSkipsLateBaseline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardPopTest::RunTest(const FString& Parameters)
{
	FHazardTestWorld S; FHazardLogCapture Log;
	auto* Still = S.Find<AStill>(TEXT("Still_1")); if (!Still) { return false; }
	TestFalse(TEXT("Idle baseline is dark"), Still->PopLight->IsVisible());
	S.Mode->RefinerySetKnob(TEXT("Still_1"), ERefineryKnob::Heat, 8);
	S.Mode->RefinerySetKnob(TEXT("Still_1"), ERefineryKnob::Feed, 10);
	S.Mode->RefinerySetKnob(TEXT("Still_1"), ERefineryKnob::Vent, 0);
	S.Tick(60.f); S.Mode->RefineryLoadHopper(TEXT("Still_1"), TEXT("C1"));
	for (int32 I = 0; I < 160 && S.State->Refinery.Stills[0].Status != EStillStatus::Popped; ++I) { S.Tick(.25f); }
	TestEqual(TEXT("Real model pop"), S.State->Refinery.Stills[0].Status, EStillStatus::Popped);
	TestEqual(TEXT("Pop's own Spill only"), S.State->FloorHazards.SpillsTotal, 1);
	TestEqual(TEXT("300 L, never doubled"), S.State->FloorHazards.LitresSpilled, 300.f);
	TestEqual(TEXT("One actor"), S.Count<ASpill>(), 1);
	TestTrue(TEXT("Transition lit the burst headless"), Still->PopLight->IsVisible());
	TestEqual(TEXT("White burst"), Still->PopLight->GetLightColor(), FLinearColor::White);
	// A newly bound actor observing the already-Popped mirror is the late-join baseline case.
	FActorSpawnParameters Params; Params.bDeferConstruction = true; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Late = S.World->SpawnActor<AStill>(Params); Late->Initialize(Still->GetSpec()); Late->FinishSpawning(Still->GetActorTransform());
	TestFalse(TEXT("Late baseline has no burst"), Late->PopLight->IsVisible());
	S.Tick(.25f); TestTrue(TEXT("Burst lasts past a quarter second"), Still->PopLight->IsVisible());
	S.Tick(.5f); TestFalse(TEXT("Half-second timer shuts burst off"), Still->PopLight->IsVisible());
	S.State->OnRep_FloorHazards(); TestFalse(TEXT("Unrelated mirror refresh never replays burst"), Still->PopLight->IsVisible());
	TestEqual(TEXT("One pop log"), Log.Count(TEXT("Refinery Pop Still_1")), 1);
	return true;
}
#endif
