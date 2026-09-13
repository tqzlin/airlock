// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockPlayerController.h"
#include "Ship/Still.h"
#include "Ship/StillControl.h"
#include "Ship/CargoCrate.h"
#include "Voyage/VoyageSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// FFloorTestWorld's quarter-second world driver, with a distinct name for unity builds.
struct FStillTestWorld
{
	UWorld* World; AAirlockGameMode* Mode; AAirlockGameState* State;
	FStillTestWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World)); World->GetGameInstance()->Init();
		auto* Voyages = World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>();
		FGuid Id; Voyages->RequestVoyage(UVoyageSubsystem::MakeVoyageSpec(EVoyageType::MilkRun, 1), Id);
		FVoyageResult Ship; Ship.IntactCasks = { TEXT("C1"), TEXT("C2"), TEXT("C3"), TEXT("C4") }; Ship.IntactLightCasks = { TEXT("C2") };
		Voyages->SetShipResult(Ship);
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>(); State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = RefineryLayouts::FloorSectionId(); World->BeginPlay();
	}
	~FStillTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->GetGameInstance()->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
	template<class T> int32 Count() const { int32 N=0; for (TActorIterator<T> It(World); It; ++It) { ++N; } return N; }
	template<class T> T* Find(FName Name) const { for (TActorIterator<T> It(World); It; ++It) { if (It->GetFName() == Name) { return *It; } } return nullptr; }
	void Tick(float Seconds) const { for (int32 I=0, N=FMath::RoundToInt(Seconds*4.f); I<N; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,.25f); } }
	AAirlockCharacter* CrewAt(FVector Location) const
	{
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Crew = World->SpawnActor<AAirlockCharacter>(P); Crew->SetActorLocation(Location);
		// This test world has no floor geometry. Hold position while ticking the real character and systems.
		Crew->GetCharacterMovement()->GravityScale = 0.f; return Crew;
	}
	ACargoCrate* Carry(AAirlockCharacter* Crew, FName Id)
	{
		auto* Crate = Find<ACargoCrate>(Id); Crew->SetActorLocation(Crate->GetActorLocation()+FVector(-150,0,96)); Crew->ServerPickUp(Crate); return Crate;
	}
	const FStillState& Still(int32 Index=0) const { return State->Refinery.Stills[Index]; }
};
class FStillLogCapture : public FOutputDevice
{
public:
	FStillLogCapture() { GLog->AddOutputDevice(this); }
	~FStillLogCapture() { GLog->RemoveOutputDevice(this); }
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{ if (Category == TEXT("LogAirlock")) { FScopeLock Lock(&Mutex); Lines += V; Lines += TEXT("\n"); } }
	FString Text() const { GLog->Flush(); FScopeLock Lock(&Mutex); return Lines; }
	int32 Count(const FString& Prefix) const
	{ TArray<FString> Out; Text().ParseIntoArrayLines(Out); int32 N=0; for (const auto& L : Out) { if (L.StartsWith(Prefix)) { ++N; } } return N; }
private:
	mutable FCriticalSection Mutex; FString Lines;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStillSpawnTest, "Airlock.Floor.StillSpawnsControlsAndUnscaledOffsets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStillSpawnTest::RunTest(const FString& Parameters)
{
	FStillTestWorld S;
	TestEqual(TEXT("Three real stills"), S.Count<AStill>(), 3); TestEqual(TEXT("Six base fixtures retained"), S.Count<AFloorFixture>(), 6);
	TestEqual(TEXT("Thirty-six controls"), S.Count<AStillControl>(), 36);
	for (const auto& Fixture : RefineryLayouts::Find(RefineryLayouts::FloorSectionId())->Fixtures)
	{
		for (const auto& Spec : RefineryLayouts::StillControls(Fixture))
		{
			auto* C = S.Find<AStillControl>(Spec.Id); if (!TestNotNull(TEXT("Control spawned"), C)) { continue; }
			TestTrue(TEXT("Table transform"), C->GetActorTransform().Equals(Spec.Transform));
			TestEqual(TEXT("Still id"), C->GetStillId(), Fixture.Id); TestEqual(TEXT("Kind"), C->GetControlKind(), Spec.Kind);
			TestTrue(TEXT("Replicated"), C->GetIsReplicated()); TestTrue(TEXT("Body ignores pawns"), C->Body->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);
			TestTrue(TEXT("Marker is traceable"), C->Marker->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block);
			TestTrue(TEXT("Marker world size independent of cosmetic scale"), C->Marker->GetComponentScale().Equals(FVector::OneVector));
			TestTrue(TEXT("Control lies outside tank radius"), FVector2D(Spec.Offset.X,Spec.Offset.Y).Size() > 120.f);
			TestTrue(TEXT("In range from this still's walkway"), FVector::Distance(Fixture.Transform.GetLocation()+FVector(0,-300,96), C->GetActorLocation()) < 312.5f);
		}
	}
	FFloorFixtureSpec Rotated = RefineryLayouts::Find(RefineryLayouts::FloorSectionId())->Fixtures[0];
	Rotated.Transform = FTransform(FRotator(0,90,0), FVector(100,200,300), FVector(.8));
	const auto Controls = RefineryLayouts::StillControls(Rotated);
	TestTrue(TEXT("Offsets rotate without tank scale"), Controls[0].Transform.GetLocation().Equals(FVector(300,0,300)));
	TestTrue(TEXT("Own scale"), Controls[1].Transform.GetScale3D().Equals(FVector(.3,.3,.15)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStillHopperTest, "Airlock.Floor.StillHopperLoadsOnlyCarriedCasks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStillHopperTest::RunTest(const FString& Parameters)
{
	FStillTestWorld S; auto* Hopper = S.Find<AStillControl>(TEXT("Still_1_Hopper"));
	if (!TestNotNull(TEXT("Hopper"), Hopper)) { return false; }
	auto* Crew = S.CrewAt(Hopper->GetActorLocation()+FVector(0,-100,96));
	IInteractable::Execute_Interact(Hopper,Crew); TestEqual(TEXT("Empty hands do nothing"), S.State->Refinery.Waiting.Num(),4);
	auto* Crate = S.Carry(Crew,TEXT("C1")); Crew->ServerLoadStill(Hopper);
	TestTrue(TEXT("Distant load keeps hands"), Crew->CarriedCrate == Crate);
	Crew->SetActorLocation(Hopper->GetActorLocation()+FVector(0,-100,96));
	Crew->bBlackedOut=true; Crew->ServerLoadStill(Hopper); TestTrue(TEXT("Blackout keeps hands"), Crew->CarriedCrate == Crate); Crew->bBlackedOut=false;
	Crew->ServerLoadStill(S.Find<AStillControl>(TEXT("Still_1_HeatPlus"))); TestTrue(TEXT("A handle is not a hopper"), Crew->CarriedCrate == Crate);
	IInteractable::Execute_Interact(Hopper,Crew);
	TestNull(TEXT("Hands freed"), Crew->CarriedCrate.Get()); TestTrue(TEXT("Crate hidden"), Crate->IsHidden());
	TestNull(TEXT("Crate detached"), Crate->GetAttachParentActor()); TestEqual(TEXT("Waiting shrinks"), S.State->Refinery.Waiting.Num(),3);
	TestEqual(TEXT("Loaded state"), Crate->GetCargoState()->State, ECargoState::Loaded);
	TestEqual(TEXT("Queued label"), Hopper->Label->Text.ToString(),FString(TEXT("HOPPER\n1 queued")));
	Crew->ServerPickUp(Crate); TestNull(TEXT("Consumed crate cannot be picked up"), Crew->CarriedCrate.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStillHandleTest, "Airlock.Floor.StillHandleTapRepeatPauseReleaseAndSilentLimits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStillHandleTest::RunTest(const FString& Parameters)
{
	FStillTestWorld S; auto* Plus=S.Find<AStillControl>(TEXT("Still_1_HeatPlus")); auto* Minus=S.Find<AStillControl>(TEXT("Still_1_HeatMinus"));
	if (!Plus || !Minus) { AddError(TEXT("Missing handles")); return false; }
	auto* Crew=S.CrewAt(Plus->GetActorLocation()+FVector(0,-100,96));
	Crew->ServerBeginHold(Plus); Crew->ServerEndHold(); TestEqual(TEXT("Tap is one step"), S.Still().HeatSteps,1);
	Crew->ServerBeginHold(Plus); S.Tick(1.f); TestEqual(TEXT("One plus four steps in four frames"),S.Still().HeatSteps,6);
	TestTrue(TEXT("Hold persists"),Crew->GetHoldTarget()==Plus);
	Crew->SetActorLocation(FVector::ZeroVector); S.Tick(1.f); TestEqual(TEXT("Out of range pauses"),S.Still().HeatSteps,6);
	Crew->SetActorLocation(Plus->GetActorLocation()+FVector(0,-100,96)); S.Tick(.25f); TestEqual(TEXT("Returning resumes"),S.Still().HeatSteps,7);
	Crew->ServerEndHold(); S.Tick(.5f); TestEqual(TEXT("Release stops steps"),S.Still().HeatSteps,7); TestNull(TEXT("No hold"),Crew->GetHoldTarget());
	S.Mode->RefinerySetKnob(TEXT("Still_1"),ERefineryKnob::Heat,20);
	{ FStillLogCapture Log; Crew->ServerBeginHold(Plus); S.Tick(1.f); Crew->ServerEndHold();
	  TestEqual(TEXT("No steps or rejections at upper stop"),Log.Count(TEXT("Refinery Still_1 heat")),0);
	  TestEqual(TEXT("No per-step hold log"),Log.Count(TEXT("Hold Still_1_HeatPlus complete")),0);
	  TestEqual(TEXT("Begin once"),Log.Count(TEXT("Hold Still_1_HeatPlus started")),1); TestEqual(TEXT("Release once"),Log.Count(TEXT("Hold Still_1_HeatPlus released")),1); }
	S.Mode->RefinerySetKnob(TEXT("Still_1"),ERefineryKnob::Heat,0);
	{ FStillLogCapture Log; Crew->ServerBeginHold(Minus); S.Tick(.5f); Crew->ServerEndHold(); TestEqual(TEXT("Silent lower stop"),Log.Count(TEXT("Refinery Still_1 heat")),0); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStillLeverTest, "Airlock.Floor.StillLeverRefusalsAndDurations", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStillLeverTest::RunTest(const FString& Parameters)
{
	FStillTestWorld S; auto* Reset=S.Find<AStillControl>(TEXT("Still_1_Reset"));
	if (!Reset) { return false; } auto* Crew=S.CrewAt(Reset->GetActorLocation()+FVector(0,-100,0));
	{ FStillLogCapture Log; Crew->ServerBeginHold(Reset); TestNull(TEXT("Wrong state never starts"),Crew->GetHoldTarget());
	  TestEqual(TEXT("Refusal names Idle"),Log.Count(TEXT("Hold Still_1_Reset rejected: Idle")),1); }
	const TCHAR* Names[] = { TEXT("Reset"),TEXT("Purge"),TEXT("Scrape"),TEXT("Repair"),TEXT("Calibrate") };
	const float Seconds[] = {4,5,8,12,3};
	const EStillStatus Allowed[] = {EStillStatus::Popped,EStillStatus::Flash,EStillStatus::Idle,EStillStatus::Failed,EStillStatus::Running};
	for (int32 I=0; I<5; ++I)
	{
		auto* C=S.Find<AStillControl>(FName(*(FString(TEXT("Still_1_"))+Names[I]))); if (!C) { return false; }
		for (EStillStatus Status : {EStillStatus::Idle,EStillStatus::Running,EStillStatus::Popped,EStillStatus::Flash,EStillStatus::Failed})
		{
			FRefineryState R=S.State->Refinery; R.Stills[0].Status=Status; S.State->PublishRefinery(R);
			TestEqual(TEXT("Hold duration in every state"),IInteractable::Execute_GetInteractHoldSeconds(C),Seconds[I]);
			TestFalse(TEXT("Prompt in every state"),IInteractable::Execute_GetInteractPrompt(C).IsEmpty());
			TestEqual(TEXT("Lever predicate"),IInteractable::Execute_GetHoldRefusal(C).IsEmpty(),I==4 ? Status!=EStillStatus::Failed : Status==Allowed[I]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStillPopTest, "Airlock.Floor.StillPressureWarnsOnceBeforePopAndResetHold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStillPopTest::RunTest(const FString& Parameters)
{
	FStillTestWorld S; FStillLogCapture Log;
	for (FName Id : {FName(TEXT("Still_1")),FName(TEXT("Still_2"))})
	{
		S.Mode->RefinerySetKnob(Id,ERefineryKnob::Heat,10); S.Mode->RefinerySetKnob(Id,ERefineryKnob::Feed,10); S.Mode->RefinerySetKnob(Id,ERefineryKnob::Vent,2);
	}
	S.Tick(60.f); // 240 preheat ticks, as in M5-1's pop fixture.
	S.Mode->RefineryLoadHopper(TEXT("Still_1"),TEXT("C1")); S.Mode->RefineryLoadHopper(TEXT("Still_2"),TEXT("C3"));
	int32 Ticks=0; for (; Ticks<80 && S.Still().Status!=EStillStatus::Popped; ++Ticks) { S.Tick(.25f); }
	TestEqual(TEXT("Pop within 80 ticks"),S.Still().Status,EStillStatus::Popped);
	TestEqual(TEXT("Second still also pops"),S.Still(1).Status,EStillStatus::Popped);
	TestEqual(TEXT("Pressure cue once across stills"),Log.Count(TEXT("Okafor: Pressure on ")),1);
	const FString Lines=Log.Text(); const int32 Warning=Lines.Find(TEXT("Okafor: Pressure on Still 1")); const int32 Pop=Lines.Find(TEXT("Refinery Pop Still_1"));
	TestTrue(TEXT("Warning precedes pop"),Warning!=INDEX_NONE && Pop>Warning);
	AddInfo(FString::Printf(TEXT("Pop after %d quarter-second ticks following 240 preheat ticks"),Ticks));
	auto* Reset=S.Find<AStillControl>(TEXT("Still_1_Reset")); if (!Reset) { return false; }
	auto* Crew=S.CrewAt(Reset->GetActorLocation()+FVector(0,-100,0));
	Crew->ServerBeginHold(Reset); S.Tick(3.75f); TestEqual(TEXT("Reset takes four seconds"),S.Still().Status,EStillStatus::Popped);
	S.Tick(.25f); TestEqual(TEXT("Reset completes through API"),S.Still().Status,EStillStatus::Idle); TestNull(TEXT("One-shot hold ends"),Crew->GetHoldTarget());
	TestEqual(TEXT("Completion"),Log.Count(TEXT("Hold Still_1_Reset complete")),1); TestEqual(TEXT("API reset"),Log.Count(TEXT("Refinery Still_1 reset")),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStillDriftTest, "Airlock.Floor.StillDriftCueSharedAndCalibrationHold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStillDriftTest::RunTest(const FString& Parameters)
{
	FStillTestWorld S; FStillLogCapture Log;
	for (FName Id : {FName(TEXT("Still_1")),FName(TEXT("Still_2"))})
	{
		S.Mode->RefinerySetKnob(Id,ERefineryKnob::Heat,8); S.Mode->RefinerySetKnob(Id,ERefineryKnob::Feed,10);
	}
	S.Mode->RefineryLoadHopper(TEXT("Still_1"),TEXT("C1")); S.Mode->RefineryLoadHopper(TEXT("Still_2"),TEXT("C3")); S.Tick(55.f);
	TestTrue(TEXT("Both traps drift"),S.Still().Calibration<.8f && S.Still(1).Calibration<.8f);
	TestEqual(TEXT("First trap cue once"),Log.Count(TEXT("Okafor: Still 1's trap has drifted")),1);
	TestEqual(TEXT("Second trap cue suppressed by shared cooldown"),Log.Count(TEXT("Okafor: Still 2's trap has drifted")),0);
	auto* Cal=S.Find<AStillControl>(TEXT("Still_1_Calibrate")); if (!Cal) { return false; }
	auto* Crew=S.CrewAt(Cal->GetActorLocation()+FVector(60,-90,0));
	Crew->ServerBeginHold(Cal); S.Tick(2.75f); TestTrue(TEXT("Not yet calibrated"),S.Still().Calibration<.8f);
	S.Tick(.25f); TestTrue(TEXT("Trap recentred"),S.Still().Calibration>=.99f);
	TestEqual(TEXT("Calibration API"),Log.Count(TEXT("Refinery Still_1 calibrated")),1);
	TestEqual(TEXT("Calibration hold completed"),Log.Count(TEXT("Hold Still_1_Calibrate complete")),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStillGaugeTest, "Airlock.Floor.StillGaugesReadMirrorAndWarningColours", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStillGaugeTest::RunTest(const FString& Parameters)
{
	FStillTestWorld S; auto* Still=S.Find<AStill>(TEXT("Still_1")); if (!Still) { return false; }
	FRefineryState R=S.State->Refinery; auto& St=R.Stills[0];
	St.Status=EStillStatus::Running; St.CurrentCask=TEXT("C1"); St.ChargeL=580; St.TempK=184; St.PressureBar=2.75f;
	St.HeatSteps=8; St.FeedSteps=10; St.VentSteps=11; St.Calibration=.7f;
	S.State->PublishRefinery(R);
	TestEqual(TEXT("Bath and pressure"),Still->BathGauge->Text.ToString(),FString(TEXT("T 184 K   P 2.75 bar")));
	TestEqual(TEXT("Knobs"),Still->KnobGauge->Text.ToString(),FString(TEXT("heat 40%  feed 50%  vent 55%")));
	TestEqual(TEXT("Batch from mirrored charge"),Still->BatchGauge->Text.ToString(),FString(TEXT("C1 42%   cal 70%")));
	TestEqual(TEXT("High pressure red"),Still->BathGauge->TextRenderColor,FLinearColor::Red.ToFColor(true));
	TestEqual(TEXT("Drift amber"),Still->BatchGauge->TextRenderColor,FLinearColor(1,.65f,.1f).ToFColor(true));
	TestTrue(TEXT("Base label retained"),Still->CurrentLabel().Contains(TEXT("C1 42% 184K")));
	const FVector Offsets[] = {FVector(0,-125,215),FVector(0,-125,195),FVector(0,-125,175)};
	const UTextRenderComponent* Gauges[] = {Still->BathGauge,Still->KnobGauge,Still->BatchGauge};
	for (int32 I=0; I<3; ++I) { TestEqual(TEXT("Gauge size"),Gauges[I]->WorldSize,18.f); TestTrue(TEXT("Specified relative offset"),Gauges[I]->GetRelativeLocation().Equals(Offsets[I])); }
	const EStillStatus States[] = {EStillStatus::Idle,EStillStatus::Popped,EStillStatus::Flash,EStillStatus::Failed};
	const TCHAR* Labels[] = {TEXT("idle"),TEXT("POPPED"),TEXT("FLASH"),TEXT("FAILED")};
	for (int32 I=0; I<4; ++I) { St.Status=States[I]; S.State->PublishRefinery(R); TestTrue(TEXT("Inactive status"),Still->BatchGauge->Text.ToString().StartsWith(Labels[I])); }
	St.PressureBar=1; St.Calibration=1; S.State->PublishRefinery(R);
	TestEqual(TEXT("Normal pressure white"),Still->BathGauge->TextRenderColor,FColor::White); TestEqual(TEXT("Calibrated white"),Still->BatchGauge->TextRenderColor,FColor::White);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStillPressTest, "Airlock.Floor.StillPressRoutesTapLoadPutDownAndRejectsDistance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStillPressTest::RunTest(const FString& Parameters)
{
	FStillTestWorld S;
	auto* Var=IConsoleManager::Get().FindConsoleVariable(TEXT("airlock.DevCheats")); const int32 Saved=Var->GetInt();
	struct FRestore { IConsoleVariable* Var; int32 Saved; ~FRestore() { Var->Set(Saved,ECVF_SetByConsole); } } Restore{Var,Saved};
	Var->Set(1,ECVF_SetByConsole);
	auto* Plus=S.Find<AStillControl>(TEXT("Still_2_HeatPlus")); auto* Hopper=S.Find<AStillControl>(TEXT("Still_2_Hopper")); if (!Plus || !Hopper) { return false; }
	auto* Crew=S.CrewAt(Plus->GetActorLocation()+FVector(0,-100,96));
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* PC=S.World->SpawnActor<AAirlockPlayerController>(P); PC->Possess(Crew);
	PC->ServerPress(TEXT("Still_2_HeatPlus")); TestEqual(TEXT("Press taps once"),S.Still(1).HeatSteps,1); TestNull(TEXT("Press released"),Crew->GetHoldTarget());
	S.Carry(Crew,TEXT("C2")); Crew->SetActorLocation(Hopper->GetActorLocation()+FVector(0,-100,96));
	TestEqual(TEXT("Carry prompt"),IInteractable::Execute_GetInteractPrompt(Hopper).ToString(),FString(TEXT("Load C2 into Still 2")));
	PC->ServerPress(TEXT("Still_2_Hopper")); TestNull(TEXT("Press loads"),Crew->CarriedCrate.Get()); TestEqual(TEXT("Queue holds C2"),S.Still(1).Hopper[0],FName(TEXT("C2")));
	S.Carry(Crew,TEXT("C1")); Crew->SetActorLocation(Plus->GetActorLocation()+FVector(0,-100,96)); PC->ServerPress(TEXT("Still_2_HeatPlus"));
	TestNull(TEXT("Other control puts cask down"),Crew->CarriedCrate.Get()); TestEqual(TEXT("Carrying does not turn heat"),S.Still(1).HeatSteps,1);
	Crew->SetActorLocation(FVector::ZeroVector);
	FStillLogCapture Log; PC->ServerPress(TEXT("Still_2_HeatPlus")); PC->ServerPress(TEXT("MissingControl"));
	TestEqual(TEXT("Distance refusal"),Log.Count(TEXT("Press Still_2_HeatPlus rejected: out of reach")),1); TestEqual(TEXT("Unknown refusal"),Log.Count(TEXT("Press MissingControl rejected: actor not found")),1);
	return true;
}
#endif
