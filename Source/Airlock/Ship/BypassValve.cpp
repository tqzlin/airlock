// Copyright Airlock project. All Rights Reserved.
#include "Ship/BypassValve.h"
#include "Ship/ShipVisuals.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ABypassValve::ABypassValve()
{
	bReplicates = true; bAlwaysRelevant = true;
	FuelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FuelRoot")); SetRootComponent(FuelRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(FuelRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(FuelRoot);
	BypassLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BypassLight"));
	BypassLight->SetupAttachment(FuelRoot);
	BypassLight->SetIntensityUnits(ELightUnits::Candelas);
	BypassLight->SetCastShadows(false); BypassLight->SetVisibility(false);
}
void ABypassValve::Initialize(const FFuelFixtureSpec& InSpec) { check(HasAuthority()); Spec = InSpec; }
void ABypassValve::BeginPlay()
{
	Super::BeginPlay(); OnRep_Spec(); BindFuel();
	if (!ObservedState.IsValid()) { GetWorldTimerManager().SetTimer(BindTimer, this, &ABypassValve::BindFuel, .25f, true); }
}
void ABypassValve::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		Body->SetRelativeScale3D(Spec.Scale);
		ShipVisuals::LoadBody(Body, Spec.Mesh, FVector(.3,.3,1.4));
		ShipVisuals::ConfigureLabel(Label, 14.f, Spec.LabelYaw, FLinearColor::White,
			Spec.Transform.InverseTransformVectorNoScale(FVector(-35,0,155)));
		BypassLight->SetRelativeLocation(Spec.Transform.InverseTransformVectorNoScale(FVector(-25,0,110)));
		BypassLight->SetAttenuationRadius(140); BypassLight->SetIntensity(80);
		BypassLight->SetLightColor(FLinearColor(1.f,.45f,.02f));
		bVisualsReady = true;
	}
	Refresh();
}
void ABypassValve::BindFuel()
{
	if (ObservedState.IsValid()) { return; }
	if (auto* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		ObservedState = GS; ChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &ABypassValve::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer); Refresh();
	}
}
void ABypassValve::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ObservedState.IsValid()) { ObservedState->OnShipGraphChanged.Remove(ChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this); Super::EndPlay(Reason);
}
void ABypassValve::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(ABypassValve, Spec, COND_InitialOnly);
}
const FFuelState* ABypassValve::GetFuelState() const
{
	const auto* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	return GS && FuelLayouts::Find(GS->CurrentSectionId) ? &GS->Fuel : nullptr;
}
void ABypassValve::Refresh()
{
	const auto* S = GetFuelState(); if (!bVisualsReady || !S) { return; }
	Label->SetText(FText::FromString(FString::Printf(TEXT("BYPASS\n%s"), S->bBypassOpen ? TEXT("OPEN") : TEXT("closed"))));
	BypassLight->SetVisibility(S->bBypassOpen);
}
FText ABypassValve::GetInteractPrompt_Implementation() const
{
	const auto* S = GetFuelState();
	return S && !S->bBypassOpen ? NSLOCTEXT("Airlock", "OpenBypass", "Open the crude bypass") : FText::GetEmpty();
}
void ABypassValve::Interact_Implementation(APawn* Pawn)
{
	if (auto* Crew = Cast<AAirlockCharacter>(Pawn)) { Crew->ServerOpenBypass(this); }
}
