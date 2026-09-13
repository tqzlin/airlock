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
#include "Ship/ControlStation.h"
#include "UI/HoldProgressWidget.h"
#include "UI/CarryPromptWidget.h"
#include "UI/ExposureWidget.h"
#include "UI/PromptWidget.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockGameState.h"
#include "Ship/CargoCrate.h"
#include "Ship/CargoSlot.h"
#include "Ship/BypassValve.h"
#include "Ship/FuelIntake.h"
#include "Ship/StillControl.h"
#include "Ship/ShipModule.h"
#include "EngineUtils.h"

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
	if (Controller == nullptr || SeatedStation != nullptr || bBlackedOut)
	{
		return; // seated: the seat holds you; E stands up
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
	if (bBlackedOut) { return; }
	if (SeatedStation != nullptr)
	{
		UE_LOG(LogAirlock, Log, TEXT("Interact: standing up from %s"), *SeatedStation->GetActorNameOrLabel());
		ServerStandUp();
		return;
	}
	AActor* Target = TraceInteractTarget();
	UE_LOG(LogAirlock, Log, TEXT("Interact: %s"), Target ? *Target->GetName() : TEXT("nothing in reach"));
	if (CarriedCrate)
	{
		if (auto* CargoSlot = Cast<ACargoSlot>(Target)) { ServerPlace(CargoSlot); }
		else if (auto* Intake = Cast<AFuelIntake>(Target)) { ServerFeedCask(Intake); }
		else if (auto* Control = Cast<AStillControl>(Target); Control && Control->GetControlKind() == EStillControl::Hopper) { ServerLoadStill(Control); }
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
	if (bBlackedOut) { UE_LOG(LogAirlock, Log, TEXT("Hold rejected: blacked out")); return; }
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
	const FText Refusal = IInteractable::Execute_GetHoldRefusal(Target);
	if (!Refusal.IsEmpty())
	{
		UE_LOG(LogAirlock, Log, TEXT("Hold %s rejected: %s"), *Target->GetActorNameOrLabel(), *Refusal.ToString());
		return;
	}
	if (IInteractable::Execute_IsRepeatingHold(Target)) { IInteractable::Execute_OnHoldComplete(Target, this); }
	HoldTarget = Target;
	ForceNetUpdate();
	UE_LOG(LogAirlock, Log, TEXT("Hold %s started by %s duration=%.1fs"), *Target->GetActorNameOrLabel(), *GetName(), Seconds);
}
void AAirlockCharacter::ServerEndHold_Implementation()
{
	if (IsValid(HoldTarget) && IInteractable::Execute_IsRepeatingHold(HoldTarget))
	{
		UE_LOG(LogAirlock, Log, TEXT("Hold %s released by %s"), *HoldTarget->GetActorNameOrLabel(), *GetName());
	}
	ResetHold();
}
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
	if (HasAuthority()) { TickCrewExposure(DeltaSeconds); TickCrewSpill(DeltaSeconds); }
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
				const bool bRepeat = IInteractable::Execute_IsRepeatingHold(CompletedTarget);
				IInteractable::Execute_OnHoldComplete(CompletedTarget, this);
				if (bRepeat) { HoldElapsed = 0.f; HoldProgress = 0.f; }
				else
				{
					UE_LOG(LogAirlock, Log, TEXT("Hold %s complete by %s"), *TargetLabel, *GetName());
					ResetHold();
				}
			}
		}
		// Out of range pauses elapsed time; release explicitly resets it.
	}
	if (HasAuthority() && SeatedStation && !IsValid(SeatedStation))
	{
		LeaveSeat(false); // the station left with its level; give the legs back
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
		if (!PromptWidget)
		{
			PromptWidget = CreateWidget<UPromptWidget>(Cast<APlayerController>(GetController()), UPromptWidget::StaticClass());
			if (PromptWidget) { PromptWidget->AddToViewport(3); }
		}
		if (PromptWidget)
		{
			const FText PromptNow = InteractPromptFor(TraceInteractTarget(), IsValid(CarriedCrate), IsValid(HoldTarget));
			PromptWidget->UpdatePrompt(PromptNow);
			if (PromptNow.ToString() != LastPrompt)
			{
				LastPrompt = PromptNow.ToString();
				UE_LOG(LogAirlock, Log, TEXT("Prompt: %s"), LastPrompt.IsEmpty() ? TEXT("none") : *LastPrompt);
			}
		}
		if (!ExposureWidget)
		{
			ExposureWidget = CreateWidget<UExposureWidget>(Cast<APlayerController>(GetController()), UExposureWidget::StaticClass());
			if (ExposureWidget) { ExposureWidget->AddToViewport(5); }
		}
		if (ExposureWidget)
		{
			const auto* GS = GetWorld()->GetGameState<AAirlockGameState>();
			ExposureWidget->UpdateExposure(Exposure, bBlackedOut, BlackoutEndServerTime, GS ? GS->GetServerWorldTimeSeconds() : 0.0);
		}
	}
}
void AAirlockCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, HoldTarget, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, HoldProgress, COND_OwnerOnly);
	DOREPLIFETIME(AAirlockCharacter, CarriedCrate);
	DOREPLIFETIME(AAirlockCharacter, SeatedStation);
	DOREPLIFETIME(AAirlockCharacter, bBlackedOut);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, Exposure, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, InSpill, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AAirlockCharacter, BlackoutEndServerTime, COND_OwnerOnly);
}
void AAirlockCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority() && Reason == EEndPlayReason::Destroyed && CarriedCrate) { ServerPutDown(); }
	if (ObservedCargoState.IsValid()) { ObservedCargoState->OnShipGraphChanged.Remove(CargoChangedHandle); }
	if (CarryWidget) { CarryWidget->RemoveFromParent(); CarryWidget = nullptr; }
	if (HoldWidget) { HoldWidget->RemoveFromParent(); HoldWidget = nullptr; }
	if (PromptWidget) { PromptWidget->RemoveFromParent(); PromptWidget = nullptr; }
	if (ExposureWidget) { ExposureWidget->RemoveFromParent(); ExposureWidget = nullptr; }
	if (HasAuthority()) { ResetHold(); LeaveSeat(false); }
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
	ApplyWalkSpeed();
}
void AAirlockCharacter::ApplyWalkSpeed()
{
	CacheCarrySpeed();
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * (IsValid(CarriedCrate) ? CarrySpeedFactor : 1.f)
		* (Exposure >= ExposureRates.SlowAtExposure ? ExposureRates.SlowFactor : 1.f)
		* (InSpill.IsNone() ? 1.f : FFloorHazardRates().SlowFactor);
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
	if (bBlackedOut) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: blacked out")); return; }
	if (!HasAuthority()) { return; }
	if (CarriedCrate) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: hands full")); return; }
	if (SeatedStation) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: seated")); return; }
	if (!IsValid(Crate) || Crate->GetWorld() != GetWorld()) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: missing crate")); return; }
	if (!IsInHoldRange(Crate)) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: out of range")); return; }
	const auto* State = Crate->GetCargoState();
	if (!State) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: missing cargo state")); return; }
	if (State->State == ECargoState::Burned) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: burned")); return; }
	if (State->State == ECargoState::Loaded) { UE_LOG(LogAirlock, Log, TEXT("PickUp rejected: loaded into a still")); return; }
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
	// A cask lands on something the crew can see (M5-5): an interactable's invisible marker (a still control's box, a
	// puddle's box, the seat) blocks the Visibility trace only for E, so the drop looks through it - before this a cask
	// dropped in a puddle rested on the marker's top 30 cm up, and a drop whose trace started inside a control's
	// marker (an initial overlap answers with the start point) hung 96 cm over the deck. Hidden shapes of other actors
	// (a test world's floor box) still count as ground.
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 300), ECC_Visibility, Params)) { break; }
		const UPrimitiveComponent* Surface = Hit.GetComponent();
		const AActor* MarkerOwner = Surface ? Surface->GetOwner() : nullptr;
		const bool bMarker = Surface && Surface->bHiddenInGame && MarkerOwner && MarkerOwner->Implements<UInteractable>();
		if (!bMarker) { return Hit.ImpactPoint; }
		Params.AddIgnoredComponent(Surface);
	}
	return FVector(Start.X, Start.Y, GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}
void AAirlockCharacter::DetachCarriedCrateAt(const FTransform& Transform)
{
	ACargoCrate* Crate = CarriedCrate;
	CarriedCrate = nullptr;
	SpillDwellSeconds = 0.f;
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
	// The landing point is logged (M5-5) so a proof can check the cask rests on the deck, not on a marker.
	UE_LOG(LogAirlock, Log, TEXT("PutDown %s by %s%s at %s"), *Id.ToString(), *GetName(), bVented ? TEXT(" (vented)") : TEXT(""),
		*Drop.GetLocation().ToString());
}
void AAirlockCharacter::ServerPlace_Implementation(ACargoSlot* CargoSlot)
{
	if (bBlackedOut) { UE_LOG(LogAirlock, Log, TEXT("Place rejected: blacked out")); return; }
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
	// A cask loaded into a still out of the carrier's hands (M5-3a) leaves them where it is; the crate hides itself.
	else if (State && State->State == ECargoState::Loaded) { DetachCarriedCrateAt(CarriedCrate->GetActorTransform()); }
}

// --- Control-room seat ---
void AAirlockCharacter::ServerSitAt_Implementation(AControlStation* Station)
{
	if (bBlackedOut) { UE_LOG(LogAirlock, Log, TEXT("Seat rejected: blacked out")); return; }
	if (!IsValid(Station) || Station->GetWorld() != GetWorld())
	{
		UE_LOG(LogAirlock, Log, TEXT("Seat rejected: no such station in this world"));
		return;
	}
	if (SeatedStation == Station) { return; }
	if (CarriedCrate) { UE_LOG(LogAirlock, Log, TEXT("Seat %s rejected: hands full"), *Station->GetStationId().ToString()); return; }
	if (!IsInHoldRange(Station))
	{
		UE_LOG(LogAirlock, Log, TEXT("Seat %s rejected: out of range (%.0f cm, reach %.0f cm)"), *Station->GetStationId().ToString(),
			FVector::Distance(GetActorLocation(), Station->GetActorLocation()), InteractReach * 1.25f);
		return;
	}
	if (!Station->ServerSeat(this))
	{
		UE_LOG(LogAirlock, Log, TEXT("Seat %s rejected: taken by %s"), *Station->GetStationId().ToString(),
			Station->GetOccupant() ? *Station->GetOccupant()->GetName() : TEXT("?"));
		return;
	}
	ResetHold();
	LeaveSeat(false);
	SeatedStation = Station;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TeleportTo(Station->GetSeatLocation() + FVector(0, 0, HalfHeight), Station->GetSeatRotation(), false, true);
	if (Controller) { Controller->SetControlRotation(Station->GetSeatRotation()); }
	ApplyLocalMovement();
	ForceNetUpdate();
	UE_LOG(LogAirlock, Log, TEXT("Seat %s taken by %s"), *Station->GetStationId().ToString(), *GetName());
}
void AAirlockCharacter::ServerStandUp_Implementation() { LeaveSeat(true); }
void AAirlockCharacter::LeaveSeat(bool bToExit)
{
	if (!SeatedStation) { return; }
	AControlStation* Station = SeatedStation;
	SeatedStation = nullptr;
	if (IsValid(Station))
	{
		Station->ServerVacate(this);
		if (bToExit)
		{
			const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			TeleportTo(Station->GetExitLocation() + FVector(0, 0, HalfHeight), GetActorRotation(), false, true);
		}
		UE_LOG(LogAirlock, Log, TEXT("Seat %s released by %s"), *Station->GetStationId().ToString(), *GetName());
	}
	ApplyLocalMovement();
	ForceNetUpdate();
}
void AAirlockCharacter::ApplyLocalMovement()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(SeatedStation || bBlackedOut ? MOVE_None : MOVE_Walking);
	}
}
void AAirlockCharacter::OnRep_SeatedStation()
{
	// Owning client mirrors the server's movement mode so prediction does not fight the seat.
	if (IsLocallyControlled()) { ApplyLocalMovement(); }
}

void AAirlockCharacter::ServerSetScrubMode_Implementation(EScrubMode Mode) { SetScrubModeServer(Mode, true); }
bool AAirlockCharacter::SetScrubModeServer(EScrubMode Mode, bool bRequireReach)
{
	if (!HasAuthority()) { return false; }
	if (bBlackedOut) { UE_LOG(LogAirlock, Log, TEXT("Scrub rejected: blacked out")); return false; }
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	const auto* GS = GetWorld()->GetGameState<AAirlockGameState>();
	if (!GM || !GM->AirHasAtmosphere()) { UE_LOG(LogAirlock, Log, TEXT("Scrub rejected: No atmosphere on this section")); return false; }
	if (GS && GS->Atmosphere.bScrubDisabled) { UE_LOG(LogAirlock, Log, TEXT("Scrub rejected: life support failed")); return false; }
	if (bRequireReach)
	{
		bool bInReach = false;
		for (TActorIterator<AShipModule> It(GetWorld()); It; ++It)
		{
			if (It->GetModuleType() == EShipModuleType::LifeSupport && IsInHoldRange(*It)) { bInReach = true; break; }
		}
		if (!bInReach) { UE_LOG(LogAirlock, Log, TEXT("Scrub rejected: out of range")); return false; }
	}
	if (!GM->AirSetScrubMode(Mode)) { UE_LOG(LogAirlock, Log, TEXT("Scrub rejected: invalid mode")); return false; }
	UE_LOG(LogAirlock, Log, TEXT("Scrub %s by %s"), FShipAtmosphere::ModeName(Mode), *GetName());
	return true;
}

void AAirlockCharacter::OnRep_Exposure() { ApplyWalkSpeed(); }
void AAirlockCharacter::OnRep_InSpill() { ApplyWalkSpeed(); }
void AAirlockCharacter::TickCrewSpill(float DeltaSeconds)
{
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	const auto* GS = GetWorld()->GetGameState<AAirlockGameState>();
	const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FName Puddle = GM && GM->RefineryHasFloor() && GS && GS->bVoyageActive ? GM->HazardContains(Feet) : NAME_None;
	if (InSpill != Puddle)
	{
		InSpill = Puddle; ApplyWalkSpeed(); ForceNetUpdate();
	}
	if (InSpill.IsNone() || !IsValid(CarriedCrate) || bBlackedOut) { SpillDwellSeconds = 0.f; return; }
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }
	SpillDwellSeconds += DeltaSeconds;
	if (SpillDwellSeconds >= FFloorHazardRates().StumbleSeconds)
	{
		UE_LOG(LogAirlock, Log, TEXT("Stumble %s drops %s in %s"), *GetName(), *CarriedCrate->GetCargoId().ToString(), *InSpill.ToString());
		ServerPutDown();
		SpillDwellSeconds = 0.f;
		if (!CarriedCrate) { GM->HazardCountStumble(this); }
	}
}
void AAirlockCharacter::OnRep_BlackedOut() { if (IsLocallyControlled()) { ApplyLocalMovement(); } }
bool AAirlockCharacter::SetExposureServer(float Percent)
{
	if (!HasAuthority() || !FMath::IsFinite(Percent) || Percent < 0 || Percent > 100) { return false; }
	Exposure = Percent;
	ApplyWalkSpeed(); ForceNetUpdate();
	return true;
}
void AAirlockCharacter::TickCrewExposure(float DeltaSeconds)
{
	const auto* GS = GetWorld()->GetGameState<AAirlockGameState>();
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	if (!GM || !GM->AirHasAtmosphere() || !GS || !GS->bVoyageActive || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return; }
	const double Now = GS->GetServerWorldTimeSeconds();
	if (bBlackedOut)
	{
		if (Now >= BlackoutEndServerTime)
		{
			Exposure = ExposureRates.ExposureAfterBlackout;
			LastExposureForLog = Exposure;
			bBlackedOut = false; BlackoutEndServerTime = 0;
			ApplyWalkSpeed(); ApplyLocalMovement(); ForceNetUpdate();
			UE_LOG(LogAirlock, Log, TEXT("Recovered %s"), *GetName());
		}
		return;
	}
	// An injected 100 must black out on this tick even when clean air would otherwise recover it.
	if (Exposure < 100.f) { Exposure = FShipAtmosphere::TickExposure(Exposure, GS->Atmosphere.Ppm, DeltaSeconds, ExposureRates); }
	ApplyWalkSpeed();
	for (const float Threshold : { 30.f, 60.f, 100.f })
	{
		if (FMath::FloorToInt(LastExposureForLog) < Threshold && FMath::FloorToInt(Exposure) >= Threshold)
		{
			UE_LOG(LogAirlock, Log, TEXT("Exposure %s %d%%"), *GetName(), FMath::FloorToInt(Exposure));
		}
	}
	LastExposureForLog = Exposure;
	if (Exposure >= ExposureRates.SlowAtExposure && Now - LastCoughTime >= 5.0)
	{
		LastCoughTime = Now;
		UE_LOG(LogAirlock, Log, TEXT("Cough %s"), *GetName());
	}
	if (Exposure >= 100.f)
	{
		bBlackedOut = true;
		BlackoutEndServerTime = Now + ExposureRates.BlackoutSeconds;
		if (CarriedCrate) { ServerPutDown(); }
		LeaveSeat(false); ResetHold(); ApplyLocalMovement(); ForceNetUpdate();
		UE_LOG(LogAirlock, Log, TEXT("Blackout %s"), *GetName());
		GM->NotifyBlackout(this);
	}
}

void AAirlockCharacter::ServerOpenBypass_Implementation(ABypassValve* Valve) { OpenBypassServer(Valve, true); }
bool AAirlockCharacter::OpenBypassServer(ABypassValve* Valve, bool bRequireReach)
{
	if (!HasAuthority()) { return false; }
	const TCHAR* Rejection = nullptr;
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	if (bBlackedOut) { Rejection = TEXT("blacked out"); }
	else if (!GM || !GM->FuelHasSystem()) { Rejection = TEXT("No fuel system on this section"); }
	else if (bRequireReach && (!IsValid(Valve) || Valve->GetWorld() != GetWorld())) { Rejection = TEXT("missing valve"); }
	else if (bRequireReach && !IsInHoldRange(Valve)) { Rejection = TEXT("out of range"); }
	else if (!GM->FuelOpenBypass()) { Rejection = TEXT("already open"); }
	if (Rejection) { UE_LOG(LogAirlock, Log, TEXT("Bypass rejected: %s"), Rejection); return false; }
	UE_LOG(LogAirlock, Log, TEXT("Bypass opened by %s"), *GetName()); return true;
}
void AAirlockCharacter::ServerFeedCask_Implementation(AFuelIntake* Intake)
{
	if (!HasAuthority()) { return; }
	const TCHAR* Rejection = nullptr;
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	const auto* GS = GetWorld()->GetGameState<AAirlockGameState>();
	if (bBlackedOut) { Rejection = TEXT("blacked out"); }
	else if (!IsValid(Intake) || Intake->GetWorld() != GetWorld()) { Rejection = TEXT("missing intake"); }
	else if (!IsInHoldRange(Intake)) { Rejection = TEXT("out of range"); }
	else if (!IsValid(CarriedCrate)) { Rejection = TEXT("hands empty"); }
	else if (!GM || !GM->FuelHasSystem() || !GS) { Rejection = TEXT("No fuel system on this section"); }
	else if (!GS->Fuel.bBypassOpen) { Rejection = TEXT("bypass closed"); }
	else if (!GS->Fuel.BurningCrate.IsNone()) { Rejection = TEXT("intake busy"); }
	if (Rejection) { UE_LOG(LogAirlock, Log, TEXT("Feed rejected: %s"), Rejection); return; }
	const FName Id = CarriedCrate->GetCargoId();
	if (!GM->FuelFeedCask(Id)) { UE_LOG(LogAirlock, Log, TEXT("Feed rejected: model refused")); return; }
	DetachCarriedCrateAt(Intake->GetActorTransform());
	UE_LOG(LogAirlock, Log, TEXT("Feed %s by %s"), *Id.ToString(), *GetName());
}

void AAirlockCharacter::ServerLoadStill_Implementation(AStillControl* Hopper)
{
	if (!HasAuthority()) { return; }
	const TCHAR* Rejection = nullptr;
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	if (bBlackedOut) { Rejection = TEXT("blacked out"); }
	else if (!IsValid(Hopper) || Hopper->GetWorld() != GetWorld() || Hopper->GetControlKind() != EStillControl::Hopper) { Rejection = TEXT("missing hopper"); }
	else if (!IsInHoldRange(Hopper)) { Rejection = TEXT("out of range"); }
	else if (!IsValid(CarriedCrate)) { Rejection = TEXT("hands empty"); }
	else if (!GM || !GM->RefineryHasFloor()) { Rejection = TEXT("No refinery on this section"); }
	if (Rejection) { UE_LOG(LogAirlock, Log, TEXT("Load rejected: %s"), Rejection); return; }
	const FName Cask = CarriedCrate->GetCargoId();
	const FName Still = Hopper->GetStillId();
	const FTransform InHand = CarriedCrate->GetActorTransform();
	if (!GM->RefineryLoadHopper(Still, Cask)) { UE_LOG(LogAirlock, Log, TEXT("Load rejected: model refused")); return; }
	// PublishCargo normally clears the carrier synchronously; retain the feed verb's safety net for late binding.
	if (CarriedCrate) { DetachCarriedCrateAt(InHand); }
	UE_LOG(LogAirlock, Log, TEXT("Load %s into %s by %s"), *Cask.ToString(), *Still.ToString(), *GetName());
}

FText AAirlockCharacter::InteractPromptFor(AActor* Target, bool bCarrying, bool bHolding)
{
	if (bHolding || !IsValid(Target) || !Target->Implements<UInteractable>()) { return FText::GetEmpty(); }
	if (bCarrying)
	{
		const auto* Control = Cast<AStillControl>(Target);
		const bool bTakesACask = Target->IsA<ACargoSlot>() || Target->IsA<AFuelIntake>() || (Control && Control->GetControlKind() == EStillControl::Hopper);
		if (!bTakesACask) { return FText::GetEmpty(); }
	}
	return IInteractable::Execute_GetInteractPrompt(Target);
}
