// Copyright Airlock project. All Rights Reserved.
#include "Ship/CargoSlot.h"
#include "Ship/ShipVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ACargoSlot::ACargoSlot()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	CargoRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CargoRoot"));
	SetRootComponent(CargoRoot);
	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(CargoRoot);
	Pad->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Pad->SetCollisionResponseToAllChannels(ECR_Ignore);
	Pad->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(CargoRoot);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ACargoSlot::Initialize(const FCargoSlotSpec& InSpec) { check(HasAuthority()); Spec = InSpec; }
void ACargoSlot::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Spec();
	BindCargo();
	if (!CargoState.IsValid()) { GetWorldTimerManager().SetTimer(BindTimer, this, &ACargoSlot::BindCargo, .25f, true); }
}
void ACargoSlot::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		ShipVisuals::LoadBody(Pad, Spec.Mesh, FallbackScale);
		Label->SetWorldSize(22.f);
		Label->SetRelativeLocation(FVector(0, 0, Spec.bRack ? 180 : 35));
		Label->SetRelativeRotation(FRotator(0, 180 - Spec.Transform.Rotator().Yaw, 0));
		bVisualsReady = true;
	}
	Refresh();
}
void ACargoSlot::BindCargo()
{
	if (CargoState.IsValid()) { return; }
	if (AAirlockGameState* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		CargoState = GS;
		CargoChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &ACargoSlot::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer);
		Refresh();
	}
}
void ACargoSlot::EndPlay(const EEndPlayReason::Type Reason)
{
	if (CargoState.IsValid()) { CargoState->OnShipGraphChanged.Remove(CargoChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}
void ACargoSlot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACargoSlot, Spec, COND_InitialOnly);
}
FName ACargoSlot::GetCrateId() const
{
	const AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	if (!GS) { return NAME_None; }
	const auto* Crate = GS->Cargo.FindByPredicate([this](const auto& C) { return C.SlotId == Spec.Id; });
	return Crate ? Crate->Id : NAME_None;
}
void ACargoSlot::Refresh()
{
	if (!bVisualsReady) { return; }
	const FName Id = GetCrateId();
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"), *Spec.Id.ToString(), Id.IsNone() ? TEXT("empty") : *Id.ToString())));
}
FText ACargoSlot::GetInteractPrompt_Implementation() const
{
	return GetCrateId().IsNone() ? FText::Format(NSLOCTEXT("Airlock", "CargoPlace", "Place crate in {0}"), FText::FromName(Spec.Id)) : FText::GetEmpty();
}

void ACargoSlot::Interact_Implementation(APawn* Pawn)
{
	if (auto* Crew = Cast<AAirlockCharacter>(Pawn)) { Crew->ServerPlace(this); }
}
