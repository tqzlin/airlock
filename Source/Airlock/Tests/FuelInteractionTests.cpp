// Copyright Airlock project. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockCharacter.h"
#include "Ship/FuelValve.h"
#include "Ship/BypassValve.h"
#include "Ship/FuelIntake.h"
#include "Ship/CargoCrate.h"
#include "Ship/ShipModule.h"
#include "Voyage/VoyageSubsystem.h"
#include "Voyage/VoyageSetPieces.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FFuelTestWorld
{
	UWorld* World; AAirlockGameMode* Mode; AAirlockGameState* State;
	explicit FFuelTestWorld(FName Section = TEXT("Hauler_Mk1"), bool bBook = true)
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World)); World->GetGameInstance()->Init();
		if (bBook) { FVoyageSpec Spec; Spec.Captain = TEXT("TestCaptain"); FGuid Id; World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>()->RequestVoyage(Spec, Id); }
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>(); State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = Section; World->BeginPlay();
	}
	~FFuelTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->GetGameInstance()->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
	template<class T> T* Find(FName Id) const
	{ for (TActorIterator<T> It(World); It; ++It) { if (It->GetFName() == Id) { return *It; } } return nullptr; }
	AAirlockCharacter* CrewAt(FVector Location = FVector::ZeroVector) const
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Crew = World->SpawnActor<AAirlockCharacter>(Params); Crew->SetActorLocation(Location); return Crew;
	}
	float Engines() const { return State->ShipModules.FindByPredicate([](const auto& M) { return M.Id == TEXT("Engines"); })->Condition; }
	ACargoCrate* Carry(AAirlockCharacter* Crew, FName Id)
	{
		auto* Crate = Find<ACargoCrate>(Id); Crew->SetActorLocation(Crate->GetActorLocation() + FVector(-150,0,96)); Crew->ServerPickUp(Crate); return Crate;
	}
};
class FFuelLogCapture : public FOutputDevice
{
public:
	FFuelLogCapture() { GLog->AddOutputDevice(this); }
	~FFuelLogCapture() { GLog->RemoveOutputDevice(this); }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelFixtureTest, "Airlock.Fuel.FixturesValveHoldAndBypassPress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFuelFixtureTest::RunTest(const FString& Parameters)
{
	FFuelTestWorld S; auto* Valve = S.Find<AFuelValve>(TEXT("FeedValve")); auto* Bypass = S.Find<ABypassValve>(TEXT("BypassValve")); auto* Intake = S.Find<AFuelIntake>(TEXT("FuelIntake"));
	TestNotNull(TEXT("Feed fixture"), Valve); TestNotNull(TEXT("Bypass fixture"), Bypass); TestNotNull(TEXT("Intake fixture"), Intake);
	if (!Valve || !Bypass || !Intake) { return false; }
	for (const auto* Actor : TArray<AActor*>({Valve,Bypass,Intake}))
	{
		TestTrue(TEXT("Engineering east side"), Actor->GetActorLocation().X >= -1531 && Actor->GetActorLocation().X <= -1106 && Actor->GetActorLocation().Y >= 230 && Actor->GetActorLocation().Y <= 950);
		TestEqual(TEXT("Floor"), Actor->GetActorLocation().Z, 22.0); TestTrue(TEXT("Replicated"), Actor->GetIsReplicated());
	}
	TestTrue(TEXT("Clock started"), S.State->bVoyageActive); TestNotNull(TEXT("Director started"), S.Mode->GetIncidentDirector());
	auto* Crew = S.CrewAt(FVector(-1300,280,118));
	TestEqual(TEXT("Healthy valve no hold"), IInteractable::Execute_GetInteractHoldSeconds(Valve), 0.f);
	S.Mode->FuelOpenLeak(); TestEqual(TEXT("Leak prompt"), IInteractable::Execute_GetInteractPrompt(Valve).ToString(), FString(TEXT("Shut the feed valve (hold E)")));
	Crew->ServerBeginHold(Valve); Crew->Tick(2.9f); TestTrue(TEXT("Three seconds required"), S.State->Fuel.bFeedLeak);
	Crew->Tick(.11f); TestFalse(TEXT("Hold shuts leak"), S.State->Fuel.bFeedLeak); TestTrue(TEXT("Prompt cleared"), IInteractable::Execute_GetInteractPrompt(Valve).IsEmpty());
	TestEqual(TEXT("Closed intake prompt"), IInteractable::Execute_GetInteractPrompt(Intake).ToString(), FString(TEXT("Bypass closed")));
	Crew->SetActorLocation(FVector::ZeroVector); IInteractable::Execute_Interact(Bypass,Crew); TestFalse(TEXT("Distant bypass refused"), S.State->Fuel.bBypassOpen);
	Crew->SetActorLocation(FVector(-1300,700,118)); IInteractable::Execute_Interact(Bypass,Crew); TestTrue(TEXT("Press opens"), S.State->Fuel.bBypassOpen);
	TestFalse(TEXT("Opens once"), S.Mode->FuelOpenBypass()); TestTrue(TEXT("Open prompt empty"), IInteractable::Execute_GetInteractPrompt(Bypass).IsEmpty());
	TestEqual(TEXT("Ready intake prompt"), IInteractable::Execute_GetInteractPrompt(Intake).ToString(), FString(TEXT("Feed a cask to the engines")));
	return true;
}

namespace
{
class FFuelFeedCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test; FFuelTestWorld S; AAirlockCharacter* Crew; AFuelIntake* Intake; int32 Step=0;
public:
	explicit FFuelFeedCommand(FAutomationTestBase* T) : Test(T) { Crew=S.CrewAt(); Intake=S.Find<AFuelIntake>(TEXT("FuelIntake")); }
	virtual bool Update() override
	{
		if (Step == 0)
		{
			S.Mode->AirSetScrubMode(EScrubMode::Off);
			auto* C = S.Carry(Crew,TEXT("C3")); Crew->SetActorLocation(FVector(-1300,600,118));
			Crew->ServerFeedCask(Intake); Test->TestTrue(TEXT("Closed refusal keeps carrying"), Crew->CarriedCrate == C);
			S.Mode->FuelOpenBypass(); const auto Before = S.State->Fuel;
			Test->TestFalse(TEXT("Loose cannot be consumed by GM"), S.Mode->FuelFeedCask(TEXT("C1")));
			Test->TestTrue(TEXT("Failed consume rolls back fuel"), S.State->Fuel == Before); Test->TestEqual(TEXT("No damage on refused consume"), S.Engines(),100.f);
			Crew->SetActorLocation(FVector::ZeroVector); Crew->ServerFeedCask(Intake); Test->TestTrue(TEXT("Range refusal retains cask"), Crew->CarriedCrate == C);
			Crew->SetActorLocation(FVector(-1300,600,118)); Crew->ServerFeedCask(Intake);
			Test->TestNull(TEXT("Feed empties hands"), Crew->CarriedCrate.Get()); Test->TestNull(TEXT("Cask detached"), C->GetAttachParentActor());
			Test->TestTrue(TEXT("Burned immediately"), C->GetCargoState()->State == ECargoState::Burned);
			Test->TestTrue(TEXT("Burned hidden"), C->IsHidden()); Test->TestFalse(TEXT("Burned collision off"), C->GetActorEnableCollision());
			Test->TestFalse(TEXT("Burned label off"), C->Label->IsVisible()); Test->TestTrue(TEXT("Burned prompt empty"), IInteractable::Execute_GetInteractPrompt(C).IsEmpty());
			Test->TestEqual(TEXT("First feed costs ten"), S.Engines(),90.f); Test->TestEqual(TEXT("Burn begins"), S.State->Fuel.BurningCrate,FName(TEXT("C3")));
			Test->TestTrue(TEXT("Busy prompt empty"), IInteractable::Execute_GetInteractPrompt(Intake).IsEmpty());
			C=S.Carry(Crew,TEXT("C1")); Crew->SetActorLocation(FVector(-1300,600,118)); Crew->ServerFeedCask(Intake);
			Test->TestTrue(TEXT("Busy refusal keeps carrying"), Crew->CarriedCrate == C);
		}
		else if (Step == 1)
		{
			S.World->GetTimerManager().Tick(20.f);
			Test->TestEqual(TEXT("First burn completes"), S.State->Fuel.CasksBurned,1);
			Test->TestEqual(TEXT("First cask adds 15 soot ppm"), S.State->Atmosphere.Ppm,15.f);
			Crew->ServerFeedCask(Intake); Test->TestEqual(TEXT("Second feed costs ten"),S.Engines(),80.f);
		}
		else if (Step == 2)
		{
			S.World->GetTimerManager().Tick(20.f); Test->TestTrue(TEXT("Second burn fouls"),S.State->Fuel.bInjectorsFouled);
			S.Carry(Crew,TEXT("C2")); Crew->SetActorLocation(FVector(-1300,600,118)); Crew->ServerFeedCask(Intake);
			Test->TestEqual(TEXT("Third fouled feed costs twenty"),S.Engines(),60.f);
			auto* Engines=S.Find<AShipModule>(TEXT("Engines")); Crew->SetActorLocation(FVector(-1300,880,118));
			Test->TestEqual(TEXT("Injector prompt"),IInteractable::Execute_GetInteractPrompt(Engines).ToString(),FString(TEXT("Clear the injectors (hold E)")));
			Crew->ServerBeginHold(Engines); Crew->Tick(3.9f); Test->TestTrue(TEXT("Four second hold"),S.State->Fuel.bInjectorsFouled);
			Crew->Tick(.11f); Test->TestFalse(TEXT("Cleared"),S.State->Fuel.bInjectorsFouled);
			Test->TestEqual(TEXT("Clear doesn't repair condition"),S.Engines(),60.f);
			Test->TestEqual(TEXT("Unfouled engine hold zero"),IInteractable::Execute_GetInteractHoldSeconds(Engines),0.f);
			return true;
		}
		++Step; return false;
	}
};
class FFuelExhaustionCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test; FFuelTestWorld S; FFuelLogCapture Log; int32 Step=0;
public:
	explicit FFuelExhaustionCommand(FAutomationTestBase* T) : Test(T) {}
	virtual bool Update() override
	{
		if (Step == 0) { S.Mode->SetVoyageSecondsRemaining(600); S.Mode->FuelSet(0); }
		else if (Step == 1)
		{
			S.World->GetTimerManager().Tick(.25f);
			Test->TestTrue(TEXT("Exhaustion latched"),S.State->Fuel.bExhausted);
			Test->TestEqual(TEXT("Exactly two extra minutes"),S.State->GetVoyageSecondsRemaining(),720.0,1.0);
			Test->TestEqual(TEXT("Exhaustion itself leaves engines intact"),S.Engines(),100.f);
			S.Mode->FuelSet(50);
		}
		else if (Step == 2) { S.World->GetTimerManager().Tick(.25f); S.Mode->FuelSet(0); }
		else
		{
			S.World->GetTimerManager().Tick(.25f);
			Test->TestEqual(TEXT("Never extends twice"),S.State->GetVoyageSecondsRemaining(),720.0,1.0);
			Test->TestEqual(TEXT("One exhaustion log"),Log.Count(TEXT("Fuel exhausted:")),1);
			Test->TestTrue(TEXT("Late result"),S.Mode->CargoTally().bLateDock); return true;
		}
		++Step; return false;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelFeedTest,"Airlock.Fuel.FeedAtomicCarryBurnSootDamageAndInjectorHold",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FFuelFeedTest::RunTest(const FString& Parameters) { ADD_LATENT_AUTOMATION_COMMAND(FFuelFeedCommand(this)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelExhaustionTest,"Airlock.Fuel.ExhaustionExtendsClockExactlyOnce",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FFuelExhaustionTest::RunTest(const FString& Parameters) { ADD_LATENT_AUTOMATION_COMMAND(FFuelExhaustionCommand(this)); return true; }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelGuardTest,"Airlock.Fuel.BlackoutGuardsPublicationAndSectionGating",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FFuelGuardTest::RunTest(const FString& Parameters)
{
	FFuelTestWorld S; auto* Crew=S.CrewAt(FVector(-1300,700,118)); auto* Valve=S.Find<ABypassValve>(TEXT("BypassValve")); auto* Intake=S.Find<AFuelIntake>(TEXT("FuelIntake"));
	Crew->bBlackedOut=true; Crew->ServerOpenBypass(Valve); TestFalse(TEXT("Blackout cannot open"),S.State->Fuel.bBypassOpen);
	TestFalse(TEXT("Console respects blackout"),Crew->OpenBypassServer(nullptr,false));
	Crew->bBlackedOut=false; S.Mode->FuelOpenBypass(); auto* C=S.Carry(Crew,TEXT("C1")); Crew->SetActorLocation(FVector(-1300,600,118));
	Crew->bBlackedOut=true; Crew->ServerFeedCask(Intake); TestTrue(TEXT("Blackout feed retains hands"),Crew->CarriedCrate==C);
	TestTrue(TEXT("No burn"),S.State->Fuel.BurningCrate.IsNone()); Crew->bBlackedOut=false;
	int32 N=0; const auto H=S.State->OnShipGraphChanged.AddLambda([&N]{++N;});
	auto F=S.State->Fuel; S.State->PublishFuel(F); TestEqual(TEXT("Same state no notify"),N,0);
	F.BurnSecondsLeft=3; S.State->PublishFuel(F); S.State->PublishFuel(F); TestEqual(TEXT("Changed state notifies once"),N,1);
	S.State->OnShipGraphChanged.Remove(H);
	FFuelTestWorld Hub(TEXT("StationHub")); TestFalse(TEXT("Hub no fuel"),Hub.Mode->FuelHasSystem());
	TestEqual(TEXT("Hub summary"),Hub.Mode->FuelSummary(),FString(TEXT("No fuel system on this section")));
	TestNull(TEXT("No hub fixture"),Hub.Find<AFuelValve>(TEXT("FeedValve")));
	TestFalse(TEXT("Hub leak refused"),Hub.Mode->FuelOpenLeak()); TestFalse(TEXT("Hub bypass refused"),Hub.Mode->FuelOpenBypass()); TestFalse(TEXT("Hub set refused"),Hub.Mode->FuelSet(0));
	FFuelTestWorld Unbooked(TEXT("Hauler_Mk1"),false); Unbooked.Mode->FuelOpenLeak(); Unbooked.World->GetTimerManager().Tick(30);
	TestEqual(TEXT("Unbooked ship never drains"),Unbooked.State->Fuel.Units,90.f); TestFalse(TEXT("Unbooked clock inactive"),Unbooked.State->bVoyageActive);
	return true;
}

namespace
{
class FFuelSetPieceCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test; FFuelTestWorld S; FFuelLogCapture Log; int32 Step=0; bool bDock;
public:
	FFuelSetPieceCommand(FAutomationTestBase* T,bool Dock) : Test(T),bDock(Dock) {}
	virtual bool Update() override
	{
		S.World->GetTimerManager().Tick(.1f);
		if (Step == 0) { S.Mode->SetVoyageSecondsRemaining(bDock ? 0 : 400); }
		else if (bDock)
		{
			const FString Text=Log.Text();
			Test->TestEqual(TEXT("Dock jump consumes leak"),Log.Count(TEXT("SetPiece FuelLeak at")),1);
			Test->TestTrue(TEXT("Leak before docking caption"),Text.Find(TEXT("SetPiece FuelLeak at")) < Text.Find(TEXT("Docking now")));
			Test->TestFalse(TEXT("Docking ends voyage"),S.State->bVoyageActive); return true;
		}
		else if (Step == 1) { Test->TestTrue(TEXT("Forward jump opens leak"),S.State->Fuel.bFeedLeak); S.Mode->FuelCloseLeak(); S.Mode->SetVoyageSecondsRemaining(1200); }
		else if (Step == 2) { Test->TestFalse(TEXT("Rewind doesn't reopen"),S.State->Fuel.bFeedLeak); S.Mode->SetVoyageSecondsRemaining(400); }
		else { Test->TestFalse(TEXT("Re-cross never replays"),S.State->Fuel.bFeedLeak); Test->TestEqual(TEXT("Consumed exactly once"),Log.Count(TEXT("SetPiece FuelLeak at")),1); return true; }
		++Step; return false;
	}
};
class FFuelFollowupCommand : public IAutomationLatentCommand
{
	FAutomationTestBase* Test; FFuelTestWorld S; FFuelLogCapture Log; int32 Step=0; int32 Case;
public:
	FFuelFollowupCommand(FAutomationTestBase* T,int32 InCase) : Test(T),Case(InCase) {}
	virtual bool Update() override
	{
		if (Step == 0)
			{ if (Case==1) { S.Mode->FuelSet(80); } S.Mode->FuelLeakConsole(); if (Case==2) { S.Mode->FuelCloseLeak(); } if (Case==3) { S.State->bVoyageActive=false; } }
		else
		{
			S.World->GetTimerManager().Tick(10.1f);
			if (Step==1) { S.Mode->FuelCloseLeak(); S.Mode->FuelLeakConsole(); }
			else
			{
				const FString Text=Log.Text();
				const TCHAR* Expected[] = { TEXT("Shut that feed. We're still good"), TEXT("At this burn we dock short."), TEXT("Feed's shut. We're still good") };
				if (Case<3) { Test->TestTrue(TEXT("Correct follow-up branch"),Text.Contains(Expected[Case])); }
				else { Test->TestFalse(TEXT("Inactive follow-up suppressed"),Text.Contains(TEXT("We're still good")) || Text.Contains(TEXT("At this burn"))); }
				Test->TestEqual(TEXT("Set piece consumed once even through console"),Log.Count(TEXT("SetPiece FuelLeak at")),1);
				Test->TestEqual(TEXT("Console reopen logged separately"),Log.Count(TEXT("SetPiece FuelLeak (console)")),1);
				TArray<FString> Lines; Text.ParseIntoArrayLines(Lines); int32 Followups=0;
				for (const auto& L:Lines) { if (L.StartsWith(TEXT("Captain")) && (L.Contains(TEXT("We're still good")) || L.Contains(TEXT("At this burn")))) { ++Followups; } }
				Test->TestEqual(TEXT("Follow-up once, or none after end"),Followups,Case==3?0:1); return true;
			}
		}
		++Step; return false;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelSetPieceJumpTest,"Airlock.Fuel.SetPieceForwardJumpAndRewindOnce",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FFuelSetPieceJumpTest::RunTest(const FString& Parameters) { ADD_LATENT_AUTOMATION_COMMAND(FFuelSetPieceCommand(this,false)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelSetPieceDockTest,"Airlock.Fuel.SetPieceDockJumpBeforeDockingCaption",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FFuelSetPieceDockTest::RunTest(const FString& Parameters) { ADD_LATENT_AUTOMATION_COMMAND(FFuelSetPieceCommand(this,true)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFuelFollowupTest,"Airlock.Fuel.FollowupReserveShortShutAndInactive",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FFuelFollowupTest::RunTest(const FString& Parameters)
{
	for (int32 I=0;I<4;++I) { FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFuelFollowupCommand>(this,I)); }
	return true;
}
#endif
