// Copyright Airlock project. All Rights Reserved.
#include "Ship/FuelValve.h"
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

AFuelValve::AFuelValve()
{
	bReplicates = true; bAlwaysRelevant = true;
	FuelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FuelRoot")); SetRootComponent(FuelRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(FuelRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(FuelRoot);
	LeakLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LeakLight"));
	LeakLight->SetupAttachment(FuelRoot);
	LeakLight->SetIntensityUnits(ELightUnits::Candelas);
	LeakLight->SetCastShadows(false); LeakLight->SetVisibility(false);
	LeakFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("LeakFX"));
	LeakFX->SetupAttachment(FuelRoot); LeakFX->bAutoActivate = false;
	LeakFX->OnSystemFinished.AddDynamic(this, &AFuelValve::OnLeakFinished);
}
void AFuelValve::Initialize(const FFuelFixtureSpec& InSpec) { check(HasAuthority()); Spec = InSpec; }
void AFuelValve::BeginPlay()
{
	Super::BeginPlay(); OnRep_Spec(); BindFuel();
	if (!ObservedState.IsValid()) { GetWorldTimerManager().SetTimer(BindTimer, this, &AFuelValve::BindFuel, .25f, true); }
}
void AFuelValve::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		Body->SetRelativeScale3D(Spec.Scale);
		ShipVisuals::LoadBody(Body, Spec.Mesh, FVector(.3,.3,1.4));
		ShipVisuals::ConfigureLabel(Label, 22.f, Spec.LabelYaw, FLinearColor::White,
			Spec.Transform.InverseTransformVectorNoScale(FVector(-100,0,155)));
		LeakLight->SetRelativeLocation(Spec.Transform.InverseTransformVectorNoScale(FVector(-25,0,110)));
		LeakLight->SetAttenuationRadius(140); LeakLight->SetIntensity(80);
		LeakLight->SetLightColor(FLinearColor::Red);
		LeakFX->SetRelativeLocation(FVector(0,0,130));
		LeakFX->SetRelativeScale3D(FVector(2.5));
		if (auto* Steam = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/ModSciInteriors/Particles/PS_SteamDoor.PS_SteamDoor"))) { LeakFX->SetTemplate(Steam); }
		bVisualsReady = true;
	}
	Refresh();
}
void AFuelValve::BindFuel()
{
	if (ObservedState.IsValid()) { return; }
	if (auto* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		ObservedState = GS; ChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &AFuelValve::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer); Refresh();
	}
}
void AFuelValve::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ObservedState.IsValid()) { ObservedState->OnShipGraphChanged.Remove(ChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this); Super::EndPlay(Reason);
}
void AFuelValve::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(AFuelValve, Spec, COND_InitialOnly);
}
const FFuelState* AFuelValve::GetFuelState() const
{
	const auto* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	return GS && FuelLayouts::Find(GS->CurrentSectionId) ? &GS->Fuel : nullptr;
}
void AFuelValve::Refresh()
{
	const auto* S = GetFuelState(); if (!bVisualsReady || !S) { return; }
	Label->SetText(FText::FromString(FString::Printf(TEXT("FEED VALVE\n%s"), S->bFeedLeak ? TEXT("leak") : TEXT("shut"))));
	LeakLight->SetVisibility(S->bFeedLeak);
	if (bLeaking != S->bFeedLeak)
	{
		bLeaking = S->bFeedLeak; GetWorldTimerManager().ClearTimer(LeakRestartTimer);
		if (bLeaking) { LeakFX->Activate(true); } else { LeakFX->DeactivateImmediate(); }
	}
}
FText AFuelValve::GetInteractPrompt_Implementation() const
{
	const auto* S = GetFuelState();
	return S && S->bFeedLeak ? NSLOCTEXT("Airlock", "ShutFeed", "Shut the feed valve (hold E)") : FText::GetEmpty();
}
float AFuelValve::GetInteractHoldSeconds_Implementation() const { return GetInteractPrompt_Implementation().IsEmpty() ? 0.f : 3.f; }
void AFuelValve::Interact_Implementation(APawn* Pawn) {}
void AFuelValve::OnHoldComplete_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !Pawn || GetInteractPrompt_Implementation().IsEmpty()) { return; }
	if (auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>()) { GM->FuelCloseLeak(); }
}
void AFuelValve::OnLeakFinished(UParticleSystemComponent* Finished)
{
	if (bLeaking) { LeakRestartTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AFuelValve::RestartLeak); }
}
void AFuelValve::RestartLeak() { if (bLeaking) { LeakFX->Activate(true); } }
