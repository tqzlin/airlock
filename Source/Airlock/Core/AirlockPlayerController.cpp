// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockPlayerController.h"
#include "Airlock.h"
#include "Blueprint/UserWidget.h"
#include "Core/AirlockGameInstance.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UI/VoyageBoardWidget.h"
#include "UI/CaptainLineWidget.h"
#include "Voyage/VoyageSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

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
	if (CaptainWidget)
	{
		CaptainWidget->RemoveFromParent();
		CaptainWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
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
