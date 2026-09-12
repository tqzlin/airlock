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
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"
#include "Ship/CargoLayouts.h"
#include "TimerManager.h"
#include "UI/VoyageBoardWidget.h"
#include "Voyage/VoyageSubsystem.h"
#include "Voyage/IncidentDirectorComponent.h"

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
	if (GS->CurrentSectionId == HubSectionId && Voyages)
	{
		GS->bHasLastVoyageResult = Voyages->GetLastVoyageResult(GS->LastVoyageResult);
	}

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
	BuildCargo();

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
		bStartedVoyageClock = true;
		GS->bVoyageActive = true;
		GS->VoyageEndServerTime = GS->GetServerWorldTimeSeconds() + ActiveVoyage.DurationSeconds;
		GS->ForceNetUpdate();
		UE_LOG(LogAirlock, Log, TEXT("Voyage clock started: captain=%s duration=%.0fs endServerTime=%.3f"),
			*ActiveVoyage.Captain.ToString(), ActiveVoyage.DurationSeconds, GS->VoyageEndServerTime);
		if (const FShipLayout* Layout = ShipLayouts::Find(GS->CurrentSectionId))
		{
			int32 CrewCount = 0;
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				if (It->IsValid()) { ++CrewCount; }
			}
			IncidentDirector = NewObject<UIncidentDirectorComponent>(this, TEXT("IncidentDirector"));
			IncidentDirector->RegisterComponent();
			IncidentDirector->StartVoyage(ActiveVoyage, *Layout, FMath::Max(1, CrewCount), CargoLayouts::Find(GS->CurrentSectionId));
		}
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
		RecordVoyageResult();
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
	// Queue incidents first so a jump straight to docking consumes them before the clock ends the voyage.
	if (IncidentDirector) { IncidentDirector->Reschedule(); }
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
	// Timer heap ties are unordered. A direct jump to docking must consume incidents while the voyage
	// is still active, before the fixed docking caption and return path, whichever timer ran first.
	if (Elapsed >= ActiveVoyage.DurationSeconds && IncidentDirector) { IncidentDirector->FireDueIncidents(); }
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
		RecordVoyageResult();
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
	if (CargoModel.Tick(0.25f, ActiveVoyage.DurationSeconds - GS->GetVoyageSecondsRemaining())) { PublishCargo(); }
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

void AAirlockGameMode::BuildCargo()
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	const FCargoLayout* Layout = GS ? CargoLayouts::Find(GS->CurrentSectionId) : nullptr;
	if (!HasAuthority() || !Layout) { return; }
	CargoModel.Build(*Layout);
	PublishCargo();
	for (const auto& Spec : Layout->Slots)
	{
		FActorSpawnParameters Params;
		Params.Name = Spec.Id;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (auto* Actor = GetWorld()->SpawnActor<ACargoSlot>(ACargoSlot::StaticClass(), Spec.Transform, Params))
		{
			Actor->Initialize(Spec);
#if WITH_EDITOR
			Actor->SetActorLabel(Spec.Id.ToString(), false);
			Actor->SetFolderPath(TEXT("Cargo"));
#endif
			Actor->FinishSpawning(Spec.Transform);
		}
	}
	for (const auto& Spec : Layout->Crates)
	{
		FActorSpawnParameters Params;
		Params.Name = Spec.Id;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (auto* Actor = GetWorld()->SpawnActor<ACargoCrate>(ACargoCrate::StaticClass(), Spec.Transform, Params))
		{
			Actor->Initialize(Spec);
#if WITH_EDITOR
			Actor->SetActorLabel(Spec.Id.ToString(), false);
			Actor->SetFolderPath(TEXT("Cargo"));
#endif
			Actor->FinishSpawning(Spec.Transform);
		}
	}
	// A cargo-only section still uses the shared systems timer.
	if (!GetWorldTimerManager().IsTimerActive(ShipGraphTimer))
	{
		GetWorldTimerManager().SetTimer(ShipGraphTimer, this, &AAirlockGameMode::TickShipGraph, .25f, true);
	}
}
void AAirlockGameMode::PublishCargo()
{
	AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS) { return; }
	TArray<FCargoCrateState> Mirror = CargoModel.Crates();
	const float Warn = FCargoRates().WarnTemperature;
	for (auto& State : Mirror)
	{
		State.Temperature = FMath::FloorToFloat(State.Temperature);
		const auto* Old = GS->Cargo.FindByPredicate([&State](const auto& C) { return C.Id == State.Id; });
		if (!Old || Old->State != State.State || ((Old->Temperature >= Warn) != (State.Temperature >= Warn)))
		{
			UE_LOG(LogAirlock, Log, TEXT("Cargo %s %s temp=%d"), *State.Id.ToString(), FCargoModel::StateName(State.State), FMath::FloorToInt(State.Temperature));
		}
	}
	GS->PublishCargo(Mirror);
}
bool AAirlockGameMode::CargoPickUp(FName Id)
{
	if (!HasAuthority() || !CargoModel.PickUp(Id)) { return false; }
	PublishCargo(); return true;
}
bool AAirlockGameMode::CargoPutDown(FName Id)
{
	if (!HasAuthority() || !CargoModel.PutDown(Id)) { return false; }
	PublishCargo(); return true;
}
bool AAirlockGameMode::CargoPlace(FName Crate, FName Slot)
{
	if (!HasAuthority() || !CargoModel.Place(Crate, Slot)) { return false; }
	PublishCargo(); return true;
}
bool AAirlockGameMode::CargoRelease(FName Slot)
{
	if (!HasAuthority() || !CargoModel.Release(Slot)) { return false; }
	PublishCargo(); return true;
}
bool AAirlockGameMode::CargoSetTemperature(FName Id, float Temperature)
{
	if (!HasAuthority() || !CargoModel.SetTemperature(Id, Temperature)) { return false; }
	PublishCargo(); return true;
}
FName AAirlockGameMode::CargoFirstFilledClamp() const
{
	for (const auto& Slot : CargoModel.Slots())
	{
		const auto* Crate = CargoModel.FindCrate(Slot.CrateId);
		if (!Slot.bRack && Crate && Crate->State == ECargoState::Secured) { return Slot.Id; }
	}
	return NAME_None;
}
FVoyageResult AAirlockGameMode::CargoTally() const { return FCargoModel::Tally(CargoModel, ShipGraph, ActiveVoyage); }
FString AAirlockGameMode::CargoSummary() const { return CargoModel.Summary(); }

void AAirlockGameMode::RecordVoyageResult()
{
	if (!HasAuthority() || !bStartedVoyageClock || bRecordedVoyageResult) { return; }
	if (auto* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr)
	{
		const FVoyageResult Result = CargoTally();
		Voyages->SetLastVoyageResult(Result);
		bRecordedVoyageResult = true;
		UE_LOG(LogAirlock, Log, TEXT("Voyage result: %s"), *Result.Summary());
	}
}
