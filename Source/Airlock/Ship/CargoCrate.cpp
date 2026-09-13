// Copyright Airlock project. All Rights Reserved.
#include "Ship/CargoCrate.h"
#include "Ship/ShipVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

ACargoCrate::ACargoCrate()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	CargoRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CargoRoot"));
	SetRootComponent(CargoRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(CargoRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(CargoRoot);
	StateLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StateLabel"));
	StateLabel->SetupAttachment(CargoRoot);
	HeatLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HeatLight"));
	HeatLight->SetupAttachment(CargoRoot);
	HeatLight->SetIntensityUnits(ELightUnits::Candelas);
	HeatLight->SetCastShadows(false);
	HeatLight->SetVisibility(false);
	VentFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("VentFX"));
	VentFX->SetupAttachment(CargoRoot);
	VentFX->bAutoActivate = false;
	VentFX->OnSystemFinished.AddDynamic(this, &ACargoCrate::OnVentFinished);
}
void ACargoCrate::Initialize(const FCargoCrateSpec& InSpec) { check(HasAuthority()); Spec = InSpec; }
void ACargoCrate::BeginPlay()
{
	Super::BeginPlay();
	// Cargo is published before a placement transform is applied. Also covers replicated movement.
	CargoRoot->TransformUpdated.AddWeakLambda(this, [this](USceneComponent*, EUpdateTransformFlags, ETeleportType)
	{
		RefreshLabel();
	});
	OnRep_Spec();
	BindCargo();
	if (!CargoState.IsValid()) { GetWorldTimerManager().SetTimer(BindTimer, this, &ACargoCrate::BindCargo, .25f, true); }
}
void ACargoCrate::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		ShipVisuals::LoadBody(Body, Spec.Mesh, FallbackScale);
		HeatLight->SetRelativeLocation(FVector(0, 0, 60));
		HeatLight->SetAttenuationRadius(120.f);
		HeatLight->SetLightColor(FLinearColor::Red);
		HeatLight->SetIntensity(0);
		VentFX->SetRelativeLocation(FVector(0, 0, 80));
		VentFX->SetRelativeScale3D(FVector(2.5));
		if (UParticleSystem* Steam = Cast<UParticleSystem>(SteamTemplate.TryLoad())) { VentFX->SetTemplate(Steam); }
		else { UE_LOG(LogAirlock, Warning, TEXT("Cargo steam template failed to load: %s"), *SteamTemplate.ToString()); }
		bVisualsReady = true;
	}
	Refresh();
}
void ACargoCrate::BindCargo()
{
	if (CargoState.IsValid()) { return; }
	if (AAirlockGameState* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		CargoState = GS;
		CargoChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &ACargoCrate::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer);
		Refresh();
	}
}
void ACargoCrate::EndPlay(const EEndPlayReason::Type Reason)
{
	CargoRoot->TransformUpdated.RemoveAll(this);
	if (CargoState.IsValid()) { CargoState->OnShipGraphChanged.Remove(CargoChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}
void ACargoCrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACargoCrate, Spec, COND_InitialOnly);
}
const FCargoCrateState* ACargoCrate::GetCargoState() const
{
	const AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	return GS ? GS->Cargo.FindByPredicate([this](const auto& Crate) { return Crate.Id == Spec.Id; }) : nullptr;
}
void ACargoCrate::RefreshLabel()
{
	const auto* State = GetCargoState();
	if (!State || !bVisualsReady) { return; }
	FString Text = FCargoModel::StateName(State->State);
	if (State->bVolatile) { Text += FString::Printf(TEXT("  %dC"), FMath::FloorToInt(State->Temperature)); }
	Label->SetText(FText::FromName(State->Id));
	StateLabel->SetText(FText::FromString(Text));
	const FLinearColor Colour = State->State == ECargoState::Vented || State->Temperature >= 80.f
		? FLinearColor(1.f, .15f, .1f) : State->Temperature >= FCargoRates().WarnTemperature
		? FLinearColor(1.f, .6f, .1f) : FLinearColor::White;
	ShipVisuals::ConfigureLabel(Label, LabelSize, Spec.LabelYaw, FLinearColor::White, FVector(0, 0, LabelHeight));
	ShipVisuals::ConfigureLabel(StateLabel, LabelSize, Spec.LabelYaw, Colour, FVector(0, 0, LabelHeight - LabelSize));
	const bool bShowLabel = State->State != ECargoState::Carried && State->State != ECargoState::Burned;
	Label->SetVisibility(bShowLabel);
	StateLabel->SetVisibility(bShowLabel);
}
void ACargoCrate::Refresh()
{
	const auto* State = GetCargoState();
	if (!State || !bVisualsReady) { return; }
	RefreshLabel();
	const bool bBurned = State->State == ECargoState::Burned;
	SetActorHiddenInGame(bBurned);
	SetActorEnableCollision(!bBurned);
	const FCargoRates Rates;
	const float Heat = FMath::Clamp((State->Temperature - Rates.WarnTemperature) / (Rates.VentTemperature - Rates.WarnTemperature), 0.f, 1.f);
	ShipVisuals::ApplyCargoHeat(Body, !bBurned && State->bVolatile ? Heat : 0.f);
	HeatLight->SetVisibility(!bBurned && State->bVolatile && State->Temperature >= Rates.WarnTemperature);
	const bool bShouldPuff = !bBurned && State->State != ECargoState::Vented && CargoState.IsValid() && CargoState->PuffingCrates.Contains(Spec.Id);
	if (bShouldPuff != bPuffing)
	{
		bPuffing = bShouldPuff; bDimPuffLight = false;
		GetWorldTimerManager().ClearTimer(PuffLightTimer);
		if (bPuffing) { GetWorldTimerManager().SetTimer(PuffLightTimer, this, &ACargoCrate::FlickerPuffLight, .4f, true); }
	}
	HeatLight->SetIntensity(HeatIntensity * Heat * (bPuffing && bDimPuffLight ? .6f : 1.f));
	const bool bShouldVent = State->State == ECargoState::Vented || bPuffing;
	if (bShouldVent != bVenting)
	{
		bVenting = bShouldVent;
		GetWorldTimerManager().ClearTimer(VentRestartTimer);
		if (bVenting) { VentFX->Activate(true); }
		else { VentFX->DeactivateImmediate(); }
	}
}
void ACargoCrate::OnVentFinished(UParticleSystemComponent* FinishedSystem)
{
	if (bVenting) { VentRestartTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ACargoCrate::RestartVent); }
}
void ACargoCrate::RestartVent()
{
	if (bVenting) { VentFX->Activate(true); }
}
void ACargoCrate::FlickerPuffLight()
{
	bDimPuffLight = !bDimPuffLight;
	Refresh();
}
FText ACargoCrate::GetInteractPrompt_Implementation() const
{
	const auto* State = GetCargoState();
	if (!State || State->State == ECargoState::Carried || State->State == ECargoState::Vented || State->State == ECargoState::Burned) { return FText::GetEmpty(); }
	return FText::Format(NSLOCTEXT("Airlock", "CargoPickUp", "Pick up {0}"), FText::FromName(Spec.Id));
}

void ACargoCrate::Interact_Implementation(APawn* Pawn)
{
	if (auto* Crew = Cast<AAirlockCharacter>(Pawn)) { Crew->ServerPickUp(this); }
}
