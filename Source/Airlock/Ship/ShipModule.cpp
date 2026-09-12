// Copyright Airlock project. All Rights Reserved.
#include "Ship/ShipModule.h"
#include "Ship/ShipVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Core/AirlockGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AShipModule::AShipModule()
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
	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(ShipRoot);
	StatusLight->SetIntensityUnits(ELightUnits::Candelas);
	StatusLight->SetCastShadows(false);
}
void AShipModule::Initialize(const FShipModuleSpec& InSpec)
{
	check(HasAuthority());
	Spec = InSpec;
}
void AShipModule::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Spec();
	BindGraph();
	if (!GraphState.IsValid())
	{
		GetWorldTimerManager().SetTimer(BindTimer, this, &AShipModule::BindGraph, 0.25f, true);
	}
}
void AShipModule::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		ShipVisuals::LoadBody(Body, Spec.Mesh, FallbackScale);
		Label->SetWorldSize(LabelSize);
		Label->SetTextRenderColor(LabelColour);
		Label->SetRelativeLocation(FVector(0, 0, LabelHeight));
		Label->SetRelativeRotation(FRotator(0, Spec.LabelYaw - Spec.Transform.Rotator().Yaw, 0));
		StatusLight->SetRelativeLocation(FVector(0, 0, LightHeight));
		StatusLight->SetAttenuationRadius(LightRadius);
		StatusLight->SetIntensity(LightIntensity);
		bVisualsReady = true;
	}
	Refresh();
}
void AShipModule::BindGraph()
{
	if (GraphState.IsValid()) { return; }
	if (AAirlockGameState* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		GraphState = GS;
		GraphChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &AShipModule::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer);
		Refresh();
	}
}
void AShipModule::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GraphState.IsValid()) { GraphState->OnShipGraphChanged.Remove(GraphChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}
void AShipModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShipModule, Spec, COND_InitialOnly);
}
void AShipModule::Refresh()
{
	if (!GraphState.IsValid() || !bVisualsReady) { return; }
	const FShipModuleState* State = GraphState->ShipModules.FindByPredicate([this](const FShipModuleState& M) { return M.Id == Spec.Id; });
	if (!State) { return; }
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %s  %.0f%%"), *Spec.Id.ToString(), FShipGraph::StatusName(State->Status), State->Condition)));
	StatusLight->SetLightColor(State->Status == EShipModuleStatus::Nominal ? NominalColour : State->Status == EShipModuleStatus::Degraded ? DegradedColour : FailedColour);
}
