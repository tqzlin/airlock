// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockPlayerController.h"
#include "Airlock.h"
#include "Engine/LocalPlayer.h"
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
