// Copyright Airlock project. All Rights Reserved.
#include "Ship/FuelIntake.h"
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

AFuelIntake::AFuelIntake()
{
	bReplicates = true; bAlwaysRelevant = true;
	FuelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FuelRoot")); SetRootComponent(FuelRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(FuelRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore); Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(FuelRoot);
}
void AFuelIntake::Initialize(const FFuelFixtureSpec& InSpec) { check(HasAuthority()); Spec = InSpec; }
void AFuelIntake::BeginPlay()
{
	Super::BeginPlay(); OnRep_Spec(); BindFuel();
	if (!ObservedState.IsValid()) { GetWorldTimerManager().SetTimer(BindTimer, this, &AFuelIntake::BindFuel, .25f, true); }
}
void AFuelIntake::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		Body->SetRelativeScale3D(Spec.Scale);
		ShipVisuals::LoadBody(Body, Spec.Mesh, FVector(.3,.3,1.4));
		ShipVisuals::ConfigureLabel(Label, 14.f, Spec.LabelYaw, FLinearColor::White,
			FVector(0,0,110));
		bVisualsReady = true;
	}
	Refresh();
}
void AFuelIntake::BindFuel()
{
	if (ObservedState.IsValid()) { return; }
	if (auto* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		ObservedState = GS; ChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &AFuelIntake::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer); Refresh();
	}
}
void AFuelIntake::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ObservedState.IsValid()) { ObservedState->OnShipGraphChanged.Remove(ChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this); Super::EndPlay(Reason);
}
void AFuelIntake::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(AFuelIntake, Spec, COND_InitialOnly);
}
const FFuelState* AFuelIntake::GetFuelState() const
{
	const auto* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	return GS && FuelLayouts::Find(GS->CurrentSectionId) ? &GS->Fuel : nullptr;
}
void AFuelIntake::Refresh()
{
	const auto* S = GetFuelState(); if (!bVisualsReady || !S) { return; }
	const FString Text = !S->BurningCrate.IsNone()
		? FString::Printf(TEXT("INTAKE\n%s %d s"), *S->BurningCrate.ToString(), S->BurnSecondsLeft)
		: S->bBypassOpen ? TEXT("INTAKE\nready") : TEXT("INTAKE\nbypass closed");
	Label->SetText(FText::FromString(Text));
}
FText AFuelIntake::GetInteractPrompt_Implementation() const
{
	const auto* S = GetFuelState();
	if (!S || !S->BurningCrate.IsNone()) { return FText::GetEmpty(); }
	return S->bBypassOpen ? NSLOCTEXT("Airlock", "FeedCask", "Feed a cask to the engines") : NSLOCTEXT("Airlock", "BypassClosed", "Bypass closed");
}
void AFuelIntake::Interact_Implementation(APawn* Pawn)
{
	if (auto* Crew = Cast<AAirlockCharacter>(Pawn)) { Crew->ServerFeedCask(this); }
}
