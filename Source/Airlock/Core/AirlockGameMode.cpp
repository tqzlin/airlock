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
#include "Voyage/FloorSchedule.h"
#include "Ship/RefineryLayouts.h"
#include "Ship/FloorFixture.h"
#include "Ship/Still.h"
#include "HAL/IConsoleManager.h"

/** The one switch behind every Airlock* console helper (M4-1). On wherever an editor is compiled in (PIE, -game from the editor binary); off in every packaged build until an operator sets it (-ExecCmds="airlock.DevCheats 1" or [ConsoleVariables]). */
static TAutoConsoleVariable<int32> CVarAirlockDevCheats(
	TEXT("airlock.DevCheats"),
#if WITH_EDITOR
	1,
#else
	0,
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
		// Occupied means another player's pawn stands there; an unpossessed or dying pawn (the arriving player's own
		// old body during a travel, a spectator, a leftover) does not count.
		APawn* Occupant = nullptr;
		for (TActorIterator<APawn> It(World); It && !Occupant; ++It)
		{
			if (IsValid(*It) && It->GetController() && It->GetController() != NewPlayer
				&& FVector::DistSquaredXY(It->GetActorLocation(), Candidate.GetLocation()) < FMath::Square(ArrivalSpacing * 0.75f)
				&& FMath::Abs(It->GetActorLocation().Z - Candidate.GetLocation().Z) < 200.f)
			{
				Occupant = *It;
			}
		}
		if (Occupant)
		{
			UE_LOG(LogAirlock, Verbose, TEXT("Arrival spawn for %s: %s (%s) occupies the spot %+.0f cm from the start"), NewPlayer ? *NewPlayer->GetName() : TEXT("?"),
				*Occupant->GetName(), *GetNameSafe(Occupant->GetController()), Step * ArrivalSpacing);
			continue;
		}
		// A side step into a wall is skipped quietly, using the same test SpawnActor's adjust-or-refuse uses: the spot
		// must be clear, or clear after a vertical lift only (a map PlayerStart sits at floor level, so the capsule
		// always needs the lift FindTeleportSpot allows); a spot the engine would have to push sideways is a wall,
		// and which way it pushes is a coin toss. The start itself keeps the engine's own behaviour.
		if (Step != 0.f)
		{
			FVector Probe = Candidate.GetLocation();
			const bool bClear = World->FindTeleportSpot(PawnClass->GetDefaultObject<APawn>(), Probe, Candidate.GetRotation().Rotator());
			if (!bClear || FVector::DistSquaredXY(Probe, Candidate.GetLocation()) > FMath::Square(10.f))
			{
				continue;
			}
		}
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
	BuildFloorFixtures();
	BuildRefinery();
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
		// The floor runs on the same clock fields with its own length and Okafor's lines (M5-2).
		const bool bFloor = IsFloorSection();
		if (bFloor) { ActiveVoyage.DurationSeconds = FFloorSchedule::SlotSeconds; CaptainSchedule = FFloorSchedule::Build(); }
		else { CaptainSchedule = FVoyageSchedule::Build(ActiveVoyage); }
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
		if (bFloor)
		{
			UE_LOG(LogAirlock, Log, TEXT("Slot clock started: %.0fs endServerTime=%.3f casks=%d"), ActiveVoyage.DurationSeconds, GS->VoyageEndServerTime, ActiveCargoLayout.Crates.Num());
		}
		else
		{
			UE_LOG(LogAirlock, Log, TEXT("Voyage clock started: captain=%s duration=%.0fs endServerTime=%.3f"),
				*ActiveVoyage.Captain.ToString(), ActiveVoyage.DurationSeconds, GS->VoyageEndServerTime);
		}
		const FShipLayout* Layout = bFloor ? nullptr : ShipLayouts::Find(GS->CurrentSectionId);
		if (Layout)
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
		UE_LOG(LogAirlock, Log, TEXT("%s: %s"), *SpeakerName(), *Line.ToString());
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
	// The ship's clock running out is the refinery dock: the crew lands on the floor (M5-2), not at the Rig.
	if (IsShipSection())
	{
		DockAtFloor();
		return;
	}
	const bool bFloor = IsFloorSection();
	if (UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr)
	{
		RecordVoyageResult();
		Voyages->ClearPendingVoyage();
	}
	const FText Title = bFloor ? NSLOCTEXT("Airlock", "TransitionUndock", "UNDOCKING - HOME TO THE VELL EXTRACTOR")
		: NSLOCTEXT("Airlock", "TransitionToHub", "DOCKING - HALDER SOLAR REFINERY");
	const FText Subtitle = bFloor ? NSLOCTEXT("Airlock", "TransitionUndockSub", "the Rig's board has your pay")
		: NSLOCTEXT("Airlock", "TransitionRefinery", "cargo off, fuel on - then home to the Vell Extractor");
	UE_LOG(LogAirlock, Log, TEXT("%s: travelling to %s (section %s) in %.1fs"), bFloor ? TEXT("Slot over") : TEXT("Voyage clock expired"),
		*HubLevel.GetLongPackageName(), *HubSectionId.ToString(), TravelFadeDelay);
	TravelToSection(HubLevel, HubSectionId, Title, Subtitle);
}

bool AAirlockGameMode::IsShipSection() const
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	return bStartedVoyageClock && GS && GS->CurrentSectionId == ActiveVoyage.ShipClassId;
}

bool AAirlockGameMode::IsFloorSection() const
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	return GS && GS->CurrentSectionId == RefineryLayouts::FloorSectionId();
}

FString AAirlockGameMode::SpeakerName() const
{
	return IsFloorSection() ? FString(FFloorSchedule::Speaker()) : FString::Printf(TEXT("Captain %s"), *ActiveVoyage.Captain.ToString());
}

void AAirlockGameMode::DockAtFloor()
{
	if (bTravelPending || !GetWorld()) { return; }
	RecordShipResult();
	UE_LOG(LogAirlock, Log, TEXT("Docked at the refinery: travelling to %s (section %s) in %.1fs"), *FloorLevel.GetLongPackageName(), *RefineryLayouts::FloorSectionId().ToString(), TravelFadeDelay);
	TravelToSection(FloorLevel, RefineryLayouts::FloorSectionId(), NSLOCTEXT("Airlock", "TransitionToFloor", "DOCKING - HALDER SOLAR REFINERY"),
		NSLOCTEXT("Airlock", "TransitionFloorSub", "cargo off - your slot on the floor starts now"));
}

void AAirlockGameMode::RecordShipResult()
{
	if (!HasAuthority() || !bStartedVoyageClock || bRecordedVoyageResult) { return; }
	if (auto* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr)
	{
		const FVoyageResult Result = CargoTally();
		Voyages->SetShipResult(Result);
		bRecordedVoyageResult = true;
		UE_LOG(LogAirlock, Log, TEXT("Ship result: %s (%d casks to the floor)"), *Result.Summary(), Result.IntactCasks.Num());
	}
}

void AAirlockGameMode::BuildFloorFixtures()
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS) { return; }
	const FRefineryLayout* Layout = RefineryLayouts::Find(GS->CurrentSectionId);
	if (!Layout) { return; }
	for (const FFloorFixtureSpec& Spec : Layout->Fixtures)
	{
		FActorSpawnParameters Params;
		Params.Name = Spec.Id;
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (auto* Fixture = GetWorld()->SpawnActor<AFloorFixture>(Spec.Kind == EFloorFixtureKind::Still ? AStill::StaticClass() : AFloorFixture::StaticClass(), Spec.Transform, Params))
		{
			Fixture->Initialize(Spec);
#if WITH_EDITOR
			Fixture->SetActorLabel(Spec.Id.ToString(), false);
			Fixture->SetFolderPath(TEXT("Floor"));
#endif
			Fixture->FinishSpawning(Spec.Transform);
		}
	}
	UE_LOG(LogAirlock, Log, TEXT("Floor fixtures: %d"), Layout->Fixtures.Num());
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
	TickRefinery();
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
	if (GS->CurrentSectionId == RefineryLayouts::FloorSectionId())
	{
		// The floor's casks are the ship leg's intact ones (M5-2); without a ship result the floor is empty.
		FVoyageResult Ship;
		const bool bHasShip = Voyages && Voyages->GetShipResult(Ship);
		ActiveCargoLayout = CargoLayouts::BuildFloor(bHasShip ? Ship.IntactCasks : TArray<FName>(), bHasShip ? Ship.IntactLightCasks : TArray<FName>());
		if (!bHasShip) { UE_LOG(LogAirlock, Log, TEXT("Floor: no ship result - an empty floor")); }
	}
	else
	{
		ActiveCargoLayout = (Voyages && Voyages->GetPendingVoyage(Pending))
			? CargoLayouts::Build(GS->CurrentSectionId, Pending.CaskCount, Pending.VolatileCount)
			: *CargoLayouts::Find(GS->CurrentSectionId);
	}
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
		// The ship leg's tally waits on the subsystem from the refinery dock (M5-2); a ship leaving through its own
		// airlock never docked, so it tallies here as before.
		FVoyageResult Result;
		if (!Voyages->GetShipResult(Result)) { Result = CargoTally(); }
		if (IsFloorSection())
		{
			// M5-3a: the floor's fields and the base fuel come from the refinery model's tally (the flat per-cask fuel
			// of M3-7 now only applies to a ship that left through its own airlock without docking).
			const FFloorResult Floor = RefineryTally();
			Result.bHasFloor = true;
			Result.FloorCasksTotal = Floor.CasksTotal;
			Result.FloorCasksRefined = Floor.CasksRefined;
			Result.FloorCasksUnrefined = Floor.CasksUnrefined;
			Result.FloorDieselL = FMath::FloorToInt(Floor.DieselL);
			Result.FloorPay = Floor.Pay;
			Result.Pay += Floor.Pay;
			Result.FuelCredit = FMath::FloorToInt(Floor.RefuelUnits);
			Result.FuelSurplus = Floor.BaseFuelTonnes;
			UE_LOG(LogAirlock, Log, TEXT("Floor result: %s"), *Floor.Summary());
		}
		Voyages->SetLastVoyageResult(Result);
		Voyages->ClearShipResult();
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
	UE_LOG(LogAirlock, Log, TEXT("%s: %s"), *SpeakerName(), *Line.ToString());
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
	SpeakAirCaptain(TEXT("Blackout"), NSLOCTEXT("Airlock", "AirBlackout", "Captain {0} here. {1} is down. Get them to air."), Pawn);
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
	UE_LOG(LogAirlock, Log, TEXT("%s: %s"), *SpeakerName(), *Line.ToString());
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

// --- The refinery model on the floor (M5-3a) ---
namespace
{
	int32 RefineryKnobValue(const FStillState& Still, ERefineryKnob Knob)
	{
		switch (Knob)
		{
		case ERefineryKnob::Heat: return Still.HeatSteps;
		case ERefineryKnob::Feed: return Still.FeedSteps;
		case ERefineryKnob::Vent: return Still.VentSteps;
		default: return 0;
		}
	}
}

bool AAirlockGameMode::RefineryHasFloor() const
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	return GS && GS->RefineryHasFloor();
}

void AAirlockGameMode::BuildRefinery()
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS) { return; }
	const FRefineryLayout* Layout = RefineryLayouts::Find(GS->CurrentSectionId);
	if (!Layout) { return; }
	// The manifest is the cargo model's (the ship result's intact casks, M5-2); a light cask is the ship's volatile one.
	TArray<FRefineryCask> Casks;
	for (const FCargoCrateState& Crate : CargoModel.Crates())
	{
		FRefineryCask Cask; Cask.Id = Crate.Id; Cask.bLight = Crate.bVolatile; Casks.Add(Cask);
	}
	RefineryModel.Build(RefineryLayouts::StillCount(*Layout), Casks);
	bFirstBatchSpoken = false;
	PublishRefinery();
	UE_LOG(LogAirlock, Log, TEXT("Refinery: %d stills, %d casks delivered"), RefineryModel.State().Stills.Num(), Casks.Num());
}

void AAirlockGameMode::TickRefinery()
{
	const AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS || !GS->bVoyageActive || !RefineryHasFloor()) { return; }
	const float RoomPpm = AirHasAtmosphere() ? AtmosphereModel.State().Ppm : 0.f;
	float Added = 0.f;
	TArray<FRefineryEvent> Events;
	const bool bChanged = RefineryModel.Tick(0.25f, RoomPpm, Added, Events);
	// The stills fume into the same air the casks puff into; the vent's excess and the gas cut arrive per tick.
	if (Added > 0.f && AirHasAtmosphere() && AtmosphereModel.AddPpm(Added)) { PublishAtmosphere(); }
	ApplyRefineryEvents(Events);
	if (bChanged || !Events.IsEmpty()) { PublishRefinery(); }
}

void AAirlockGameMode::PublishRefinery()
{
	AAirlockGameState* GS = GetGameState<AAirlockGameState>();
	if (!HasAuthority() || !GS || !RefineryHasFloor()) { return; }
	const FRefineryRates& Rates = RefineryModel.Rates;
	FRefineryState Mirror = RefineryModel.State();
	auto Tens = [](float Value) { return FMath::FloorToFloat(Value / 10.f) * 10.f; };
	for (FStillState& S : Mirror.Stills)
	{
		S.TempK = FMath::FloorToFloat(S.TempK);
		S.PressureBar = FMath::RoundToFloat(S.PressureBar * 20.f) / 20.f;
		S.Calibration = FMath::FloorToFloat(S.Calibration * 100.f) / 100.f;
		S.Coke = FMath::FloorToFloat(S.Coke);
		S.Condition = FMath::FloorToFloat(S.Condition);
		S.ChargeL = Tens(S.ChargeL);
		S.BatchDieselL = Tens(S.BatchDieselL); S.BatchGasL = Tens(S.BatchGasL); S.BatchTarL = Tens(S.BatchTarL);
		S.FloodSeconds = FMath::FloorToFloat(S.FloodSeconds);
	}
	Mirror.TankDieselL = FMath::FloorToFloat(Mirror.TankDieselL);
	Mirror.TotalGasL = FMath::FloorToFloat(Mirror.TotalGasL);
	Mirror.TotalTarL = FMath::FloorToFloat(Mirror.TotalTarL);
	Mirror.SpilledL = FMath::FloorToFloat(Mirror.SpilledL);
	for (const FStillState& S : Mirror.Stills)
	{
		const FStillState* Old = GS->Refinery.Stills.FindByPredicate([&S](const FStillState& O) { return O.Id == S.Id; });
		const FString Id = S.Id.ToString();
		if (!Old || Old->Status != S.Status || Old->CurrentCask != S.CurrentCask)
		{
			if (S.CurrentCask.IsNone()) { UE_LOG(LogAirlock, Log, TEXT("Still %s %s"), *Id, RefineryLayouts::StatusName(S.Status)); }
			else { UE_LOG(LogAirlock, Log, TEXT("Still %s %s cask=%s"), *Id, RefineryLayouts::StatusName(S.Status), *S.CurrentCask.ToString()); }
		}
		const bool bWasHigh = Old && Old->PressureBar >= Rates.WarnBar;
		if ((S.PressureBar >= Rates.WarnBar) != bWasHigh)
		{
			UE_LOG(LogAirlock, Log, TEXT("Still %s pressure %.2f bar - %s"), *Id, S.PressureBar, bWasHigh ? TEXT("back under the warning band") : TEXT("warning band"));
			if (!bWasHigh) { SpeakFloorLine(TEXT("Pressure"), FText::Format(NSLOCTEXT("Airlock", "OkaforPressure", "Pressure on {0} - vent it before the relief goes."), FText::FromString(Id.Replace(TEXT("_"), TEXT(" "))))); }
		}
		if (Old && Old->bFouled != S.bFouled) { UE_LOG(LogAirlock, Log, TEXT("Still %s %s"), *Id, S.bFouled ? TEXT("fouled") : TEXT("clean")); }
		const bool bWasDrifted = Old && Old->Calibration < Rates.CalWarn;
		if ((S.Calibration < Rates.CalWarn) != bWasDrifted)
		{
			UE_LOG(LogAirlock, Log, TEXT("Still %s calibration %d%% - %s"), *Id, FMath::FloorToInt(S.Calibration * 100.f), bWasDrifted ? TEXT("calibrated") : TEXT("drifted"));
			if (!bWasDrifted) { SpeakFloorLine(TEXT("Drift"), FText::Format(NSLOCTEXT("Airlock", "OkaforDrift", "{0}'s trap has drifted - you're losing the cut. Calibrate it."), FText::FromString(Id.Replace(TEXT("_"), TEXT(" "))))); }
		}
	}
	if (FMath::FloorToInt(GS->Refinery.TankDieselL / 100.f) != FMath::FloorToInt(Mirror.TankDieselL / 100.f))
	{
		UE_LOG(LogAirlock, Log, TEXT("Tank %d L diesel"), FMath::FloorToInt(Mirror.TankDieselL));
	}
	GS->PublishRefinery(Mirror);
}

void AAirlockGameMode::ApplyRefineryEvents(const TArray<FRefineryEvent>& Events)
{
	for (const FRefineryEvent& E : Events)
	{
		const FString Still = E.StillId.ToString();
		const FString Cask = E.CaskId.ToString();
		const FText Name = FText::FromString(Still.Replace(TEXT("_"), TEXT(" ")));
		switch (E.Type)
		{
		case ERefineryEvent::BatchDone:
			UE_LOG(LogAirlock, Log, TEXT("Refinery BatchDone %s %s diesel=%d gas=%d tar=%d"), *Still, *Cask,
				FMath::FloorToInt(E.DieselL), FMath::FloorToInt(E.GasL), FMath::FloorToInt(E.TarL));
			if (!bFirstBatchSpoken)
			{
				bFirstBatchSpoken = true;
				SpeakFloorLine(TEXT("FirstBatch"), NSLOCTEXT("Airlock", "OkaforFirstBatch", "That's diesel in the tank. Keep them coming."));
			}
			break;
		case ERefineryEvent::Spill:
			UE_LOG(LogAirlock, Log, TEXT("Refinery Spill %s %s litres=%d"), *Still, *Cask, FMath::FloorToInt(E.Litres));
			SpeakFloorLine(TEXT("Spill"), FText::Format(NSLOCTEXT("Airlock", "OkaforSpill", "Flood on {0} - crude over the lip. Feed down, and mind your footing, it's slick."), Name));
			break;
		case ERefineryEvent::Pop:
			UE_LOG(LogAirlock, Log, TEXT("Refinery Pop %s %s litres=%d"), *Still, *Cask, FMath::FloorToInt(E.Litres));
			SpeakFloorLine(TEXT("Pop"), FText::Format(NSLOCTEXT("Airlock", "OkaforPop", "{0}'s relief just blew. Vent it wide, reset the trap, and keep your face out of that cloud."), Name));
			break;
		case ERefineryEvent::Flash:
			UE_LOG(LogAirlock, Log, TEXT("Refinery Flash %s %s"), *Still, *Cask);
			SpeakFloorLine(TEXT("Flash"), FText::Format(NSLOCTEXT("Airlock", "OkaforFlash", "Flash on {0}! Kill the heat and purge it. Anyone who breathed that sits down before they fall down."), Name));
			break;
		case ERefineryEvent::Fouled:
			UE_LOG(LogAirlock, Log, TEXT("Refinery Fouled %s"), *Still);
			SpeakFloorLine(TEXT("Fouled"), FText::Format(NSLOCTEXT("Airlock", "OkaforFouled", "{0}'s coking up - you're getting half the cut. Scrape it when the hopper's empty."), Name));
			break;
		case ERefineryEvent::Failed:
			UE_LOG(LogAirlock, Log, TEXT("Refinery Failed %s"), *Still);
			SpeakFloorLine(TEXT("Failed"), FText::Format(NSLOCTEXT("Airlock", "OkaforFailed", "{0}'s dead - coked solid. Get it repaired or write it off."), Name));
			break;
		default: break;
		}
	}
}

void AAirlockGameMode::SpeakFloorLine(FName Cue, const FText& Line)
{
	if (!HasAuthority() || !GetWorld()) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const double* Last = AirCaptainLastSpoken.Find(Cue);
	if (Last && Now - *Last < 20.0) { return; }
	AirCaptainLastSpoken.Add(Cue, Now);
	SpeakVoyageLine(Line);
}

bool AAirlockGameMode::RefineryLoadHopper(FName Still, FName Cask)
{
	if (!HasAuthority()) { return false; }
	FString Reason;
	const FCargoCrateState* Crate = CargoModel.FindCrate(Cask);
	const FStillState* S = RefineryModel.FindStill(Still);
	if (!RefineryHasFloor()) { Reason = TEXT("no stills on this section"); }
	else if (!S) { Reason = TEXT("no such still"); }
	else if (!Crate) { Reason = TEXT("no such cask"); }
	else if (Crate->State != ECargoState::Loose && Crate->State != ECargoState::Carried) { Reason = FString::Printf(TEXT("cask is %s"), FCargoModel::StateName(Crate->State)); }
	else if (S->Hopper.Num() >= RefineryModel.Rates.HopperCasks) { Reason = TEXT("hopper full"); }
	else if (!RefineryModel.LoadHopper(Still, Cask)) { Reason = TEXT("model refused"); }
	if (!Reason.IsEmpty())
	{
		UE_LOG(LogAirlock, Log, TEXT("Refinery load rejected: %s (%s into %s)"), *Reason, *Cask.ToString(), *Still.ToString());
		return false;
	}
	if (!CargoModel.Load(Cask)) { UE_LOG(LogAirlock, Warning, TEXT("Refinery load: the cargo model refused %s after the hopper took it"), *Cask.ToString()); }
	PublishCargo();
	PublishRefinery();
	UE_LOG(LogAirlock, Log, TEXT("Refinery %s loaded %s (hopper %d/%d)"), *Still.ToString(), *Cask.ToString(), S->Hopper.Num(), RefineryModel.Rates.HopperCasks);
	return true;
}

bool AAirlockGameMode::RefinerySetKnob(FName Still, ERefineryKnob Knob, int32 Steps)
{
	if (!HasAuthority() || !RefineryHasFloor() || !RefineryModel.SetKnob(Still, Knob, Steps))
	{
		UE_LOG(LogAirlock, Log, TEXT("Refinery %s %s rejected"), *Still.ToString(), RefineryLayouts::KnobName(Knob));
		return false;
	}
	PublishRefinery();
	UE_LOG(LogAirlock, Log, TEXT("Refinery %s %s %d/%d"), *Still.ToString(), RefineryLayouts::KnobName(Knob),
		RefineryKnobValue(*RefineryModel.FindStill(Still), Knob), RefineryModel.Rates.KnobSteps);
	return true;
}

bool AAirlockGameMode::RefineryStepKnob(FName Still, ERefineryKnob Knob, int32 Delta)
{
	if (!HasAuthority() || !RefineryHasFloor() || !RefineryModel.StepKnob(Still, Knob, Delta))
	{
		UE_LOG(LogAirlock, Log, TEXT("Refinery %s %s rejected"), *Still.ToString(), RefineryLayouts::KnobName(Knob));
		return false;
	}
	PublishRefinery();
	UE_LOG(LogAirlock, Log, TEXT("Refinery %s %s %d/%d"), *Still.ToString(), RefineryLayouts::KnobName(Knob),
		RefineryKnobValue(*RefineryModel.FindStill(Still), Knob), RefineryModel.Rates.KnobSteps);
	return true;
}

bool AAirlockGameMode::RefineryLever(FName Still, ERefineryLever Lever)
{
	bool bOk = HasAuthority() && RefineryHasFloor();
	if (bOk)
	{
		switch (Lever)
		{
		case ERefineryLever::Reset: bOk = RefineryModel.ClearPop(Still); break;
		case ERefineryLever::Purge: bOk = RefineryModel.Purge(Still); break;
		case ERefineryLever::Scrape: bOk = RefineryModel.Scrape(Still); break;
		case ERefineryLever::Repair: bOk = RefineryModel.Repair(Still); break;
		case ERefineryLever::Calibrate: bOk = RefineryModel.Calibrate(Still); break;
		default: bOk = false; break;
		}
	}
	if (!bOk)
	{
		const FStillState* S = RefineryModel.FindStill(Still);
		UE_LOG(LogAirlock, Log, TEXT("Refinery %s %s rejected: %s"), *Still.ToString(), RefineryLayouts::LeverName(Lever),
			!RefineryHasFloor() ? TEXT("no stills on this section") : S ? RefineryLayouts::StatusName(S->Status) : TEXT("no such still"));
		return false;
	}
	PublishRefinery();
	UE_LOG(LogAirlock, Log, TEXT("Refinery %s %s"), *Still.ToString(), RefineryLayouts::LeverDone(Lever));
	return true;
}

FFloorResult AAirlockGameMode::RefineryTally() const
{
	float Missing = 0.f;
	FVoyageResult Ship;
	const UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	if (Voyages && Voyages->GetShipResult(Ship) && Ship.FuelUnitsAtDock >= 0.f)
	{
		Missing = FMath::Max(0.f, FFuelRates().Capacity - Ship.FuelUnitsAtDock);
	}
	return RefineryModel.Tally(BlackoutCount, Missing);
}

FString AAirlockGameMode::RefinerySummary() const
{
	return RefineryHasFloor() ? RefineryModel.Summary() : FString(TEXT("No stills on this section"));
}

FString AAirlockGameMode::RefineryStillReadout(FName Still) const { return RefineryModel.StillReadout(Still); }
