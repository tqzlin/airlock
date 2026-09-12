// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockPlayerController.h"
#include "Airlock.h"
#include "Blueprint/UserWidget.h"
#include "Core/AirlockGameInstance.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockCharacter.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UI/VoyageBoardWidget.h"
#include "UI/CaptainLineWidget.h"
#include "UI/TaskBoardWidget.h"
#include "UI/ControlBoardWidget.h"
#include "Ship/ControlStation.h"
#include "Voyage/VoyageSubsystem.h"
#include "Voyage/IncidentDirectorComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"

AAirlockPlayerController::AAirlockPlayerController()
{
	DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Default"));

	MoveAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;

	InteractAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Interact"));
	InteractAction->ValueType = EInputActionValueType::Boolean;

	// --- Move: WASD -> (X = right, Y = forward) ---
	{
		// W: key value goes into Y (swizzle X->Y).
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::W);
		M.Modifiers.Add(CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("Mod_Move_W_Swizzle")));
	}
	{
		// S: negate, then swizzle into Y.
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::S);
		M.Modifiers.Add(CreateDefaultSubobject<UInputModifierNegate>(TEXT("Mod_Move_S_Negate")));
		M.Modifiers.Add(CreateDefaultSubobject<UInputModifierSwizzleAxis>(TEXT("Mod_Move_S_Swizzle")));
	}
	{
		// A: negative X.
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(MoveAction, EKeys::A);
		M.Modifiers.Add(CreateDefaultSubobject<UInputModifierNegate>(TEXT("Mod_Move_A_Negate")));
	}
	{
		// D: positive X, no modifier.
		DefaultMappingContext->MapKey(MoveAction, EKeys::D);
	}
	{
		// Gamepad left stick maps straight onto the 2D axis.
		DefaultMappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);
	}

	// --- Look: mouse 2D (Y inverted so pushing forward looks up) ---
	{
		FEnhancedActionKeyMapping& M = DefaultMappingContext->MapKey(LookAction, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = CreateDefaultSubobject<UInputModifierNegate>(TEXT("Mod_Look_NegateY"));
		NegateY->bX = false;
		NegateY->bY = true;
		NegateY->bZ = false;
		M.Modifiers.Add(NegateY);
	}
	{
		DefaultMappingContext->MapKey(LookAction, EKeys::Gamepad_Right2D);
	}

	// --- Interact ---
	DefaultMappingContext->MapKey(InteractAction, EKeys::E);
	DefaultMappingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Bottom);
}

void AAirlockPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(DefaultMappingContext, /*Priority*/ 0);
			UE_LOG(LogAirlock, Log, TEXT("AirlockPlayerController: default mapping context registered"));
		}
	}

	// Arriving through an airlock: the card is still up from the previous level; let the new one settle, then fade in.
	if (IsLocalController())
	{
		if (UAirlockGameInstance* GI = Cast<UAirlockGameInstance>(GetGameInstance()); GI && GI->IsTransitionShown())
		{
			GetWorldTimerManager().SetTimer(ArrivalFadeTimer, this, &AAirlockPlayerController::ClientEndTransition, 1.0f, false);
		}
	}
}

void AAirlockPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (TaskBoardWidget)
	{
		TaskBoardWidget->RemoveFromParent();
		TaskBoardWidget = nullptr;
	}
	if (ControlBoardWidget)
	{
		ControlBoardWidget->RemoveFromParent();
		ControlBoardWidget = nullptr;
	}
	if (CaptainWidget)
	{
		CaptainWidget->RemoveFromParent();
		CaptainWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AAirlockPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController() || !GetLocalPlayer()) { return; }
	if (!TaskBoardWidget)
	{
		TaskBoardWidget = CreateWidget<UTaskBoardWidget>(this, UTaskBoardWidget::StaticClass());
		if (TaskBoardWidget) { TaskBoardWidget->AddToViewport(4); }
	}
	if (TaskBoardWidget)
	{
		TaskBoardWidget->UpdateState(GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr);
	}
	if (!ControlBoardWidget)
	{
		ControlBoardWidget = CreateWidget<UControlBoardWidget>(this, UControlBoardWidget::StaticClass());
		if (ControlBoardWidget) { ControlBoardWidget->AddToViewport(6); }
	}
	if (ControlBoardWidget)
	{
		const AAirlockCharacter* Crew = Cast<AAirlockCharacter>(GetPawn());
		ControlBoardWidget->UpdateState(GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr,
			Crew ? Crew->GetSeatedStation() : nullptr);
	}
}

void AAirlockPlayerController::AirlockSit(FName StationId) { ServerSitByStationId(StationId); }
void AAirlockPlayerController::AirlockStand() { ServerStandUpFromConsole(); }
void AAirlockPlayerController::AirlockLook(float Yaw, float Pitch)
{
	SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockLook yaw=%.0f pitch=%.0f"), Yaw, Pitch);
}
void AAirlockPlayerController::ServerSitByStationId_Implementation(FName StationId)
{
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockSit %s"), *StationId.ToString());
	AAirlockCharacter* Crew = Cast<AAirlockCharacter>(GetPawn());
	if (!Crew) { UE_LOG(LogAirlock, Log, TEXT("Seat rejected: no crew pawn")); return; }
	for (TActorIterator<AControlStation> It(GetWorld()); It; ++It)
	{
		if (It->GetStationId() == StationId) { Crew->ServerSitAt(*It); return; }
	}
	UE_LOG(LogAirlock, Log, TEXT("Seat %s rejected: no such station on this section"), *StationId.ToString());
}
void AAirlockPlayerController::ServerStandUpFromConsole_Implementation()
{
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockStand"));
	if (AAirlockCharacter* Crew = Cast<AAirlockCharacter>(GetPawn())) { Crew->ServerStandUp(); }
}

void AAirlockPlayerController::ClientCaptainSays_Implementation(const FText& Line)
{
	if (!IsLocalController())
	{
		return;
	}
	if (!CaptainWidget)
	{
		CaptainWidget = CreateWidget<UCaptainLineWidget>(this, UCaptainLineWidget::StaticClass());
		if (!CaptainWidget)
		{
			return;
		}
		CaptainWidget->AddToViewport(5);
	}
	CaptainWidget->ShowLine(Line);
}

void AAirlockPlayerController::ClientBeginTransition_Implementation(const FText& Title, const FText& Subtitle)
{
	CloseVoyageBoard();
	if (UAirlockGameInstance* GI = Cast<UAirlockGameInstance>(GetGameInstance()))
	{
		GI->ShowTransition(Title, Subtitle);
	}
}

void AAirlockPlayerController::ClientEndTransition_Implementation()
{
	if (UAirlockGameInstance* GI = Cast<UAirlockGameInstance>(GetGameInstance()))
	{
		GI->HideTransition();
	}
}

void AAirlockPlayerController::OpenVoyageBoard()
{
	if (!IsLocalController() || BoardWidget)
	{
		return;
	}

	const AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	const UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	if (!GS || !Voyages)
	{
		UE_LOG(LogAirlock, Warning, TEXT("OpenVoyageBoard: no game state / voyage subsystem yet"));
		return;
	}

	BoardWidget = CreateWidget<UVoyageBoardWidget>(this, UVoyageBoardWidget::StaticClass());
	if (!BoardWidget)
	{
		return;
	}
	BoardWidget->SetOffers(Voyages->GetOfferedVoyages(GS->DaySeed));
	BoardWidget->SetLastVoyageResult(GS->LastVoyageResult, GS->bHasLastVoyageResult);
	BoardWidget->OnVoyageChosen.AddDynamic(this, &AAirlockPlayerController::HandleVoyageChosen);
	BoardWidget->OnClosed.AddDynamic(this, &AAirlockPlayerController::HandleBoardClosed);
	BoardWidget->SetAnchorsInViewport(FAnchors(0.5f, 0.5f));
	BoardWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	BoardWidget->AddToViewport(10);
	if (GS->bVoyageBooked)
	{
		BoardWidget->SetStatus(NSLOCTEXT("Airlock", "VoyageAlreadyBooked", "A voyage is already booked - proceed to Dock B."));
	}

	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(BoardWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	SetShowMouseCursor(true);
	UE_LOG(LogAirlock, Log, TEXT("Voyage board opened (day seed %d)"), GS->DaySeed);
}

void AAirlockPlayerController::CloseVoyageBoard()
{
	if (BoardWidget)
	{
		BoardWidget->RemoveFromParent();
		BoardWidget = nullptr;
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}

void AAirlockPlayerController::HandleVoyageChosen(EVoyageType Type)
{
	if (BoardWidget)
	{
		BoardWidget->SetStatus(NSLOCTEXT("Airlock", "VoyageBooking", "Booking..."));
	}
	ServerChooseVoyage(Type);
}

void AAirlockPlayerController::HandleBoardClosed()
{
	CloseVoyageBoard();
}

void AAirlockPlayerController::ServerChooseVoyage_Implementation(EVoyageType Type)
{
	AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	if (!GS || !Voyages)
	{
		ClientVoyageBooked(FVoyageSpec(), false);
		return;
	}

	const FVoyageSpec Spec = UVoyageSubsystem::MakeVoyageSpec(Type, GS->DaySeed);
	FGuid VoyageId;
	const bool bOk = Voyages->RequestVoyage(Spec, VoyageId);
	if (bOk)
	{
		GS->bVoyageBooked = true;
	}
	ClientVoyageBooked(Spec, bOk);
}

void AAirlockPlayerController::ClientVoyageBooked_Implementation(const FVoyageSpec& Spec, bool bAccepted)
{
	if (!BoardWidget)
	{
		return;
	}
	if (bAccepted)
	{
		BoardWidget->SetStatus(FText::Format(NSLOCTEXT("Airlock", "VoyageBooked", "Booked: {0}. Proceed to Dock B."), UVoyageBoardWidget::DescribeOffer(Spec)));
	}
	else
	{
		BoardWidget->SetStatus(NSLOCTEXT("Airlock", "VoyageRefused", "Booking refused."));
	}
}

void AAirlockPlayerController::AirlockBook(int32 TypeIndex)
{
	const EVoyageType Type = static_cast<EVoyageType>(FMath::Clamp(TypeIndex, 0, 2));
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockBook %d"), TypeIndex);
	ServerChooseVoyage(Type);
}

void AAirlockPlayerController::AirlockWarp(float X, float Y, float Z)
{
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockWarp %.0f %.0f %.0f"), X, Y, Z);
	ServerWarp(FVector(X, Y, Z));
}

void AAirlockPlayerController::AirlockBoard()
{
	OpenVoyageBoard();
}

void AAirlockPlayerController::AirlockWhere()
{
	FVector ViewLoc = FVector::ZeroVector;
	FRotator ViewRot = FRotator::ZeroRotator;
	GetPlayerViewPoint(ViewLoc, ViewRot);
	const APawn* P = GetPawn();
	UE_LOG(LogAirlock, Log, TEXT("Where: map=%s pawn=%s at %s rot %s view %s rot %s"),
		GetWorld() ? *GetWorld()->GetMapName() : TEXT("?"), P ? *P->GetName() : TEXT("none"),
		P ? *P->GetActorLocation().ToString() : TEXT("-"), P ? *P->GetActorRotation().ToString() : TEXT("-"),
		*ViewLoc.ToString(), *ViewRot.ToString());
}

void AAirlockPlayerController::AirlockVoyageClock(float SecondsRemaining)
{
	ServerSetVoyageClock(SecondsRemaining);
}

void AAirlockPlayerController::ServerSetVoyageClock_Implementation(float SecondsRemaining)
{
	if (AAirlockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr)
	{
		GM->SetVoyageSecondsRemaining(SecondsRemaining);
	}
}

void AAirlockPlayerController::ServerWarp_Implementation(FVector Location)
{
	if (APawn* P = GetPawn())
	{
		P->TeleportTo(Location, P->GetActorRotation(), /*bIsATest*/ false, /*bNoCheck*/ true);
	}
}

void AAirlockPlayerController::AirlockFault(FName LineId, FString Fault) { ServerShipFault(LineId, Fault); }
void AAirlockPlayerController::AirlockDamage(FName ModuleId, float Amount) { ServerAdjustShipModule(ModuleId, Amount, false); }
void AAirlockPlayerController::AirlockRepairModule(FName ModuleId, float Amount) { ServerAdjustShipModule(ModuleId, Amount, true); }
void AAirlockPlayerController::AirlockShipStatus() { ServerShipStatus(); }
void AAirlockPlayerController::AirlockHold(FString TargetLabel) { ServerHoldByLabel(TargetLabel); }
void AAirlockPlayerController::AirlockRelease() { ServerReleaseHold(); }

void AAirlockPlayerController::ServerShipFault_Implementation(FName LineId, const FString& Fault)
{
	EShipLineFault Parsed;
	if (Fault.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { Parsed = EShipLineFault::None; }
	else if (Fault.Equals(TEXT("Leak"), ESearchCase::IgnoreCase)) { Parsed = EShipLineFault::Leak; }
	else if (Fault.Equals(TEXT("Short"), ESearchCase::IgnoreCase)) { Parsed = EShipLineFault::Short; }
	else
	{
		UE_LOG(LogAirlock, Log, TEXT("Console: AirlockFault %s %s rejected: expected None, Leak or Short"), *LineId.ToString(), *Fault);
		return;
	}
	AAirlockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	const bool bOk = GM && GM->ShipSetLineFault(LineId, Parsed);
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockFault %s %s accepted=%d"), *LineId.ToString(), *Fault, bOk);
}
void AAirlockPlayerController::ServerAdjustShipModule_Implementation(FName ModuleId, float Amount, bool bRepair)
{
	AAirlockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	const bool bOk = GM && (bRepair ? GM->ShipRepairModule(ModuleId, Amount) : GM->ShipDamageModule(ModuleId, Amount));
	UE_LOG(LogAirlock, Log, TEXT("Console: %s %s %.2f accepted=%d"), bRepair ? TEXT("AirlockRepairModule") : TEXT("AirlockDamage"), *ModuleId.ToString(), Amount, bOk);
}
void AAirlockPlayerController::ServerShipStatus_Implementation()
{
	if (const AAirlockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr)
	{
		UE_LOG(LogAirlock, Log, TEXT("Console: AirlockShipStatus"));
		TArray<FString> Lines;
		GM->ShipSummary().ParseIntoArrayLines(Lines);
		if (Lines.IsEmpty()) { UE_LOG(LogAirlock, Log, TEXT("Ship graph empty")); }
		for (const FString& Line : Lines) { UE_LOG(LogAirlock, Log, TEXT("%s"), *Line); }
	}
}
void AAirlockPlayerController::ServerHoldByLabel_Implementation(const FString& TargetLabel)
{
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockHold %s"), *TargetLabel);
	AAirlockCharacter* Crew = Cast<AAirlockCharacter>(GetPawn());
	if (!Crew) { UE_LOG(LogAirlock, Log, TEXT("Hold rejected: no crew pawn")); return; }
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetActorNameOrLabel() == TargetLabel)
		{
			Crew->ServerBeginHold(*It);
			return;
		}
	}
	UE_LOG(LogAirlock, Log, TEXT("Hold %s rejected: actor not found"), *TargetLabel);
}
void AAirlockPlayerController::ServerReleaseHold_Implementation()
{
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockRelease"));
	if (AAirlockCharacter* Crew = Cast<AAirlockCharacter>(GetPawn())) { Crew->ServerEndHold(); }
}

void AAirlockPlayerController::AirlockIncident(FString Type, FName TargetId, float Magnitude)
{
	ServerIncident(Type, TargetId, Magnitude);
}

void AAirlockPlayerController::AirlockIncidentSchedule() { ServerIncidentSchedule(); }

void AAirlockPlayerController::ServerIncident_Implementation(const FString& Type, FName TargetId, float Magnitude)
{
	EIncidentType Parsed;
	if (Type.Equals(TEXT("CoolantLeak"), ESearchCase::IgnoreCase)) { Parsed = EIncidentType::CoolantLeak; }
	else if (Type.Equals(TEXT("PowerShort"), ESearchCase::IgnoreCase)) { Parsed = EIncidentType::PowerShort; }
	else if (Type.Equals(TEXT("ModuleDamage"), ESearchCase::IgnoreCase)) { Parsed = EIncidentType::ModuleDamage; }
	else if (Type.Equals(TEXT("CargoShift"), ESearchCase::IgnoreCase)) { Parsed = EIncidentType::CargoShift; }
	else
	{
		UE_LOG(LogAirlock, Log, TEXT("Console: AirlockIncident %s rejected: expected CoolantLeak, PowerShort, ModuleDamage or CargoShift"), *Type);
		return;
	}
	AAirlockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	UIncidentDirectorComponent* Director = GM ? GM->GetIncidentDirector() : nullptr;
	if (!Director)
	{
		UE_LOG(LogAirlock, Log, TEXT("Console: AirlockIncident rejected: no active incident director"));
		return;
	}
	const bool bApplied = Director->FireNow(Parsed, TargetId, Magnitude);
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockIncident %s %s %.2f accepted=%d"), *Type, *TargetId.ToString(), Magnitude, bApplied);
}

void AAirlockPlayerController::ServerIncidentSchedule_Implementation()
{
	const AAirlockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	const UIncidentDirectorComponent* Director = GM ? GM->GetIncidentDirector() : nullptr;
	if (Director) { Director->LogSchedule(); }
	else { UE_LOG(LogAirlock, Log, TEXT("Incident schedule empty: no active incident director")); }
}

void AAirlockPlayerController::AirlockCargoStatus() { ServerCargoStatus(); }
void AAirlockPlayerController::AirlockCarry(FName CrateId) { ServerCarryById(CrateId); }
void AAirlockPlayerController::AirlockDrop() { ServerDropCargo(); }
void AAirlockPlayerController::AirlockPlace(FName SlotId) { ServerPlaceById(SlotId); }
void AAirlockPlayerController::AirlockCrateHeat(FName CrateId, float Temperature) { ServerCrateHeat(CrateId, Temperature); }
void AAirlockPlayerController::AirlockTally() { ServerCargoTally(); }

void AAirlockPlayerController::ServerCargoStatus_Implementation()
{
	if (const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr)
	{
		TArray<FString> Lines;
		GM->CargoSummary().ParseIntoArrayLines(Lines);
		for (const auto& Line : Lines) { UE_LOG(LogAirlock, Log, TEXT("%s"), *Line); }
	}
	else { UE_LOG(LogAirlock, Log, TEXT("Cargo status rejected: no game mode")); }
}
void AAirlockPlayerController::ServerCarryById_Implementation(FName CrateId)
{
	auto* Crew = Cast<AAirlockCharacter>(GetPawn());
	if (!Crew) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: no crew pawn")); return; }
	for (TActorIterator<ACargoCrate> It(GetWorld()); It; ++It)
	{
		if (It->GetCargoId() == CrateId) { Crew->ServerPickUp(*It); return; }
	}
	UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: crate %s not found"), *CrateId.ToString());
}
void AAirlockPlayerController::ServerDropCargo_Implementation()
{
	if (auto* Crew = Cast<AAirlockCharacter>(GetPawn())) { Crew->ServerPutDown(); }
	else { UE_LOG(LogAirlock, Log, TEXT("PutDown rejected: no crew pawn")); }
}
void AAirlockPlayerController::ServerPlaceById_Implementation(FName SlotId)
{
	auto* Crew = Cast<AAirlockCharacter>(GetPawn());
	if (!Crew) { UE_LOG(LogAirlock, Log, TEXT("Place rejected: no crew pawn")); return; }
	for (TActorIterator<ACargoSlot> It(GetWorld()); It; ++It)
	{
		if (It->GetSlotId() == SlotId) { Crew->ServerPlace(*It); return; }
	}
	UE_LOG(LogAirlock, Log, TEXT("Place rejected: slot %s not found"), *SlotId.ToString());
}
void AAirlockPlayerController::ServerCrateHeat_Implementation(FName CrateId, float Temperature)
{
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	const bool bOk = GM && GM->CargoSetTemperature(CrateId, Temperature);
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockCrateHeat %s %.2f accepted=%d%s"), *CrateId.ToString(), Temperature, bOk,
		bOk ? TEXT("") : TEXT(" (requires a live volatile crate and finite temperature in 0..100)"));
}
void AAirlockPlayerController::ServerCargoTally_Implementation()
{
	if (const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr)
	{
		UE_LOG(LogAirlock, Log, TEXT("Tally now: %s"), *GM->CargoTally().Summary());
	}
	else { UE_LOG(LogAirlock, Log, TEXT("Tally rejected: no game mode")); }
}
