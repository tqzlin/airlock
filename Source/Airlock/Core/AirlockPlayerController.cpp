// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockPlayerController.h"
#include "Airlock.h"
#include "Blueprint/UserWidget.h"
#include "Core/AirlockGameState.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UI/VoyageBoardWidget.h"
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
