// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockCharacter.h"
#include "Airlock.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/AirlockPlayerController.h"
#include "Core/Interactable.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "UI/HoldProgressWidget.h"

AAirlockCharacter::AAirlockCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); // eye height above capsule centre
	FirstPersonCamera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->MaxWalkSpeed = 450.f;
	GetCharacterMovement()->JumpZVelocity = 420.f;

	InteractReach = 250.f;
}

void AAirlockCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	AAirlockPlayerController* PC = Cast<AAirlockPlayerController>(GetController());
	if (!EnhancedInput || !PC)
	{
		UE_LOG(LogAirlock, Warning, TEXT("AirlockCharacter: input not bound (EnhancedInput=%s, AirlockPlayerController=%s)"),
			EnhancedInput ? TEXT("yes") : TEXT("no"), PC ? TEXT("yes") : TEXT("no"));
		return;
	}

	EnhancedInput->BindAction(PC->MoveAction, ETriggerEvent::Triggered, this, &AAirlockCharacter::Move);
	EnhancedInput->BindAction(PC->LookAction, ETriggerEvent::Triggered, this, &AAirlockCharacter::Look);
	EnhancedInput->BindAction(PC->InteractAction, ETriggerEvent::Started, this, &AAirlockCharacter::Interact);
	EnhancedInput->BindAction(PC->InteractAction, ETriggerEvent::Completed, this, &AAirlockCharacter::EndInteract);
	EnhancedInput->BindAction(PC->InteractAction, ETriggerEvent::Canceled, this, &AAirlockCharacter::EndInteract);
}

void AAirlockCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller == nullptr)
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AAirlockCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller != nullptr)
	{
		AddControllerYawInput(Axis.X);
		AddControllerPitchInput(Axis.Y);
	}
}

void AAirlockCharacter::Interact(const FInputActionValue& Value)
{
	if (!FirstPersonCamera)
	{
		return;
	}

	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + FirstPersonCamera->GetForwardVector() * InteractReach;

	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("AirlockInteract"), /*bTraceComplex*/ false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	AActor* Target = bHit ? Hit.GetActor() : nullptr;
	UE_LOG(LogAirlock, Log, TEXT("Interact: %s"), Target ? *Target->GetName() : TEXT("nothing in reach"));
	if (Target && Target->Implements<UInteractable>())
	{
		if (IInteractable::Execute_GetInteractHoldSeconds(Target) > 0.f) { ServerBeginHold(Target); }
		else { IInteractable::Execute_Interact(Target, this); }
	}
}

void AAirlockCharacter::EndInteract(const FInputActionValue& Value) { ServerEndHold(); }

bool AAirlockCharacter::IsInHoldRange(const AActor* Target) const
{
	return IsValid(Target) && FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(InteractReach * 1.25f);
}
void AAirlockCharacter::ServerBeginHold_Implementation(AActor* Target)
{
	ResetHold();
	if (!IsValid(Target) || Target->GetWorld() != GetWorld() || !Target->Implements<UInteractable>())
	{
		UE_LOG(LogAirlock, Log, TEXT("Hold rejected: target is not an interactable in this world"));
		return;
	}
	const float Seconds = IInteractable::Execute_GetInteractHoldSeconds(Target);
	if (!FMath::IsFinite(Seconds) || Seconds <= 0.f || IInteractable::Execute_GetInteractPrompt(Target).IsEmpty())
	{
		UE_LOG(LogAirlock, Log, TEXT("Hold %s rejected: no active fault/hold interaction"), *Target->GetActorNameOrLabel());
		return;
	}
	if (!IsInHoldRange(Target))
	{
		UE_LOG(LogAirlock, Log, TEXT("Hold %s rejected: out of range (%.0f cm, reach %.0f cm)"),
			*Target->GetActorNameOrLabel(), FVector::Distance(GetActorLocation(), Target->GetActorLocation()), InteractReach * 1.25f);
		return;
	}
	HoldTarget = Target;
	ForceNetUpdate();
	UE_LOG(LogAirlock, Log, TEXT("Hold %s started by %s duration=%.1fs"), *Target->GetActorNameOrLabel(), *GetName(), Seconds);
}
void AAirlockCharacter::ServerEndHold_Implementation() { ResetHold(); }
void AAirlockCharacter::ResetHold()
{
	HoldTarget = nullptr;
	HoldElapsed = 0.f;
	HoldProgress = 0.f;
	ForceNetUpdate();
}
void AAirlockCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && (HoldTarget || HoldElapsed > 0.f))
	{
		const float Seconds = IsValid(HoldTarget) && HoldTarget->Implements<UInteractable>()
			? IInteractable::Execute_GetInteractHoldSeconds(HoldTarget) : 0.f;
		if (!FMath::IsFinite(Seconds) || Seconds <= 0.f || IInteractable::Execute_GetInteractPrompt(HoldTarget).IsEmpty())
		{
			// Another repair, destruction or travel ends this hold without a completion log.
			ResetHold();
		}
		else if (IsInHoldRange(HoldTarget) && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.f)
		{
			HoldElapsed += DeltaSeconds;
			HoldProgress = FMath::Clamp(HoldElapsed / Seconds, 0.f, 1.f);
			if (HoldProgress >= 1.f)
			{
				AActor* CompletedTarget = HoldTarget;
				const FString TargetLabel = CompletedTarget->GetActorNameOrLabel();
				IInteractable::Execute_OnHoldComplete(CompletedTarget, this);
				UE_LOG(LogAirlock, Log, TEXT("Hold %s complete by %s"), *TargetLabel, *GetName());
				ResetHold();
			}
		}
		// Out of range pauses elapsed time; release explicitly resets it.
	}
	if (IsLocallyControlled())
	{
		if (!HoldWidget && IsValid(HoldTarget))
		{
			HoldWidget = CreateWidget<UHoldProgressWidget>(Cast<APlayerController>(GetController()), UHoldProgressWidget::StaticClass());
			if (HoldWidget) { HoldWidget->AddToViewport(4); }
		}
		if (HoldWidget) { HoldWidget->UpdateHold(HoldTarget, HoldProgress); }
	}
}
void AAirlockCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, HoldTarget, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, HoldProgress, COND_OwnerOnly);
}
void AAirlockCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HoldWidget) { HoldWidget->RemoveFromParent(); HoldWidget = nullptr; }
	if (HasAuthority()) { ResetHold(); }
	Super::EndPlay(Reason);
}
