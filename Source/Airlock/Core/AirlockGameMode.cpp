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
#include "Ship/ControlStation.h"
#include "Ship/ShipLayouts.h"
#include "Ship/ShipModule.h"
#include "Ship/ShipLine.h"
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"
#include "Ship/CargoLayouts.h"
#include "Station/StationSign.h"
#include "Ship/FuelLayouts.h"
#include "Ship/FuelValve.h"
#include "Ship/BypassValve.h"
#include "Ship/FuelIntake.h"
#include "TimerManager.h"
#include "UI/VoyageBoardWidget.h"
#include "Voyage/VoyageSubsystem.h"
#include "Voyage/IncidentDirectorComponent.h"
#include "HAL/IConsoleManager.h"

/** The one switch behind every Airlock* console helper (M4-1). On in every build but Shipping; a server operator can set it. */
static TAutoConsoleVariable<int32> CVarAirlockDevCheats(
	TEXT("airlock.DevCheats"),
#if UE_BUILD_SHIPPING
	0,
#else
	1,
#endif
	TEXT("1 = the Airlock* console helpers (warp, clock, faults, cargo, fuel, air, save) are accepted from any client; 0 = every one is refused and logged."),
	ECVF_Default);

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

APawn* AAirlockGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	// Crew arrive in a row (M4-1): the second player through an airlock found the only PlayerStart occupied by the
	// first and came home as a bodiless spectator ("SpawnActor failed because of collision"). A candidate spot with
	// a pawn within 90 cm is skipped, the next lies ArrivalSpacing cm along the start's right axis, alternating
	// sides; when nothing within 240 cm is clear the pawn is still spawned, adjusted where possible.
	UWorld* World = GetWorld();
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!World || !PawnClass) { return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform); }
	const FVector Right = SpawnTransform.GetRotation().GetRightVector();
	static const float Steps[] = { 0.f, 1.f, -1.f, 2.f, -2.f };
	for (float Step : Steps)
	{
		FTransform Candidate = SpawnTransform;
		Candidate.SetLocation(SpawnTransform.GetLocation() + Right * (Step * ArrivalSpacing));
		bool bOccupied = false;
		for (TActorIterator<APawn> It(World); It && !bOccupied; ++It)
		{
			bOccupied = FVector::DistSquared2D(It->GetActorLocation(), Candidate.GetLocation()) < FMath::Square(ArrivalSpacing * 0.75f);
		}
		if (bOccupied) { continue; }
		FActorSpawnParameters Params;
		Params.Instigator = GetInstigator();
		Params.ObjectFlags |= RF_Transient;
		if (APawn* Pawn = World->SpawnActor<APawn>(PawnClass, Candidate, Params))
		{
			if (Step != 0.f)
			{
				UE_LOG(LogAirlock, Log, TEXT("Arrival spawn for %s stepped %+.0f cm aside: the start was occupied"), NewPlayer ? *NewPlayer->GetName() : TEXT("?"), Step * ArrivalSpacing);
			}
			return Pawn;
		}
	}
	FActorSpawnParameters Forced;
	Forced.Instigator = GetInstigator();
	Forced.ObjectFlags |= RF_Transient;
	Forced.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	UE_LOG(LogAirlock, Warning, TEXT("Arrival spawn for %s forced at the start: no clear spot within %.0f cm"), NewPlayer ? *NewPlayer->GetName() : TEXT("?"), 2.f * ArrivalSpacing);
	return World->SpawnActor<APawn>(PawnClass, SpawnTransform, Forced);
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
		GS->BaseFuelTonnes = Voyages->GetBaseFuelTonnes();
		GS->RunsCompleted = Voyages->GetRunsCompleted();
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
	BuildStationSigns();
	BuildFuel();

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
		SetPieces = FuelHasSystem() ? FVoyageSetPieces::Build(ActiveVoyage) : TArray<FVoyageSetPiece>();
		NextSetPiece = 0; bFuelFollowupPending = false; bFuelShortSpoken = false;
		bStartedVoyageClock = true;
		if (AirHasAtmosphere()) { AtmosphereModel.Reset(); PublishAtmosphere(); }
		if (FuelHasSystem()) { FuelModel.Reset(); PublishFuel(); }
		AirCaptainLastSpoken.Reset();
		BlackoutCount = 0; CountedBlackouts.Reset();
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
			IncidentDirector->StartVoyage(ActiveVoyage, *Layout, FMath::Max(1, CrewCount), ActiveCargoLayout.Crates.IsEmpty() ? nullptr : &ActiveCargoLayout);
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
	// The hub-bound crossing is the refinery dock (M3-7): the card names the destination, not the map.
	const FText Title = bToHub
		? NSLOCTEXT("Airlock", "TransitionToHub", "DOCKING - HALDER SOLAR REFINERY")
		: NSLOCTEXT("Airlock", "TransitionBoarding", "CYCLING AIRLOCK");
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	const FText FromSection = FText::FromName(GS ? GS->CurrentSectionId : NAME_None);
	const FText Subtitle = bToHub
		? NSLOCTEXT("Airlock", "TransitionRefinery", "cargo off, fuel on - then home to the Vell Extractor")
		: bHasSpec ? UVoyageBoardWidget::DescribeOffer(Spec)
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
	double NextAt = CaptainSchedule.IsValidIndex(NextCaptainEntry)
		? CaptainSchedule[NextCaptainEntry].AtSeconds : ActiveVoyage.DurationSeconds;
	if (SetPieces.IsValidIndex(NextSetPiece)) { NextAt = FMath::Min(NextAt, static_cast<double>(SetPieces[NextSetPiece].AtSeconds)); }
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
		// On a jump to docking, consume the leak before the final docking caption.
		if (CaptainSchedule[NextCaptainEntry].AtSeconds >= ActiveVoyage.DurationSeconds) { FireDueSetPieces(Elapsed); }
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
	FireDueSetPieces(Elapsed);
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
	const FText Subtitle = NSLOCTEXT("Airlock", "TransitionRefinery", "cargo off, fuel on - then home to the Vell Extractor");
	UE_LOG(LogAirlock, Log, TEXT("Voyage clock expired: travelling to %s (section %s) in %.1fs"),
		*HubLevel.GetLongPackageName(), *HubSectionId.ToString(), TravelFadeDelay);
	TravelToSection(HubLevel, HubSectionId, NSLOCTEXT("Airlock", "TransitionToHub", "DOCKING - HALDER SOLAR REFINERY"), Subtitle);
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
	FVoyageSpec Pending;
	const UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	const FName Captain = (Voyages && Voyages->GetPendingVoyage(Pending)) ? Pending.Captain : FName(TEXT("Autopilot"));
	for (const FControlStationSpec& Spec : Layout->Stations)
	{
		FActorSpawnParameters Params;
		Params.Name = *FString::Printf(TEXT("Station_%s"), *Spec.Id.ToString());
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AControlStation* Actor = GetWorld()->SpawnActor<AControlStation>(AControlStation::StaticClass(), Spec.Transform, Params);
		if (Actor)
		{
			Actor->Initialize(Spec, Captain);
#if WITH_EDITOR
			Actor->SetActorLabel(FString::Printf(TEXT("Station_%s"), *Spec.Id.ToString()), false);
			Actor->SetFolderPath(TEXT("Ship"));
#endif
			Actor->FinishSpawning(Spec.Transform);
			UE_LOG(LogAirlock, Log, TEXT("Control station %s at %s (captain %s has the conn)"),
				*Spec.Id.ToString(), *Spec.Transform.GetLocation().ToString(), *Captain.ToString());
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
	const bool bAirFlagsChanged = UpdateAtmosphereFlags();
	if (CargoModel.Tick(0.25f, ActiveVoyage.DurationSeconds - GS->GetVoyageSecondsRemaining())) { PublishCargo(); }
	if (AirHasAtmosphere())
	{
		const bool bAirChanged = AtmosphereModel.Tick(0.25f, CargoModel.Crates());
		if (bAirChanged || bAirFlagsChanged) { PublishAtmosphere(); }
	}
	TickFuel();
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
	if (UpdateAtmosphereFlags()) { PublishAtmosphere(); }
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
	if (!HasAuthority() || !GS || !CargoLayouts::Find(GS->CurrentSectionId)) { return; }
	// A booked contract sizes the manifest (M3-7); a ship entered unbooked keeps the four-cask layout.
	FVoyageSpec Pending;
	const UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	ActiveCargoLayout = (Voyages && Voyages->GetPendingVoyage(Pending))
		? CargoLayouts::Build(GS->CurrentSectionId, Pending.CaskCount, Pending.VolatileCount)
		: *CargoLayouts::Find(GS->CurrentSectionId);
	const FCargoLayout* Layout = &ActiveCargoLayout;
	UE_LOG(LogAirlock, Log, TEXT("Cargo manifest: %d casks, %d slots"), Layout->Crates.Num(), Layout->Slots.Num());
	CargoModel.Build(*Layout);
	AtmosphereModel.Reset();
	PublishAtmosphere();
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
FVoyageResult AAirlockGameMode::CargoTally() const
{
	return FCargoModel::Tally(CargoModel, ShipGraph, ActiveVoyage, AirHasAtmosphere() ? &AtmosphereModel.State() : nullptr, BlackoutCount, FuelHasSystem() ? &FuelModel.State() : nullptr);
}
FString AAirlockGameMode::CargoSummary() const { return CargoModel.Summary(); }

void AAirlockGameMode::RecordVoyageResult()
{
	if (!HasAuthority() || !bStartedVoyageClock || bRecordedVoyageResult) { return; }
	if (auto* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr)
	{
		const FVoyageResult Result = CargoTally();
		Voyages->SetLastVoyageResult(Result);
		bRecordedVoyageResult = true;
		UE_LOG(LogAirlock, Log, TEXT("Voyage result: %s, fuel +%d t"), *Result.Summary(), Result.FuelSurplus);
	}
}

void AAirlockGameMode::BuildStationSigns()
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS) { return; }
	for (const FStationSignSpec& Spec : StationSigns::Find(GS->CurrentSectionId))
	{
		FActorSpawnParameters Params;
		Params.Name = Spec.Id;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (auto* Sign = GetWorld()->SpawnActor<AStationSign>(AStationSign::StaticClass(), Spec.Transform, Params))
		{
			Sign->Initialize(Spec);
#if WITH_EDITOR
			Sign->SetActorLabel(Spec.Id.ToString(), false);
			Sign->SetFolderPath(TEXT("Signs"));
#endif
			Sign->FinishSpawning(Spec.Transform);
		}
	}
}

bool AAirlockGameMode::AirHasAtmosphere() const
{
	const auto* GS = GetGameState<AAirlockGameState>();
	return GS && GS->AirHasAtmosphere();
}
bool AAirlockGameMode::UpdateAtmosphereFlags()
{
	if (!HasAuthority() || !AirHasAtmosphere()) { return false; }
	const auto Status = [this](FName Id)
	{
		const auto* M = ShipGraph.Modules().FindByPredicate([Id](const auto& Module) { return Module.Id == Id; });
		return M ? M->Status : EShipModuleStatus::Nominal;
	};
	const auto Core = Status(TEXT("PowerCore"));
	const auto Life = Status(TEXT("LifeSupport"));
	bool bChanged = AtmosphereModel.SetScrubDisabled(Life == EShipModuleStatus::Failed);
	bChanged |= AtmosphereModel.SetBrownout(Core != EShipModuleStatus::Nominal || Life == EShipModuleStatus::Degraded);
	CargoModel.SetRackCoolFactor((Status(TEXT("CoolantLoop")) == EShipModuleStatus::Nominal ? 1.f : .5f)
		* (AirGetScrubMode() == EScrubMode::Hazard && Core != EShipModuleStatus::Nominal ? .5f : 1.f));
	return bChanged;
}
void AAirlockGameMode::PublishAtmosphere()
{
	auto* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS || !AirHasAtmosphere()) { return; }
	FAtmosphereState Mirror = AtmosphereModel.State();
	Mirror.Ppm = FMath::FloorToFloat(Mirror.Ppm);
	const FAtmosphereState Old = GS->Atmosphere;
	const FAtmosphereRates R;
	bool bLog = Old.ScrubMode != Mirror.ScrubMode || Old.bBrownout != Mirror.bBrownout || Old.bScrubDisabled != Mirror.bScrubDisabled;
	for (const float Threshold : { R.CleanPpm, R.WarnPpm, R.DangerPpm })
	{
		bLog |= (Old.Ppm >= Threshold) != (Mirror.Ppm >= Threshold);
	}
	GS->PublishAtmosphere(Mirror, AtmosphereModel.PuffingCrates());
	// A single upward jump speaks only its highest crossed threshold.
	if (Old.Ppm < R.DangerPpm && Mirror.Ppm >= R.DangerPpm)
	{
		SpeakAirCaptain(TEXT("Danger"), NSLOCTEXT("Airlock", "AirDanger", "Captain {0} here. Air's bad. Get those scrubbers up and keep moving."));
	}
	else if (Old.Ppm < R.WarnPpm && Mirror.Ppm >= R.WarnPpm)
	{
		SpeakAirCaptain(TEXT("Warn"), NSLOCTEXT("Airlock", "AirWarn", "Captain {0} here. Nitriles in the hold air. Scrubbers to hazard mode - the life-support panel."));
	}
	else if (Old.Ppm >= R.WarnPpm && Mirror.Ppm < R.WarnPpm)
	{
		SpeakAirCaptain(TEXT("Clear"), NSLOCTEXT("Airlock", "AirClear", "Captain {0} here. Air's clearing. Good work."));
	}
	if (bLog)
	{
		UE_LOG(LogAirlock, Log, TEXT("Air %d ppm scrub=%s%s%s"), FMath::FloorToInt(Mirror.Ppm), FShipAtmosphere::ModeName(Mirror.ScrubMode),
			Mirror.bBrownout ? TEXT(" brownout") : TEXT(""), Mirror.bScrubDisabled ? TEXT(" disabled") : TEXT(""));
	}
}
bool AAirlockGameMode::AirSetScrubMode(EScrubMode Mode)
{
	if (!HasAuthority() || !AirHasAtmosphere()) { return false; }
	if (Mode != EScrubMode::Off && Mode != EScrubMode::Normal && Mode != EScrubMode::Hazard) { return false; }
	if (AtmosphereModel.SetScrubMode(Mode)) { UpdateAtmosphereFlags(); PublishAtmosphere(); }
	return true;
}
bool AAirlockGameMode::AirSetPpm(float Ppm)
{
	if (!HasAuthority() || !AirHasAtmosphere() || !AtmosphereModel.SetPpm(Ppm)) { return false; }
	PublishAtmosphere(); return true;
}
EScrubMode AAirlockGameMode::AirGetScrubMode() const { return AtmosphereModel.State().ScrubMode; }
FString AAirlockGameMode::AirSummary() const { return AirHasAtmosphere() ? AtmosphereModel.Summary() : TEXT("No atmosphere on this section"); }

void AAirlockGameMode::SpeakAirCaptain(FName Cue, const FText& Pattern, APawn* Pawn)
{
	if (!HasAuthority() || !GetWorld()) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const double* Last = AirCaptainLastSpoken.Find(Cue);
	if (Last && Now - *Last < 20.0) { return; }
	AirCaptainLastSpoken.Add(Cue, Now);
	const FText Line = FText::Format(Pattern, FText::FromName(ActiveVoyage.Captain), FText::FromString(GetNameSafe(Pawn)));
	UE_LOG(LogAirlock, Log, TEXT("Captain %s: %s"), *ActiveVoyage.Captain.ToString(), *Line.ToString());
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (auto* PC = Cast<AAirlockPlayerController>(It->Get())) { PC->ClientCaptainSays(Line); }
	}
}

void AAirlockGameMode::NotifyBlackout(APawn* Pawn)
{
	const auto* Crew = Cast<AAirlockCharacter>(Pawn);
	const auto* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !IsValid(Crew) || Crew->GetWorld() != GetWorld() || !Crew->bBlackedOut
		|| !AirHasAtmosphere() || !GS || !GS->bVoyageActive) { return; }
	const double* Counted = CountedBlackouts.Find(Pawn);
	if (Counted && *Counted == Crew->BlackoutEndServerTime) { return; }
	CountedBlackouts.Add(Pawn, Crew->BlackoutEndServerTime);
	++BlackoutCount;
	SpeakAirCaptain(TEXT("Blackout"), NSLOCTEXT("Airlock", "AirBlackout", "Captain {0} here. {1} is down - med-bot's on it."), Pawn);
}

bool AAirlockGameMode::FuelHasSystem() const
{
	const auto* GS = GetGameState<AAirlockGameState>();
	return GS && FuelLayouts::Find(GS->CurrentSectionId) != nullptr;
}
void AAirlockGameMode::BuildFuel()
{
	const auto* GS = GetGameState<AAirlockGameState>();
	const auto* Layout = GS ? FuelLayouts::Find(GS->CurrentSectionId) : nullptr;
	if (!HasAuthority() || !Layout) { return; }
	FuelModel.Reset(); PublishFuel();
	const auto Spawn = [this]<typename T>(const FFuelFixtureSpec& Spec)
	{
		FActorSpawnParameters Params; Params.Name = Spec.Id; Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (auto* Actor = GetWorld()->SpawnActor<T>(T::StaticClass(), Spec.Transform, Params))
		{
			Actor->Initialize(Spec);
#if WITH_EDITOR
			Actor->SetActorLabel(Spec.Id.ToString(), false); Actor->SetFolderPath(TEXT("Fuel"));
#endif
			Actor->FinishSpawning(Spec.Transform);
		}
	};
	Spawn.operator()<AFuelValve>(Layout->FeedValve);
	Spawn.operator()<ABypassValve>(Layout->BypassValve);
	Spawn.operator()<AFuelIntake>(Layout->Intake);
	if (!GetWorldTimerManager().IsTimerActive(ShipGraphTimer))
	{ GetWorldTimerManager().SetTimer(ShipGraphTimer, this, &AAirlockGameMode::TickShipGraph, .25f, true); }
}
void AAirlockGameMode::PublishFuel()
{
	if (!HasAuthority() || !FuelHasSystem()) { return; }
	auto* GS = GetGameState<AAirlockGameState>();
	FFuelState Mirror = FuelModel.State(); Mirror.Units = FMath::FloorToFloat(Mirror.Units);
	const FFuelState Old = GS->Fuel; const FFuelRates R;
	bool bLog = Old.bFeedLeak != Mirror.bFeedLeak || Old.bBypassOpen != Mirror.bBypassOpen
		|| Old.bInjectorsFouled != Mirror.bInjectorsFouled || Old.bExhausted != Mirror.bExhausted;
	for (float Percent : { R.LowFuelPercent, R.CriticalFuelPercent })
	{ const float Threshold = R.Capacity * Percent / 100.f; bLog |= (Old.Units < Threshold) != (Mirror.Units < Threshold); }
	GS->PublishFuel(Mirror);
	if (bLog) { UE_LOG(LogAirlock, Log, TEXT("Fuel %d%% leak=%d bypass=%s"), FMath::FloorToInt(Mirror.Units), Mirror.bFeedLeak, Mirror.bBypassOpen ? TEXT("open") : TEXT("closed")); }
	if (!Old.bInjectorsFouled && Mirror.bInjectorsFouled) { UE_LOG(LogAirlock, Log, TEXT("Injectors fouled")); }
	if (Old.bInjectorsFouled && !Mirror.bInjectorsFouled) { UE_LOG(LogAirlock, Log, TEXT("Injectors cleared")); }
}
void AAirlockGameMode::TickFuel()
{
	const auto* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !FuelHasSystem() || !GS->bVoyageActive) { return; }
	const auto Before = FuelModel.State();
	if (AirHasAtmosphere() && AtmosphereModel.AddPpm(FuelModel.SootPerSecond() * .25f)) { PublishAtmosphere(); }
	if (FuelModel.Tick(.25f)) { PublishFuel(); }
	// The reserve call is tied to the fuel, not to the 10 s follow-up: whenever the ship first goes short, once.
	if (!bFuelShortSpoken && !bTravelPending && FuelModel.IsShort(GS->GetVoyageSecondsRemaining()))
	{
		bFuelShortSpoken = true;
		SpeakVoyageLine(FText::Format(NSLOCTEXT("Airlock", "FuelReserveGone", "Captain {0} here. Reserve's gone - open the crude bypass and feed me casks, or we dock short."), FText::FromName(ActiveVoyage.Captain)));
	}
	if (FuelModel.State().CasksBurned != Before.CasksBurned)
	{ UE_LOG(LogAirlock, Log, TEXT("Fuel feed %s done: +15"), *Before.BurningCrate.ToString()); }
	if (!Before.bExhausted && FuelModel.State().bExhausted)
	{
		SetVoyageSecondsRemaining(GS->GetVoyageSecondsRemaining() + FFuelRates().LateDockSeconds);
		UE_LOG(LogAirlock, Log, TEXT("Fuel exhausted: docking 120 s late"));
		SpeakAirCaptain(TEXT("FuelExhausted"), NSLOCTEXT("Airlock", "FuelExhausted", "Captain {0} here. We're dry. Coasting in on momentum - we dock late."));
	}
}
bool AAirlockGameMode::FuelOpenLeak()
{
	if (!HasAuthority() || !FuelHasSystem()) { return false; }
	FuelModel.OpenLeak(); PublishFuel(); return true;
}
bool AAirlockGameMode::FuelCloseLeak()
{
	if (!HasAuthority() || !FuelHasSystem() || !FuelModel.CloseLeak()) { return false; }
	PublishFuel(); FuelFollowup(); return true;
}
bool AAirlockGameMode::FuelOpenBypass()
{
	if (!HasAuthority() || !FuelHasSystem() || !FuelModel.OpenBypass()) { return false; }
	PublishFuel(); return true;
}
bool AAirlockGameMode::FuelFeedCask(FName Id)
{
	if (!HasAuthority() || !FuelHasSystem()) { return false; }
	const float Fouling = FuelModel.FoulingOnFeed();
	// Stage fuel first: a refused cargo consume must leave even the burn clock unchanged.
	FFuelSystem Candidate = FuelModel;
	if (!Candidate.FeedCask(Id) || !CargoModel.Consume(Id)) { return false; }
	FuelModel = Candidate;
	ShipDamageModule(TEXT("Engines"), Fouling);
	PublishCargo(); PublishFuel();
	UE_LOG(LogAirlock, Log, TEXT("Fuel feed %s started"), *Id.ToString()); return true;
}
bool AAirlockGameMode::FuelClearInjectors()
{
	if (!HasAuthority() || !FuelHasSystem() || !FuelModel.ClearInjectors()) { return false; }
	PublishFuel(); return true;
}
bool AAirlockGameMode::FuelSet(float Units)
{
	if (!HasAuthority() || !FuelHasSystem() || !FuelModel.SetUnits(Units)) { return false; }
	PublishFuel(); return true;
}
FString AAirlockGameMode::FuelSummary() const { return FuelHasSystem() ? FuelModel.Summary() : TEXT("No fuel system on this section"); }

void AAirlockGameMode::SpeakVoyageLine(const FText& Line)
{
	UE_LOG(LogAirlock, Log, TEXT("Captain %s: %s"), *ActiveVoyage.Captain.ToString(), *Line.ToString());
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{ if (auto* PC = Cast<AAirlockPlayerController>(It->Get())) { PC->ClientCaptainSays(Line); } }
}
void AAirlockGameMode::FireDueSetPieces(double Elapsed)
{
	while (SetPieces.IsValidIndex(NextSetPiece) && SetPieces[NextSetPiece].AtSeconds <= Elapsed)
	{ const auto Entry = SetPieces[NextSetPiece++]; FireSetPiece(Entry, Elapsed); }
}
void AAirlockGameMode::FireSetPiece(const FVoyageSetPiece& Entry, double Elapsed)
{
	const auto* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS || !GS->bVoyageActive || bTravelPending) { return; }
	FuelOpenLeak(); SpeakVoyageLine(Entry.CaptainLine);
	UE_LOG(LogAirlock, Log, TEXT("SetPiece FuelLeak at %.2fs elapsed=%.2fs"), Entry.AtSeconds, Elapsed);
	bFuelFollowupPending = true;
	GetWorldTimerManager().SetTimer(FuelFollowupTimer, this, &AAirlockGameMode::FuelFollowup, 10.f, false);
}
void AAirlockGameMode::FuelFollowup()
{
	if (!bFuelFollowupPending) { return; }
	bFuelFollowupPending = false; GetWorldTimerManager().ClearTimer(FuelFollowupTimer);
	const auto* GS = GetGameState<AAirlockGameState>();
	if (!GS || !GS->bVoyageActive || bTravelPending) { return; }
	const FText Pattern = FuelModel.IsShort(GS->GetVoyageSecondsRemaining())
		? NSLOCTEXT("Airlock", "FuelShort", "Captain {0} here. At this burn we dock short. Sludge is methane - open the crude bypass and feed me casks.")
		: !FuelModel.State().bFeedLeak
		? NSLOCTEXT("Airlock", "FuelShutGood", "Captain {0} here. Feed's shut. We're still good for the refinery.")
		: NSLOCTEXT("Airlock", "FuelStillGood", "Captain {0} here. Shut that feed. We're still good for the refinery - for now.");
	SpeakVoyageLine(FText::Format(Pattern, FText::FromName(ActiveVoyage.Captain)));
}
void AAirlockGameMode::FuelLeakConsole()
{
	const auto* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !FuelHasSystem()) { UE_LOG(LogAirlock, Log, TEXT("No fuel system on this section")); return; }
	if (GS->bVoyageActive && !bTravelPending && SetPieces.IsValidIndex(NextSetPiece))
	{
		const auto Entry = SetPieces[NextSetPiece++];
		FireSetPiece(Entry, ActiveVoyage.DurationSeconds - GS->GetVoyageSecondsRemaining());
		ScheduleNextVoyageEvent();
	}
	else { FuelOpenLeak(); UE_LOG(LogAirlock, Log, TEXT("SetPiece FuelLeak (console)")); }
}

bool AAirlockGameMode::AllowCheats(APlayerController* /*Player*/)
{
	return CVarAirlockDevCheats.GetValueOnGameThread() != 0;
}
