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
#include "Ship/FuelIntake.h"
#include "Ship/StillControl.h"
#include "Ship/RefineryLayouts.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerState.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Widgets/SViewport.h"

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
	if (DeltaTime > 0.f)
	{
		SmoothedFrameSeconds = SmoothedFrameSeconds <= 0.f ? DeltaTime
			: FMath::Lerp(SmoothedFrameSeconds, DeltaTime, FMath::Clamp(DeltaTime / 2.f, 0.f, 1.f));
	}
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
	if (!CheatAllowed(TEXT("AirlockSit"))) { return; }
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
	if (!CheatAllowed(TEXT("AirlockStand"))) { return; }
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
	BoardWidget->SetBaseFuel(GS->BaseFuelTonnes, GS->RunsCompleted, GS->bHasLastVoyageResult ? GS->LastVoyageResult.FuelSurplus : 0, GS->bHasLastVoyageResult);
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

void AAirlockPlayerController::AirlockFps()
{
	const float Seconds = SmoothedFrameSeconds > 0.f ? SmoothedFrameSeconds : static_cast<float>(FApp::GetDeltaTime());
	UE_LOG(LogAirlock, Log, TEXT("Fps: %.1f fps, %.1f ms/frame, map=%s"),
		Seconds > 0.f ? 1.f / Seconds : 0.f, Seconds * 1000.f, GetWorld() ? *GetWorld()->GetMapName() : TEXT("?"));
}

bool AAirlockPlayerController::CheatAllowed(const TCHAR* Helper)
{
	AGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	if (GM && GM->AllowCheats(this)) { return true; }
	UE_LOG(LogAirlock, Log, TEXT("Cheat %s refused for %s: %s"), Helper,
		PlayerState ? *PlayerState->GetPlayerName() : *GetName(), GM ? TEXT("airlock.DevCheats is 0") : TEXT("no game mode"));
	return false;
}

void AAirlockPlayerController::AirlockCapture(FString Label)
{
#if UE_BUILD_SHIPPING
	UE_LOG(LogAirlock, Log, TEXT("Capture %s failed: not available in Shipping"), *Label);
#else
	Label = FPaths::MakeValidFileName(Label);
	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
	if (Label.IsEmpty() || !Widget.IsValid() || !FSlateApplication::IsInitialized())
	{
		UE_LOG(LogAirlock, Log, TEXT("Capture %s failed: no game viewport on this instance"), *Label);
		return;
	}
	TArray<FColor> Pixels;
	FIntVector Size(0, 0, 0);
	if (!FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size) || Size.X <= 0 || Size.Y <= 0 || Pixels.Num() < Size.X * Size.Y)
	{
		UE_LOG(LogAirlock, Log, TEXT("Capture %s failed: the viewport did not render"), *Label);
		return;
	}
	for (FColor& Pixel : Pixels) { Pixel.A = 255; }
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AgentCaptures"), Label + TEXT(".png")));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), Size.X, Size.Y)))
	{
		UE_LOG(LogAirlock, Log, TEXT("Capture %s saved %dx%d: %s"), *Label, Size.X, Size.Y, *Path);
	}
	else
	{
		UE_LOG(LogAirlock, Log, TEXT("Capture %s failed: could not write %s"), *Label, *Path);
	}
#endif
}

void AAirlockPlayerController::AirlockVoyageClock(float SecondsRemaining)
{
	ServerSetVoyageClock(SecondsRemaining);
}

void AAirlockPlayerController::ServerSetVoyageClock_Implementation(float SecondsRemaining)
{
	if (!CheatAllowed(TEXT("AirlockVoyageClock"))) { return; }
	if (AAirlockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr)
	{
		GM->SetVoyageSecondsRemaining(SecondsRemaining);
	}
}

void AAirlockPlayerController::ServerWarp_Implementation(FVector Location)
{
	if (!CheatAllowed(TEXT("AirlockWarp"))) { return; }
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
void AAirlockPlayerController::AirlockPress(FString TargetLabel) { ServerPress(TargetLabel); }
void AAirlockPlayerController::AirlockRelease() { ServerReleaseHold(); }

void AAirlockPlayerController::ServerShipFault_Implementation(FName LineId, const FString& Fault)
{
	if (!CheatAllowed(TEXT("AirlockFault"))) { return; }
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
	if (!CheatAllowed(bRepair ? TEXT("AirlockRepairModule") : TEXT("AirlockDamage"))) { return; }
	AAirlockGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	const bool bOk = GM && (bRepair ? GM->ShipRepairModule(ModuleId, Amount) : GM->ShipDamageModule(ModuleId, Amount));
	UE_LOG(LogAirlock, Log, TEXT("Console: %s %s %.2f accepted=%d"), bRepair ? TEXT("AirlockRepairModule") : TEXT("AirlockDamage"), *ModuleId.ToString(), Amount, bOk);
}
void AAirlockPlayerController::ServerShipStatus_Implementation()
{
	if (!CheatAllowed(TEXT("AirlockShipStatus"))) { return; }
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
	if (!CheatAllowed(TEXT("AirlockHold"))) { return; }
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
	if (!CheatAllowed(TEXT("AirlockRelease"))) { return; }
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockRelease"));
	if (AAirlockCharacter* Crew = Cast<AAirlockCharacter>(GetPawn())) { Crew->ServerEndHold(); }
}

void AAirlockPlayerController::ServerPress_Implementation(const FString& TargetLabel)
{
	if (!CheatAllowed(TEXT("AirlockPress"))) { return; }
	auto* Crew = Cast<AAirlockCharacter>(GetPawn());
	if (!Crew) { UE_LOG(LogAirlock, Log, TEXT("Press %s rejected: no crew pawn"), *TargetLabel); return; }
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetActorNameOrLabel() != TargetLabel) { continue; }
		AActor* Target = *It;
		if (!Crew->IsInHoldRange(Target)) { UE_LOG(LogAirlock, Log, TEXT("Press %s rejected: out of reach"), *TargetLabel); return; }
		if (Crew->bBlackedOut) { UE_LOG(LogAirlock, Log, TEXT("Press %s rejected: blacked out"), *TargetLabel); return; }
		if (Crew->IsSeated()) { Crew->ServerStandUp(); return; }
		if (Crew->CarriedCrate)
		{
			if (auto* Slot = Cast<ACargoSlot>(Target)) { Crew->ServerPlace(Slot); }
			else if (auto* Intake = Cast<AFuelIntake>(Target)) { Crew->ServerFeedCask(Intake); }
			else if (auto* Control = Cast<AStillControl>(Target); Control && Control->GetControlKind() == EStillControl::Hopper) { Crew->ServerLoadStill(Control); }
			else { Crew->ServerPutDown(); }
		}
		else if (Target->Implements<UInteractable>())
		{
			if (IInteractable::Execute_GetInteractHoldSeconds(Target) > 0.f) { Crew->ServerBeginHold(Target); Crew->ServerEndHold(); }
			else { IInteractable::Execute_Interact(Target, Crew); }
		}
		return;
	}
	UE_LOG(LogAirlock, Log, TEXT("Press %s rejected: actor not found"), *TargetLabel);
}

void AAirlockPlayerController::AirlockIncident(FString Type, FName TargetId, float Magnitude)
{
	ServerIncident(Type, TargetId, Magnitude);
}

void AAirlockPlayerController::AirlockIncidentSchedule() { ServerIncidentSchedule(); }

void AAirlockPlayerController::ServerIncident_Implementation(const FString& Type, FName TargetId, float Magnitude)
{
	if (!CheatAllowed(TEXT("AirlockIncident"))) { return; }
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
	if (!CheatAllowed(TEXT("AirlockIncidentSchedule"))) { return; }
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
	if (!CheatAllowed(TEXT("AirlockCargoStatus"))) { return; }
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
	if (!CheatAllowed(TEXT("AirlockCarry"))) { return; }
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
	if (!CheatAllowed(TEXT("AirlockDrop"))) { return; }
	if (auto* Crew = Cast<AAirlockCharacter>(GetPawn())) { Crew->ServerPutDown(); }
	else { UE_LOG(LogAirlock, Log, TEXT("PutDown rejected: no crew pawn")); }
}
void AAirlockPlayerController::ServerPlaceById_Implementation(FName SlotId)
{
	if (!CheatAllowed(TEXT("AirlockPlace"))) { return; }
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
	if (!CheatAllowed(TEXT("AirlockCrateHeat"))) { return; }
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	const bool bOk = GM && GM->CargoSetTemperature(CrateId, Temperature);
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockCrateHeat %s %.2f accepted=%d%s"), *CrateId.ToString(), Temperature, bOk,
		bOk ? TEXT("") : TEXT(" (requires a live volatile crate and finite temperature in 0..100)"));
}
void AAirlockPlayerController::ServerCargoTally_Implementation()
{
	if (!CheatAllowed(TEXT("AirlockTally"))) { return; }
	if (const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr)
	{
		UE_LOG(LogAirlock, Log, TEXT("Tally now: %s"), *GM->CargoTally().Summary());
		if (GM->RefineryHasFloor()) { UE_LOG(LogAirlock, Log, TEXT("Floor tally now: %s"), *GM->RefineryTally().Summary()); }
	}
	else { UE_LOG(LogAirlock, Log, TEXT("Tally rejected: no game mode")); }
}

void AAirlockPlayerController::AirlockBaseFuel() { ServerBaseFuel(0, 0); }
void AAirlockPlayerController::AirlockBaseFuelSet(int32 Tonnes) { ServerBaseFuel(1, Tonnes); }
void AAirlockPlayerController::AirlockSaveReset() { ServerBaseFuel(2, 0); }
void AAirlockPlayerController::ServerBaseFuel_Implementation(int32 Mode, int32 Tonnes)
{
	if (!CheatAllowed(Mode == 2 ? TEXT("AirlockSaveReset") : Mode == 1 ? TEXT("AirlockBaseFuelSet") : TEXT("AirlockBaseFuel"))) { return; }
	UVoyageSubsystem* Voyages = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVoyageSubsystem>() : nullptr;
	if (!Voyages) { UE_LOG(LogAirlock, Log, TEXT("Base fuel rejected: no voyage subsystem")); return; }
	if (Mode == 1) { Voyages->SetBaseFuelTonnes(Tonnes); }
	else if (Mode == 2) { Voyages->ResetSave(); }
	if (AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr)
	{
		GS->BaseFuelTonnes = Voyages->GetBaseFuelTonnes(); GS->RunsCompleted = Voyages->GetRunsCompleted(); GS->ForceNetUpdate();
	}
	UE_LOG(LogAirlock, Log, TEXT("Base fuel %d t, runs %d"), Voyages->GetBaseFuelTonnes(), Voyages->GetRunsCompleted());
}
void AAirlockPlayerController::AirlockAir() { ServerAirStatus(); }
void AAirlockPlayerController::AirlockScrub(FString Mode) { ServerScrubFromConsole(Mode); }
void AAirlockPlayerController::AirlockAirSet(float Ppm) { ServerAirSet(Ppm); }
void AAirlockPlayerController::AirlockExpose(float Percent) { ServerExpose(Percent); }
void AAirlockPlayerController::ServerAirStatus_Implementation()
{
	if (!CheatAllowed(TEXT("AirlockAir"))) { return; }
	const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	if (!GM) { UE_LOG(LogAirlock, Log, TEXT("Air status rejected: no game mode")); return; }
	UE_LOG(LogAirlock, Log, TEXT("%s"), *GM->AirSummary());
	for (TActorIterator<AAirlockCharacter> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogAirlock, Log, TEXT("Crew %s exposure=%d%% blackedOut=%d"), *It->GetName(), FMath::FloorToInt(It->Exposure), It->bBlackedOut);
	}
}
void AAirlockPlayerController::ServerScrubFromConsole_Implementation(const FString& Mode)
{
	if (!CheatAllowed(TEXT("AirlockScrub"))) { return; }
	EScrubMode Parsed;
	if (Mode.Equals(TEXT("Off"), ESearchCase::IgnoreCase)) { Parsed = EScrubMode::Off; }
	else if (Mode.Equals(TEXT("Normal"), ESearchCase::IgnoreCase)) { Parsed = EScrubMode::Normal; }
	else if (Mode.Equals(TEXT("Hazard"), ESearchCase::IgnoreCase)) { Parsed = EScrubMode::Hazard; }
	else { UE_LOG(LogAirlock, Log, TEXT("Scrub rejected: unknown mode %s (Off|Normal|Hazard)"), *Mode); return; }
	if (auto* Crew = Cast<AAirlockCharacter>(GetPawn())) { Crew->SetScrubModeServer(Parsed, false); }
	else { UE_LOG(LogAirlock, Log, TEXT("Scrub rejected: no crew pawn")); }
}
void AAirlockPlayerController::ServerAirSet_Implementation(float Ppm)
{
	if (!CheatAllowed(TEXT("AirlockAirSet"))) { return; }
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	if (!GM || !GM->AirHasAtmosphere()) { UE_LOG(LogAirlock, Log, TEXT("Air set rejected: No atmosphere on this section")); return; }
	const bool bOk = GM->AirSetPpm(Ppm);
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockAirSet %.2f accepted=%d%s"), Ppm, bOk,
		bOk ? TEXT("") : TEXT(" (unchanged or requires finite ppm in 0..500)"));
}
void AAirlockPlayerController::ServerExpose_Implementation(float Percent)
{
	if (!CheatAllowed(TEXT("AirlockExpose"))) { return; }
	auto* Crew = Cast<AAirlockCharacter>(GetPawn());
	const bool bOk = Crew && Crew->SetExposureServer(Percent);
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockExpose %.2f accepted=%d%s"), Percent, bOk,
		bOk ? TEXT("") : TEXT(" (requires a crew pawn and finite exposure in 0..100)"));
}

void AAirlockPlayerController::AirlockFuel() { ServerFuelStatus(); }
void AAirlockPlayerController::AirlockFuelSet(float Units) { ServerFuelSet(Units); }
void AAirlockPlayerController::AirlockFuelLeak() { ServerFuelLeak(); }
void AAirlockPlayerController::AirlockBypass() { ServerBypassFromConsole(); }
void AAirlockPlayerController::AirlockFeed(FName CrateId) { ServerFeedById(CrateId); }
void AAirlockPlayerController::ServerFuelStatus_Implementation()
{
	if (!CheatAllowed(TEXT("AirlockFuel"))) { return; }
	if (const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr)
	{ UE_LOG(LogAirlock, Log, TEXT("%s"), *GM->FuelSummary()); }
	else { UE_LOG(LogAirlock, Log, TEXT("Fuel status rejected: no game mode")); }
}
void AAirlockPlayerController::ServerFuelSet_Implementation(float Units)
{
	if (!CheatAllowed(TEXT("AirlockFuelSet"))) { return; }
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	if (!GM || !GM->FuelHasSystem()) { UE_LOG(LogAirlock, Log, TEXT("Fuel set rejected: No fuel system on this section")); return; }
	const bool bAccepted = GM->FuelSet(Units);
	UE_LOG(LogAirlock, Log, TEXT("Console: AirlockFuelSet %.2f accepted=%d"), Units, bAccepted);
	UE_LOG(LogAirlock, Log, TEXT("%s"), *GM->FuelSummary());
}
void AAirlockPlayerController::ServerFuelLeak_Implementation()
{
	if (!CheatAllowed(TEXT("AirlockFuelLeak"))) { return; }
	if (auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr) { GM->FuelLeakConsole(); }
	else { UE_LOG(LogAirlock, Log, TEXT("Fuel leak rejected: no game mode")); }
}
void AAirlockPlayerController::ServerBypassFromConsole_Implementation()
{
	if (!CheatAllowed(TEXT("AirlockBypass"))) { return; }
	if (auto* Crew = Cast<AAirlockCharacter>(GetPawn())) { Crew->OpenBypassServer(nullptr, false); }
	else { UE_LOG(LogAirlock, Log, TEXT("Bypass rejected: no crew pawn")); }
}
void AAirlockPlayerController::ServerFeedById_Implementation(FName CrateId)
{
	if (!CheatAllowed(TEXT("AirlockFeed"))) { return; }
	auto* Crew = Cast<AAirlockCharacter>(GetPawn());
	if (!Crew) { UE_LOG(LogAirlock, Log, TEXT("Feed rejected: no crew pawn")); return; }
	if (!Crew->CarriedCrate || Crew->CarriedCrate->GetCargoId() != CrateId)
	{
		UE_LOG(LogAirlock, Log, TEXT("Feed rejected: carrying %s"), Crew->CarriedCrate ? *Crew->CarriedCrate->GetCargoId().ToString() : TEXT("none")); return;
	}
	for (TActorIterator<AFuelIntake> It(GetWorld()); It; ++It) { Crew->ServerFeedCask(*It); return; }
	UE_LOG(LogAirlock, Log, TEXT("Feed rejected: no intake on this section"));
}

// --- The refinery floor's stills (M5-3a) ---
void AAirlockPlayerController::AirlockRefinery() { ServerRefineryStatus(); }
void AAirlockPlayerController::AirlockFloor() { ServerFloorStatus(); }
void AAirlockPlayerController::AirlockSpill(FName StillId, float Litres) { ServerSpill(StillId, Litres); }
void AAirlockPlayerController::ServerFloorStatus_Implementation()
{
	if (!CheatAllowed(TEXT("AirlockFloor"))) { return; }
	const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	const auto* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	if (!GM || !GS) { UE_LOG(LogAirlock, Log, TEXT("Floor status rejected: no game state")); return; }
	UE_LOG(LogAirlock, Log, TEXT("%s"), *GM->HazardSummary());
	UE_LOG(LogAirlock, Log, TEXT("FloorCriticals %d"), GS->FloorCriticals);
}
void AAirlockPlayerController::ServerSpill_Implementation(FName StillId, float Litres)
{
	if (!CheatAllowed(TEXT("AirlockSpill"))) { return; }
	if (auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr) { GM->HazardSpill(StillId, Litres); }
}
void AAirlockPlayerController::AirlockKnob(FName StillId, FString Knob, int32 Steps) { ServerKnob(StillId, Knob, Steps, false); }
void AAirlockPlayerController::AirlockKnobStep(FName StillId, FString Knob, int32 Delta) { ServerKnob(StillId, Knob, Delta, true); }
void AAirlockPlayerController::AirlockStillLoad(FName StillId, FName CaskId) { ServerStillLoad(StillId, CaskId); }
void AAirlockPlayerController::AirlockStill(FName StillId, FString Lever) { ServerStillLever(StillId, Lever); }
void AAirlockPlayerController::ServerRefineryStatus_Implementation()
{
	if (!CheatAllowed(TEXT("AirlockRefinery"))) { return; }
	const auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	if (!GM) { UE_LOG(LogAirlock, Log, TEXT("Refinery status rejected: no game mode")); return; }
	TArray<FString> Lines;
	GM->RefinerySummary().ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines) { UE_LOG(LogAirlock, Log, TEXT("%s"), *Line); }
}
void AAirlockPlayerController::ServerKnob_Implementation(FName StillId, const FString& Knob, int32 Steps, bool bStep)
{
	if (!CheatAllowed(bStep ? TEXT("AirlockKnobStep") : TEXT("AirlockKnob"))) { return; }
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	if (!GM) { UE_LOG(LogAirlock, Log, TEXT("Knob rejected: no game mode")); return; }
	ERefineryKnob Which;
	if (!RefineryLayouts::ParseKnob(Knob, Which)) { UE_LOG(LogAirlock, Log, TEXT("Knob rejected: unknown knob %s (Heat|Feed|Vent)"), *Knob); return; }
	if (bStep) { GM->RefineryStepKnob(StillId, Which, Steps); } else { GM->RefinerySetKnob(StillId, Which, Steps); }
}
void AAirlockPlayerController::ServerStillLoad_Implementation(FName StillId, FName CaskId)
{
	if (!CheatAllowed(TEXT("AirlockStillLoad"))) { return; }
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	if (!GM) { UE_LOG(LogAirlock, Log, TEXT("Refinery load rejected: no game mode")); return; }
	GM->RefineryLoadHopper(StillId, CaskId);
}
void AAirlockPlayerController::ServerStillLever_Implementation(FName StillId, const FString& Lever)
{
	if (!CheatAllowed(TEXT("AirlockStill"))) { return; }
	auto* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAirlockGameMode>() : nullptr;
	if (!GM) { UE_LOG(LogAirlock, Log, TEXT("Still rejected: no game mode")); return; }
	ERefineryLever Which;
	if (!RefineryLayouts::ParseLever(Lever, Which)) { UE_LOG(LogAirlock, Log, TEXT("Still rejected: unknown lever %s (Reset|Purge|Scrape|Repair|Calibrate)"), *Lever); return; }
	GM->RefineryLever(StillId, Which);
}
