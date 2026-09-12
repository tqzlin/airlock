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
#include "UI/CarryPromptWidget.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"

AAirlockCharacter::AAirlockCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); // eye height above capsule centre
	FirstPersonCamera->bUsePawnControlRotation = true;
	CarrySocket = CreateDefaultSubobject<USceneComponent>(TEXT("CarrySocket"));
	CarrySocket->SetupAttachment(GetCapsuleComponent());
	CarrySocket->SetRelativeLocation(CarryOffset);

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
	AActor* Target = TraceInteractTarget();
	UE_LOG(LogAirlock, Log, TEXT("Interact: %s"), Target ? *Target->GetName() : TEXT("nothing in reach"));
	if (CarriedCrate)
	{
		if (auto* CargoSlot = Cast<ACargoSlot>(Target)) { ServerPlace(CargoSlot); }
		else { ServerPutDown(); }
		return;
	}
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
	if (CarriedCrate) { UE_LOG(LogAirlock, Log, TEXT("Hold rejected: hands full")); return; }
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
	if (HasAuthority()) { BindCarriedCargo(); CheckCarriedCargo(); }
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
		if (!CarryWidget && IsValid(CarriedCrate))
		{
			CarryWidget = CreateWidget<UCarryPromptWidget>(Cast<APlayerController>(GetController()), UCarryPromptWidget::StaticClass());
			if (CarryWidget) { CarryWidget->AddToViewport(4); }
		}
		if (CarryWidget) { CarryWidget->UpdateCarry(CarriedCrate); }
	}
}
void AAirlockCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, HoldTarget, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, HoldProgress, COND_OwnerOnly);
	DOREPLIFETIME(AAirlockCharacter, CarriedCrate);
}
void AAirlockCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority() && Reason == EEndPlayReason::Destroyed && CarriedCrate) { ServerPutDown(); }
	if (ObservedCargoState.IsValid()) { ObservedCargoState->OnShipGraphChanged.Remove(CargoChangedHandle); }
	if (CarryWidget) { CarryWidget->RemoveFromParent(); CarryWidget = nullptr; }
	if (HoldWidget) { HoldWidget->RemoveFromParent(); HoldWidget = nullptr; }
	if (HasAuthority()) { ResetHold(); }
	Super::EndPlay(Reason);
}

void AAirlockCharacter::BeginPlay()
{
	Super::BeginPlay();
	CarrySocket->SetRelativeLocation(CarryOffset);
	CacheCarrySpeed();
	OnRep_CarriedCrate();
	if (HasAuthority()) { BindCarriedCargo(); }
}
void AAirlockCharacter::CacheCarrySpeed()
{
	if (!bCarrySpeedCached)
	{
		BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
		bCarrySpeedCached = true;
	}
}
void AAirlockCharacter::OnRep_CarriedCrate()
{
	CacheCarrySpeed();
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * (IsValid(CarriedCrate) ? CarrySpeedFactor : 1.f);
}
AActor* AAirlockCharacter::TraceInteractTarget() const
{
	if (!FirstPersonCamera || !GetWorld()) { return nullptr; }
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("AirlockInteract"), false, this);
	Params.AddIgnoredActor(CarriedCrate);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + FirstPersonCamera->GetForwardVector() * InteractReach, ECC_Visibility, Params)
		? Hit.GetActor() : nullptr;
}
void AAirlockCharacter::ServerPickUp_Implementation(ACargoCrate* Crate)
{
	if (!HasAuthority()) { return; }
	if (CarriedCrate) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: hands full")); return; }
	if (!IsValid(Crate) || Crate->GetWorld() != GetWorld()) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: missing crate")); return; }
	if (!IsInHoldRange(Crate)) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: out of range")); return; }
	const auto* State = Crate->GetCargoState();
	if (!State) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: missing cargo state")); return; }
	if (State->State == ECargoState::Vented) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: vented")); return; }
	if (State->State == ECargoState::Carried) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: already carried")); return; }
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	if (!GM || !GM->CargoPickUp(Crate->GetCargoId())) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: model refused")); return; }
	ResetHold();
	CarriedCrate = Crate;
	Crate->AttachToComponent(CarrySocket, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	OnRep_CarriedCrate();
	ForceNetUpdate();
	Crate->ForceNetUpdate();
	UE_LOG(LogAirlock, Log, TEXT("Carry %s by %s"), *Crate->GetCargoId().ToString(), *GetName());
}
FVector AAirlockCharacter::PutDownLocation() const
{
	const FVector Start = GetActorLocation() + GetActorForwardVector() * 120.f;
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("CargoPutDown"), false, this);
	Params.AddIgnoredActor(CarriedCrate);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 300), ECC_Visibility, Params)) { return Hit.ImpactPoint; }
	return FVector(Start.X, Start.Y, GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}
void AAirlockCharacter::DetachCarriedCrateAt(const FTransform& Transform)
{
	ACargoCrate* Crate = CarriedCrate;
	CarriedCrate = nullptr;
	if (IsValid(Crate))
	{
		Crate->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Crate->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
		Crate->ForceNetUpdate();
	}
	OnRep_CarriedCrate();
	ForceNetUpdate();
}
void AAirlockCharacter::ServerPutDown_Implementation()
{
	if (!HasAuthority()) { return; }
	if (!IsValid(CarriedCrate)) { UE_LOG(LogAirlock, Log, TEXT("PutDown rejected: hands empty")); return; }
	const FName Id = CarriedCrate->GetCargoId();
	const FTransform Drop(GetActorRotation(), PutDownLocation(), CarriedCrate->GetActorScale3D());
	const bool bVented = CarriedCrate->GetCargoState() && CarriedCrate->GetCargoState()->State == ECargoState::Vented;
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	if (!bVented && (!GM || !GM->CargoPutDown(Id))) { UE_LOG(LogAirlock, Log, TEXT("PutDown rejected: model refused")); return; }
	DetachCarriedCrateAt(Drop);
	UE_LOG(LogAirlock, Log, TEXT("PutDown %s by %s%s"), *Id.ToString(), *GetName(), bVented ? TEXT(" (vented)") : TEXT(""));
}
void AAirlockCharacter::ServerPlace_Implementation(ACargoSlot* CargoSlot)
{
	if (!HasAuthority()) { return; }
	if (!IsValid(CarriedCrate)) { UE_LOG(LogAirlock, Log, TEXT("Place rejected: hands empty")); return; }
	if (!IsValid(CargoSlot) || CargoSlot->GetWorld() != GetWorld()) { UE_LOG(LogAirlock, Log, TEXT("Place rejected: missing slot")); return; }
	if (!IsInHoldRange(CargoSlot)) { UE_LOG(LogAirlock, Log, TEXT("Place rejected: out of range")); return; }
	if (!CargoSlot->GetCrateId().IsNone()) { UE_LOG(LogAirlock, Log, TEXT("Place rejected: slot occupied")); return; }
	const FName Id = CarriedCrate->GetCargoId();
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	if (!GM || !GM->CargoPlace(Id, CargoSlot->GetSlotId())) { UE_LOG(LogAirlock, Log, TEXT("Place rejected: model refused")); return; }
	FTransform Rest = CargoSlot->GetActorTransform();
	Rest.SetLocation(Rest.TransformPosition(CargoSlot->CrateRestOffset));
	DetachCarriedCrateAt(Rest);
	UE_LOG(LogAirlock, Log, TEXT("Place %s in %s by %s"), *Id.ToString(), *CargoSlot->GetSlotId().ToString(), *GetName());
}
void AAirlockCharacter::BindCarriedCargo()
{
	if (ObservedCargoState.IsValid() || !GetWorld()) { return; }
	if (auto* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		ObservedCargoState = GS;
		CargoChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &AAirlockCharacter::CheckCarriedCargo);
	}
}
void AAirlockCharacter::CheckCarriedCargo()
{
	if (!HasAuthority() || !CarriedCrate) { return; }
	if (!IsValid(CarriedCrate)) { CarriedCrate = nullptr; OnRep_CarriedCrate(); ForceNetUpdate(); return; }
	const auto* State = CarriedCrate->GetCargoState();
	// Delegate notification frees a vented carrier in the same publication; Tick also covers late binding.
	if (State && State->State == ECargoState::Vented) { ServerPutDown(); }
}
