// Copyright Airlock project. All Rights Reserved.
// M4-1: the dev-cheats gate in front of every console helper, and the per-instance file console.
#include "Misc/AutomationTest.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockPlayerController.h"
#include "Core/AgentConsoleSubsystem.h"
#include "Voyage/VoyageSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/CheatManager.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
/** A booked Hauler_Mk1 section with a controller possessing a crew pawn, like FFuelTestWorld plus the controller. */
struct FCheatTestWorld
{
	UWorld* World; AAirlockGameMode* Mode; AAirlockGameState* State; AAirlockPlayerController* Controller; AAirlockCharacter* Crew;
	explicit FCheatTestWorld(FName Section = TEXT("Hauler_Mk1"))
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->SetGameInstance(NewObject<UGameInstance>(World)); World->GetGameInstance()->Init();
		FVoyageSpec Spec; Spec.Captain = TEXT("TestCaptain"); FGuid Id;
		World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>()->RequestVoyage(Spec, Id);
		FURL URL; URL.AddOption(TEXT("game=/Script/Airlock.AirlockGameMode"));
		World->SetGameMode(URL); World->InitializeActorsForPlay(URL);
		Mode = World->GetAuthGameMode<AAirlockGameMode>(); State = World->GetGameState<AAirlockGameState>();
		State->CurrentSectionId = Section; World->BeginPlay();
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Crew = World->SpawnActor<AAirlockCharacter>(Params); Crew->SetActorLocation(FVector(-5000, 1560, 118));
		Controller = World->SpawnActor<AAirlockPlayerController>(Params); Controller->Possess(Crew);
	}
	~FCheatTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit); World->GetGameInstance()->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	}
};

/** Sets airlock.DevCheats for the test and restores whatever it was, whatever the outcome. */
struct FCheatVariableScope
{
	IConsoleVariable* Var; int32 Saved;
	FCheatVariableScope() { Var = IConsoleManager::Get().FindConsoleVariable(TEXT("airlock.DevCheats")); Saved = Var ? Var->GetInt() : 1; }
	~FCheatVariableScope() { Set(Saved); }
	// SetByConsole: the proofs flip the variable from the console, and a lower-priority set would be ignored after that.
	void Set(int32 Value) { if (Var) { Var->Set(Value, ECVF_SetByConsole); } }
};

class FCheatLogCapture : public FOutputDevice
{
public:
	FCheatLogCapture() { GLog->AddOutputDevice(this); }
	~FCheatLogCapture() { GLog->RemoveOutputDevice(this); }
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{ if (Category == TEXT("LogAirlock")) { FScopeLock Lock(&Mutex); Lines += V; Lines += TEXT("\n"); } }
	FString Text() const { GLog->Flush(); FScopeLock Lock(&Mutex); return Lines; }
	int32 Count(const FString& Needle) const
	{ TArray<FString> Out; Text().ParseIntoArrayLines(Out); int32 N = 0; for (const auto& L : Out) { if (L.Contains(Needle)) { ++N; } } return N; }
private:
	mutable FCriticalSection Mutex; FString Lines;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCheatGateVariableTest, "Airlock.Cheats.GateFollowsConsoleVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCheatGateVariableTest::RunTest(const FString& Parameters)
{
	FCheatVariableScope Cheats; TestNotNull(TEXT("airlock.DevCheats exists"), Cheats.Var); if (!Cheats.Var) { return false; }
	FCheatTestWorld S;
	Cheats.Set(1);
	TestTrue(TEXT("Game mode allows cheats at 1"), S.Mode->AllowCheats(S.Controller));
	S.Controller->ServerWarp(FVector(-4000, 1560, 118));
	TestEqual(TEXT("Warp accepted at 1"), S.Crew->GetActorLocation().X, -4000.0, 1.0);

	TestNotNull(TEXT("A controller spawned at 1 gets the engine's cheat manager"), S.Controller->CheatManager.Get());

	Cheats.Set(0);
	TestFalse(TEXT("Game mode refuses cheats at 0"), S.Mode->AllowCheats(S.Controller));
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Late = S.World->SpawnActor<AAirlockPlayerController>(Params);
		TestNull(TEXT("A controller spawned at 0 gets no cheat manager"), Late->CheatManager.Get());
	}
	{
		FCheatLogCapture Log;
		S.Controller->ServerWarp(FVector(-3000, 1560, 118));
		TestEqual(TEXT("Warp refused at 0"), S.Crew->GetActorLocation().X, -4000.0, 1.0);
		TestEqual(TEXT("One refusal logged"), Log.Count(TEXT("Cheat AirlockWarp refused")), 1);
		TestEqual(TEXT("Refusal names the switch"), Log.Count(TEXT("airlock.DevCheats is 0")), 1);
	}

	Cheats.Set(1);
	S.Controller->ServerWarp(FVector(-3000, 1560, 118));
	TestEqual(TEXT("Warp accepted again at 1"), S.Crew->GetActorLocation().X, -3000.0, 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCheatGateEveryRpcTest, "Airlock.Cheats.EveryConsoleRpcIsGated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCheatGateEveryRpcTest::RunTest(const FString& Parameters)
{
	FCheatVariableScope Cheats; if (!Cheats.Var) { AddError(TEXT("airlock.DevCheats missing")); return false; }
	FCheatTestWorld S; auto* PC = S.Controller;
	Cheats.Set(0);
	const FVector Before = S.Crew->GetActorLocation();
	const float FuelBefore = S.State->Fuel.Units; const double ClockBefore = S.State->GetVoyageSecondsRemaining();
	const FString CargoBefore = S.Mode->CargoSummary(); const FString ShipBefore = S.Mode->ShipSummary();
	const int32 BaseFuelBefore = S.World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>()->GetBaseFuelTonnes();
	int32 Calls = 0;
	{
		FCheatLogCapture Log;
		PC->ServerSetVoyageClock(100.f); ++Calls;
		PC->ServerWarp(FVector::ZeroVector); ++Calls;
		PC->ServerShipFault(TEXT("Coolant_1"), TEXT("Leak")); ++Calls;
		PC->ServerAdjustShipModule(TEXT("PowerCore"), 50.f, false); ++Calls;
		PC->ServerShipStatus(); ++Calls;
		PC->ServerHoldByLabel(TEXT("Coolant_1")); ++Calls;
		PC->ServerReleaseHold(); ++Calls;
		PC->ServerIncident(TEXT("CoolantLeak"), NAME_None, 40.f); ++Calls;
		PC->ServerIncidentSchedule(); ++Calls;
		PC->ServerCargoStatus(); ++Calls;
		PC->ServerCarryById(TEXT("C1")); ++Calls;
		PC->ServerDropCargo(); ++Calls;
		PC->ServerPlaceById(TEXT("Clamp_1")); ++Calls;
		PC->ServerCrateHeat(TEXT("C2"), 70.f); ++Calls;
		PC->ServerCargoTally(); ++Calls;
		PC->ServerFuelStatus(); ++Calls;
		PC->ServerFuelSet(10.f); ++Calls;
		PC->ServerFuelLeak(); ++Calls;
		PC->ServerBypassFromConsole(); ++Calls;
		PC->ServerFeedById(TEXT("C1")); ++Calls;
		PC->ServerAirStatus(); ++Calls;
		PC->ServerScrubFromConsole(TEXT("Hazard")); ++Calls;
		PC->ServerAirSet(50.f); ++Calls;
		PC->ServerExpose(100.f); ++Calls;
		PC->ServerBaseFuel(0, 0); ++Calls; // the read-only mode: a regression here must never touch the real save slot
		PC->ServerSitByStationId(TEXT("Bridge")); ++Calls;
		PC->ServerStandUpFromConsole(); ++Calls;
		PC->ServerRefineryStatus(); ++Calls; // M5-3a: the floor's console helpers
		PC->ServerKnob(TEXT("Still_1"), TEXT("Heat"), 8, false); ++Calls;
		PC->ServerStillLoad(TEXT("Still_1"), TEXT("C1")); ++Calls;
		PC->ServerStillLever(TEXT("Still_1"), TEXT("Reset")); ++Calls;
		PC->ServerPress(TEXT("Still_1_HeatPlus")); ++Calls;
		TestEqual(TEXT("Thirty-two helpers"), Calls, 32);
		TestEqual(TEXT("Every call refused"), Log.Count(TEXT("Cheat ")), Calls);
		TestEqual(TEXT("Every refusal names the switch"), Log.Count(TEXT("refused for")), Calls);
		TestEqual(TEXT("No status line leaked"), Log.Count(TEXT("Ship ")), 0);
	}
	TestEqual(TEXT("Pawn unmoved"), S.Crew->GetActorLocation(), Before);
	TestEqual(TEXT("Fuel unchanged"), S.State->Fuel.Units, FuelBefore);
	TestFalse(TEXT("No leak"), S.State->Fuel.bFeedLeak); TestFalse(TEXT("No bypass"), S.State->Fuel.bBypassOpen);
	TestEqual(TEXT("Clock unchanged"), S.State->GetVoyageSecondsRemaining(), ClockBefore, 0.5);
	TestEqual(TEXT("Cargo unchanged"), S.Mode->CargoSummary(), CargoBefore);
	TestEqual(TEXT("Ship unchanged"), S.Mode->ShipSummary(), ShipBefore);
	TestEqual(TEXT("Air unchanged"), S.State->Atmosphere.Ppm, 0.f);
	TestEqual(TEXT("Base fuel unchanged"), S.World->GetGameInstance()->GetSubsystem<UVoyageSubsystem>()->GetBaseFuelTonnes(), BaseFuelBefore);
	TestNull(TEXT("Nothing carried"), S.Crew->CarriedCrate.Get());

	// The board's own path is not a cheat: it still books with cheats off.
	S.State->bVoyageBooked = false;
	PC->ServerChooseVoyage(EVoyageType::MilkRun);
	TestTrue(TEXT("Booking is not gated"), S.State->bVoyageBooked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConsoleFilePerInstanceTest, "Airlock.Cheats.ConsoleFileFollowsPIEInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FConsoleFilePerInstanceTest::RunTest(const FString& Parameters)
{
	const FString Primary = UAgentConsoleSubsystem::CommandFilePath();
	TestTrue(TEXT("Primary under Saved/AgentConsole"), Primary.EndsWith(TEXT("AgentConsole/commands.txt")) || Primary.EndsWith(TEXT("AgentConsole\\commands.txt")));
	TestEqual(TEXT("No PIE = primary"), UAgentConsoleSubsystem::CommandFilePathFor(INDEX_NONE), Primary);
	TestEqual(TEXT("Instance 0 = primary"), UAgentConsoleSubsystem::CommandFilePathFor(0), Primary);
	TestTrue(TEXT("Client 1"), UAgentConsoleSubsystem::CommandFilePathFor(1).EndsWith(TEXT("commands.client1.txt")));
	TestTrue(TEXT("Client 2"), UAgentConsoleSubsystem::CommandFilePathFor(2).EndsWith(TEXT("commands.client2.txt")));
	TestEqual(TEXT("Primary prefix"), UAgentConsoleSubsystem::EchoPrefixFor(0), FString(TEXT("AgentConsole")));
	TestEqual(TEXT("No PIE prefix"), UAgentConsoleSubsystem::EchoPrefixFor(INDEX_NONE), FString(TEXT("AgentConsole")));
	TestEqual(TEXT("Client prefix"), UAgentConsoleSubsystem::EchoPrefixFor(1), FString(TEXT("AgentConsole client 1")));
	return true;
}
#endif
