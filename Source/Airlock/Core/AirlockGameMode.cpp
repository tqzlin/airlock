// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockGameMode.h"
#include "Airlock.h"
#include "Core/AirlockCharacter.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Core/AirlockGameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Ship/AirlockDoor.h"
#include "Ship/ShipLayouts.h"
#include "Ship/ShipModule.h"
#include "Ship/ShipLine.h"
#include "TimerManager.h"
#include "UI/VoyageBoardWidget.h"
#include "Voyage/VoyageSubsystem.h"

const FName AAirlockGameMode::HubSectionId(TEXT("StationHub"));

AAirlockGameMode::AAirlockGameMode()
{
	DefaultPawnClass = AAirlockCharacter::StaticClass();
	PlayerControllerClass = AAirlockPlayerController::StaticClass();
	GameStateClass = AAirlockGameState::StaticClass();

	// Section-to-section moves through an airlock are level transitions; keep the player connection alive across them.
	bUseSeamlessTravel = true;
}

void AAirlockGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (UAirlockGameInstance* GI = Cast<UAirlockGameInstance>(GetGameInstance()))
	{
		bArrivedByAirlock = GI->bArrivingByAirlock;
		GI->bArrivingByAirlock = false;
	}
	UE_LOG(LogAirlock, Log, TEXT("AirlockGameMode::InitGame map=%s options=%s arrivedByAirlock=%d"), *MapName, *Options, bArrivedByAirlock ? 1 : 0);
}

AActor* AAirlockGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (bArrivedByAirlock && GetWorld())
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			// APlayerStartPIE lives in an editor-only header; match it by class name instead of including it.
			if (It->GetClass()->GetName() != TEXT("PlayerStartPIE"))
			{
				UE_LOG(LogAirlock, Log, TEXT("Arrival spawn for %s: %s at %s"), Player ? *Player->GetName() : TEXT("?"), *It->GetName(), *It->GetActorLocation().ToString());
				return *It;
			}
		}
	}
	AActor* Chosen = Super::ChoosePlayerStart_Implementation(Player);
	UE_LOG(LogAirlock, Log, TEXT("PlayerStart for %s: %s at %s"), Player ? *Player->GetName() : TEXT("?"),
		Chosen ? *Chosen->GetName() : TEXT("none"), Chosen ? *Chosen->GetActorLocation().ToString() : TEXT("-"));
	return Chosen;
}

void AAirlockGameMode::InitGameState()
{
	Super::InitGameState();

	AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	const UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	if (!GS)
	{
		return;
	}

	// Section id = map name without the PIE prefix ("StationHub", "Hauler_Mk1", ...).
	FString MapName = GetWorld() ? GetWorld()->GetMapName() : FString();
	if (GetWorld())
	{
		MapName = GetWorld()->RemovePIEPrefix(MapName);
	}
	GS->CurrentSectionId = FName(*MapName);
	GS->DaySeed = UVoyageSubsystem::TodaySeed();
	GS->bVoyageBooked = Voyages && Voyages->HasPendingVoyage();

	FVoyageSpec Spec;
	if (Voyages && Voyages->GetPendingVoyage(Spec))
	{
		UE_LOG(LogAirlock, Log, TEXT("Section %s: voyage underway (type=%d ship=%s captain=%s)"),
			*MapName, static_cast<int32>(Spec.Type), *Spec.ShipClassId.ToString(), *Spec.Captain.ToString());
	}
	else
	{
		UE_LOG(LogAirlock, Log, TEXT("Section %s: no voyage booked (day seed %d)"), *MapName, GS->DaySeed);
	}
}

void AAirlockGameMode::BeginPlay()
{
	Super::BeginPlay();
	BuildShipGraph();

	int32 Bound = 0;
	for (TActorIterator<AAirlockDoor> It(GetWorld()); It; ++It)
	{
		It->OnTransitionRequested.AddUniqueDynamic(this, &AAirlockGameMode::HandleTransitionRequested);
		if (It->TargetSectionId == HubSectionId && !It->TargetLevel.IsNull())
		{
			HubLevel = It->TargetLevel;
		}
		++Bound;
	}
	UE_LOG(LogAirlock, Log, TEXT("AirlockGameMode: listening to %d airlock door(s)"), Bound);

	AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	const UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	if (GS && GS->CurrentSectionId != HubSectionId && Voyages && Voyages->GetPendingVoyage(ActiveVoyage))
	{
		CaptainSchedule = FVoyageSchedule::Build(ActiveVoyage);
		GS->bVoyageActive = true;
		GS->VoyageEndServerTime = GS->GetServerWorldTimeSeconds() + ActiveVoyage.DurationSeconds;
		GS->ForceNetUpdate();
		UE_LOG(LogAirlock, Log, TEXT("Voyage clock started: captain=%s duration=%.0fs endServerTime=%.3f"),
			*ActiveVoyage.Captain.ToString(), ActiveVoyage.DurationSeconds, GS->VoyageEndServerTime);
		ScheduleNextVoyageEvent();
	}
}

void AAirlockGameMode::HandleTransitionRequested(AAirlockDoor* Door, APawn* Pawn, FName TargetSectionId)
{
	if (bTravelPending || !Door || !GetWorld())
	{
		return;
	}
	if (Door->TargetLevel.IsNull())
	{
		if (!TargetSectionId.IsNone())
		{
			UE_LOG(LogAirlock, Log, TEXT("Airlock %s: section '%s' has no level set - staying put"), *Door->GetName(), *TargetSectionId.ToString());
		}
		return;
	}

	UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	const bool bToHub = (TargetSectionId == HubSectionId);
	if (!bToHub && !(Voyages && Voyages->HasPendingVoyage()))
	{
		UE_LOG(LogAirlock, Log, TEXT("Airlock %s: %s tried to board with no voyage booked - use the departure board first"),
			*Door->GetName(), Pawn ? *Pawn->GetName() : TEXT("?"));
		return;
	}
	if (bToHub && Voyages)
	{
		Voyages->ClearPendingVoyage();
	}

	// Card text: where we're going, and who's flying if it's a voyage.
	FVoyageSpec Spec;
	const bool bHasSpec = Voyages && Voyages->GetPendingVoyage(Spec);
	const FText Title = bToHub
		? NSLOCTEXT("Airlock", "TransitionToHub", "RETURNING TO STATION")
		: NSLOCTEXT("Airlock", "TransitionBoarding", "CYCLING AIRLOCK");
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	const FText FromSection = FText::FromName(GS ? GS->CurrentSectionId : NAME_None);
	const FText Subtitle = (bHasSpec && !bToHub)
		? UVoyageBoardWidget::DescribeOffer(Spec)
		: FText::Format(NSLOCTEXT("Airlock", "TransitionGeneric", "{0}  ->  {1}"), FromSection, FText::FromName(TargetSectionId));
	UE_LOG(LogAirlock, Log, TEXT("Airlock %s: travelling to %s (section %s) in %.1fs"), *Door->GetName(), *Door->TargetLevel.GetLongPackageName(), *TargetSectionId.ToString(), TravelFadeDelay);
	TravelToSection(Door->TargetLevel, TargetSectionId, Title, Subtitle);
}

void AAirlockGameMode::TravelToSection(const TSoftObjectPtr<UWorld>& Level, FName SectionId, const FText& Title, const FText& Subtitle)
{
	const FString URL = Level.GetLongPackageName();
	bTravelPending = true;
	GetWorldTimerManager().ClearTimer(VoyageTimer);
	if (AAirlockGameState* GS = GetGameState<AAirlockGameState>())
	{
		GS->bVoyageActive = false;
		GS->VoyageEndServerTime = 0.0;
		GS->ForceNetUpdate();
	}
	if (UAirlockGameInstance* GI = Cast<UAirlockGameInstance>(GetGameInstance()))
	{
		GI->bArrivingByAirlock = true;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AAirlockPlayerController* PC = Cast<AAirlockPlayerController>(It->Get()))
		{
			PC->ClientBeginTransition(Title, Subtitle);
		}
	}

	UE_LOG(LogAirlock, Log, TEXT("Transition: %s -> %s (section %s) in %.1fs"), *Title.ToString(), *URL, *SectionId.ToString(), TravelFadeDelay);
	GetWorldTimerManager().SetTimer(TravelTimer, FTimerDelegate::CreateWeakLambda(this, [this, URL]()
	{
		if (GetWorld())
		{
			GetWorld()->ServerTravel(URL, /*bAbsolute*/ false);
		}
	}), TravelFadeDelay, false);
}

void AAirlockGameMode::SetVoyageSecondsRemaining(float SecondsRemaining)
{
	float ClampedSeconds = 0.f;
	if (!FVoyageSchedule::TryClampRemainingSeconds(SecondsRemaining, ActiveVoyage.DurationSeconds, ClampedSeconds))
	{
		UE_LOG(LogAirlock, Log, TEXT("Voyage clock: ignored non-finite seconds remaining"));
		return;
	}
	AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!GS || !GS->bVoyageActive || bTravelPending)
	{
		UE_LOG(LogAirlock, Log, TEXT("Voyage clock: ignored adjustment with no active voyage"));
		return;
	}
	GS->VoyageEndServerTime = GS->GetServerWorldTimeSeconds() + ClampedSeconds;
	GS->ForceNetUpdate();
	UE_LOG(LogAirlock, Log, TEXT("Voyage clock moved: requested=%.3fs remaining=%.3fs endServerTime=%.3f nextAnnouncement=%d"),
		SecondsRemaining, ClampedSeconds, GS->VoyageEndServerTime, NextCaptainEntry);
	ScheduleNextVoyageEvent();
}

void AAirlockGameMode::ScheduleNextVoyageEvent()
{
	GetWorldTimerManager().ClearTimer(VoyageTimer);
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!GS || !GS->bVoyageActive || bTravelPending)
	{
		return;
	}
	const double Elapsed = ActiveVoyage.DurationSeconds - (GS->VoyageEndServerTime - GS->GetServerWorldTimeSeconds());
	const double NextAt = CaptainSchedule.IsValidIndex(NextCaptainEntry)
		? CaptainSchedule[NextCaptainEntry].AtSeconds : ActiveVoyage.DurationSeconds;
	const double Delay = NextAt - Elapsed;
	if (Delay <= 0.0)
	{
		// Includes departure and clock jumps: catch up in order on the next tick, never inside the RPC.
		VoyageTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AAirlockGameMode::TickVoyageClock);
	}
	else
	{
		GetWorldTimerManager().SetTimer(VoyageTimer, this, &AAirlockGameMode::TickVoyageClock, static_cast<float>(Delay), false);
	}
}

void AAirlockGameMode::TickVoyageClock()
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!GS || !GS->bVoyageActive || bTravelPending)
	{
		return;
	}
	const double Elapsed = ActiveVoyage.DurationSeconds - (GS->VoyageEndServerTime - GS->GetServerWorldTimeSeconds());
	const int32 DueEnd = FVoyageSchedule::FindNextUnfiredEntry(CaptainSchedule, NextCaptainEntry, Elapsed);
	while (NextCaptainEntry < DueEnd)
	{
		const FText& Line = CaptainSchedule[NextCaptainEntry++].Line;
		UE_LOG(LogAirlock, Log, TEXT("Captain %s: %s"), *ActiveVoyage.Captain.ToString(), *Line.ToString());
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (AAirlockPlayerController* PC = Cast<AAirlockPlayerController>(It->Get()))
			{
				PC->ClientCaptainSays(Line);
			}
		}
	}
	if (Elapsed >= ActiveVoyage.DurationSeconds)
	{
		ReturnToHub();
	}
	else
	{
		ScheduleNextVoyageEvent();
	}
}

void AAirlockGameMode::ReturnToHub()
{
	if (bTravelPending || !GetWorld())
	{
		return;
	}
	if (UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr)
	{
		Voyages->ClearPendingVoyage();
	}
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	const FText Subtitle = FText::Format(NSLOCTEXT("Airlock", "TransitionGeneric", "{0}  ->  {1}"),
		FText::FromName(GS ? GS->CurrentSectionId : NAME_None), FText::FromName(HubSectionId));
	UE_LOG(LogAirlock, Log, TEXT("Voyage clock expired: travelling to %s (section %s) in %.1fs"),
		*HubLevel.GetLongPackageName(), *HubSectionId.ToString(), TravelFadeDelay);
	TravelToSection(HubLevel, HubSectionId, NSLOCTEXT("Airlock", "TransitionToHub", "RETURNING TO STATION"), Subtitle);
}

void AAirlockGameMode::BuildShipGraph()
{
	AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	const FShipLayout* Layout = GS ? ShipLayouts::Find(GS->CurrentSectionId) : nullptr;
	if (!HasAuthority() || !Layout) { return; }
	ShipGraph.Build(*Layout);
	PublishShipGraph();
	UE_LOG(LogAirlock, Log, TEXT("Ship graph built: %d modules, %d lines (section %s)"),
		ShipGraph.Modules().Num(), ShipGraph.Lines().Num(), *Layout->SectionId.ToString());
	for (const FShipModuleSpec& Spec : Layout->Modules)
	{
		FActorSpawnParameters Params;
		Params.Name = Spec.Id;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AShipModule* Actor = GetWorld()->SpawnActor<AShipModule>(AShipModule::StaticClass(), Spec.Transform, Params);
		if (Actor)
		{
			Actor->Initialize(Spec);
#if WITH_EDITOR
			Actor->SetActorLabel(Spec.Id.ToString(), false);
			Actor->SetFolderPath(TEXT("Ship"));
#endif
			Actor->FinishSpawning(Spec.Transform);
		}
	}
	for (const FShipLineSpec& Spec : Layout->Lines)
	{
		FActorSpawnParameters Params;
		Params.Name = Spec.Id;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AShipLine* Actor = GetWorld()->SpawnActor<AShipLine>(AShipLine::StaticClass(), Spec.Transform, Params);
		if (Actor)
		{
			Actor->Initialize(Spec);
#if WITH_EDITOR
			Actor->SetActorLabel(Spec.Id.ToString(), false);
			Actor->SetFolderPath(TEXT("Ship"));
#endif
			Actor->FinishSpawning(Spec.Transform);
		}
	}
	GetWorldTimerManager().SetTimer(ShipGraphTimer, this, &AAirlockGameMode::TickShipGraph, 0.25f, true);
}

void AAirlockGameMode::TickShipGraph()
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS || !GS->bVoyageActive) { return; }
	ShipGraph.Tick(0.25f);
	PublishShipGraph();
}

void AAirlockGameMode::PublishShipGraph()
{
	AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS) { return; }
	for (const FShipModuleState& State : ShipGraph.Modules())
	{
		const FShipModuleState* Old = GS->ShipModules.FindByPredicate([&State](const FShipModuleState& M) { return M.Id == State.Id; });
		if (Old && Old->Status != State.Status)
		{
			UE_LOG(LogAirlock, Log, TEXT("Ship %s %s cond=%.2f"), *State.Id.ToString(), FShipGraph::StatusName(State.Status), State.Condition);
		}
	}
	for (const FShipLineState& State : ShipGraph.Lines())
	{
		const FShipLineState* Old = GS->ShipLines.FindByPredicate([&State](const FShipLineState& L) { return L.Id == State.Id; });
		if (Old && Old->Fault != State.Fault)
		{
			UE_LOG(LogAirlock, Log, TEXT("Line %s fault=%s"), *State.Id.ToString(), FShipGraph::FaultName(State.Fault));
		}
	}
	GS->PublishShipGraph(ShipGraph);
}

bool AAirlockGameMode::ShipSetLineFault(FName Id, EShipLineFault Fault)
{
	if (!HasAuthority() || !ShipGraph.SetLineFault(Id, Fault)) { return false; }
	PublishShipGraph();
	return true;
}
bool AAirlockGameMode::ShipRepairLine(FName Id) { return ShipSetLineFault(Id, EShipLineFault::None); }
bool AAirlockGameMode::ShipDamageModule(FName Id, float Amount)
{
	if (!HasAuthority() || !ShipGraph.DamageModule(Id, Amount)) { return false; }
	PublishShipGraph();
	return true;
}
bool AAirlockGameMode::ShipRepairModule(FName Id, float Amount)
{
	if (!HasAuthority() || !ShipGraph.RepairModule(Id, Amount)) { return false; }
	PublishShipGraph();
	return true;
}
FString AAirlockGameMode::ShipSummary() const { return ShipGraph.Summary(); }
