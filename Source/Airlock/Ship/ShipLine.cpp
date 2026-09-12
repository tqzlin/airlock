// Copyright Airlock project. All Rights Reserved.
#include "Ship/ShipLine.h"
#include "Ship/ShipVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Core/AirlockGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Core/AirlockGameMode.h"
#include "GameFramework/Pawn.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"

AShipLine::AShipLine()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	SetRootComponent(ShipRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(ShipRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(ShipRoot);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShortLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ShortLight"));
	ShortLight->SetupAttachment(ShipRoot);
	ShortLight->SetIntensityUnits(ELightUnits::Candelas);
	ShortLight->SetCastShadows(false);
	ShortLight->SetVisibility(false);
	LeakFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("LeakFX"));
	LeakFX->SetupAttachment(ShipRoot);
	LeakFX->bAutoActivate = false;
	LeakFX->OnSystemFinished.AddDynamic(this, &AShipLine::OnLeakFinished);
}
void AShipLine::Initialize(const FShipLineSpec& InSpec)
{
	check(HasAuthority());
	Spec = InSpec;
}
void AShipLine::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Spec();
	BindGraph();
	if (!GraphState.IsValid())
	{
		GetWorldTimerManager().SetTimer(BindTimer, this, &AShipLine::BindGraph, 0.25f, true);
	}
}
void AShipLine::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		ShipVisuals::LoadBody(Body, Spec.Mesh, FallbackScale);
		Label->SetWorldSize(LabelSize);
		Label->SetTextRenderColor(LabelColour);
		Label->SetRelativeLocation(FVector(0, 0, LabelHeight));
		Label->SetRelativeRotation(FRotator(0, Spec.LabelYaw - Spec.Transform.Rotator().Yaw, 0));
		ShortLight->SetRelativeLocation(FVector(0, 0, LightHeight));
		ShortLight->SetAttenuationRadius(LightRadius);
		ShortLight->SetLightColor(ShortColour);
		ShortLight->SetIntensity(0);
		LeakFX->SetRelativeLocation(FVector(0, 0, LightHeight));
		if (UParticleSystem* Steam = Cast<UParticleSystem>(SteamTemplate.TryLoad())) { LeakFX->SetTemplate(Steam); }
		else { UE_LOG(LogAirlock, Warning, TEXT("Ship steam template failed to load: %s"), *SteamTemplate.ToString()); }
		bVisualsReady = true;
	}
	Refresh();
}
void AShipLine::BindGraph()
{
	if (GraphState.IsValid()) { return; }
	if (AAirlockGameState* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		GraphState = GS;
		GraphChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &AShipLine::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer);
		Refresh();
	}
}
void AShipLine::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GraphState.IsValid()) { GraphState->OnShipGraphChanged.Remove(GraphChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}
void AShipLine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShipLine, Spec, COND_InitialOnly);
}
EShipLineFault AShipLine::GetFault() const
{
	const AAirlockGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	const FShipLineState* State = GS ? GS->ShipLines.FindByPredicate([this](const FShipLineState& L) { return L.Id == Spec.Id; }) : nullptr;
	return State ? State->Fault : EShipLineFault::None;
}
void AShipLine::Refresh()
{
	if (!bVisualsReady) { return; }
	const EShipLineFault Fault = GetFault();
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"), *Spec.Id.ToString(), FShipGraph::FaultName(Fault))));
	if (Fault == DisplayedFault) { return; }
	DisplayedFault = Fault;
	GetWorldTimerManager().ClearTimer(LeakRestartTimer);
	if (Fault == EShipLineFault::Leak) { LeakFX->Activate(true); }
	else { LeakFX->DeactivateImmediate(); }
	GetWorldTimerManager().ClearTimer(FlickerTimer);
	ShortLight->SetVisibility(Fault == EShipLineFault::Short);
	ShortLight->SetIntensity(0);
	bShortLit = false;
	if (Fault == EShipLineFault::Short)
	{
		Flicker();
		GetWorldTimerManager().SetTimer(FlickerTimer, this, &AShipLine::Flicker, FlickerSeconds, true);
	}
}
void AShipLine::OnLeakFinished(UParticleSystemComponent* FinishedSystem)
{
	// PS_SteamDoor may finish a door puff. Restart on the next tick, after Cascade
	// finishes deactivating the component, so a persistent fault keeps venting.
	if (GetFault() == EShipLineFault::Leak)
	{
		LeakRestartTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AShipLine::RestartLeak);
	}
}
void AShipLine::RestartLeak()
{
	if (GetFault() == EShipLineFault::Leak) { LeakFX->Activate(true); }
}
void AShipLine::Flicker()
{
	bShortLit = !bShortLit;
	ShortLight->SetIntensity(bShortLit ? ShortIntensity : 0.f);
}
FText AShipLine::GetInteractPrompt_Implementation() const
{
	switch (GetFault())
	{
	case EShipLineFault::Leak: return NSLOCTEXT("Airlock", "TapeLeak", "Tape the leak (hold E)");
	case EShipLineFault::Short: return NSLOCTEXT("Airlock", "PatchShort", "Patch the short (hold E)");
	default: return FText::GetEmpty();
	}
}
float AShipLine::GetInteractHoldSeconds_Implementation() const
{
	const EShipLineFault Fault = GetFault();
	return Fault == EShipLineFault::Leak ? LeakHoldSeconds : Fault == EShipLineFault::Short ? ShortHoldSeconds : 0.f;
}
void AShipLine::OnHoldComplete_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !Pawn || GetFault() == EShipLineFault::None) { return; }
	if (AAirlockGameMode* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>()) { GM->ShipRepairLine(Spec.Id); }
}
