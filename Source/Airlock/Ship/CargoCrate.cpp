// Copyright Airlock project. All Rights Reserved.
#include "Ship/CargoCrate.h"
#include "Ship/ShipVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Core/AirlockGameState.h"
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
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
		Label->SetWorldSize(LabelSize);
		Label->SetRelativeLocation(FVector(0, 0, LabelHeight));
		Label->SetRelativeRotation(FRotator(0, 180 - Spec.Transform.Rotator().Yaw, 0));
		HeatLight->SetRelativeLocation(FVector(0, 0, 60));
		HeatLight->SetAttenuationRadius(180.f);
		HeatLight->SetLightColor(FLinearColor::Red);
		HeatLight->SetIntensity(0);
		VentFX->SetRelativeLocation(FVector(0, 0, 50));
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
void ACargoCrate::Refresh()
{
	const auto* State = GetCargoState();
	if (!State || !bVisualsReady) { return; }
	FString Text = FString::Printf(TEXT("%s  %s"), *State->Id.ToString(), FCargoModel::StateName(State->State));
	if (State->bVolatile) { Text += FString::Printf(TEXT("  %dC"), FMath::FloorToInt(State->Temperature)); }
	Label->SetText(FText::FromString(Text));
	const FCargoRates Rates;
	const float Heat = FMath::Clamp((State->Temperature - Rates.WarnTemperature) / (Rates.VentTemperature - Rates.WarnTemperature), 0.f, 1.f);
	HeatLight->SetVisibility(State->bVolatile && State->Temperature >= Rates.WarnTemperature);
	HeatLight->SetIntensity(HeatIntensity * Heat);
	const bool bShouldVent = State->State == ECargoState::Vented;
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
FText ACargoCrate::GetInteractPrompt_Implementation() const
{
	const auto* State = GetCargoState();
	if (!State || State->State == ECargoState::Carried || State->State == ECargoState::Vented) { return FText::GetEmpty(); }
	return FText::Format(NSLOCTEXT("Airlock", "CargoPickUp", "Pick up {0}"), FText::FromName(Spec.Id));
}
