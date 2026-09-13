// Copyright Airlock project. All Rights Reserved.
#include "Ship/Spill.h"
#include "Ship/ShipVisuals.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ASpill::ASpill()
{
	bReplicates = true; bAlwaysRelevant = true;
	SpillRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SpillRoot")); SetRootComponent(SpillRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(SpillRoot);
	Body->SetRelativeLocation(FVector(0, 0, 1));
	Marker = CreateDefaultSubobject<UBoxComponent>(TEXT("Marker")); Marker->SetupAttachment(SpillRoot);
	for (UPrimitiveComponent* Component : { static_cast<UPrimitiveComponent*>(Body), static_cast<UPrimitiveComponent*>(Marker) })
	{
		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Component->SetGenerateOverlapEvents(false);
	}
	Marker->SetHiddenInGame(true);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(SpillRoot);
	SteamFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("SteamFX")); SteamFX->SetupAttachment(SpillRoot);
	SteamFX->bAutoActivate = false; SteamFX->SetRelativeScale3D(FVector(2.5));
	SteamFX->OnSystemFinished.AddDynamic(this, &ASpill::OnSteamFinished);
}
void ASpill::Initialize(FName InId) { check(HasAuthority()); Id = InId; }
void ASpill::BeginPlay()
{
	Super::BeginPlay();
	Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	if (auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		auto* Dynamic = UMaterialInstanceDynamic::Create(Material, this);
		Dynamic->SetVectorParameterValue(TEXT("Color"), FLinearColor(.01f, .015f, .03f));
		Dynamic->SetScalarParameterValue(TEXT("Roughness"), .15f);
		Body->SetMaterial(0, Dynamic);
	}
	ShipVisuals::ConfigureLabel(Label, 14.f, -90.f, FLinearColor::White, FVector(0, 0, 40));
	if (auto* Steam = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/ModSciInteriors/Particles/PS_SteamDoor.PS_SteamDoor"))) { SteamFX->SetTemplate(Steam); }
	BindState(); OnRep_Id();
	if (!ObservedState.IsValid()) { GetWorldTimerManager().SetTimer(BindTimer, this, &ASpill::BindState, .25f, true); }
}
void ASpill::EndPlay(const EEndPlayReason::Type Reason)
{
	bPuddlePresent = false;
	if (ObservedState.IsValid()) { ObservedState->OnShipGraphChanged.Remove(ChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this); Super::EndPlay(Reason);
}
void ASpill::BindState()
{
	if (ObservedState.IsValid()) { return; }
	if (auto* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		ObservedState = GS; ChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &ASpill::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer); Refresh();
	}
}
void ASpill::OnRep_Id() { Refresh(); }
void ASpill::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(ASpill, Id, COND_InitialOnly);
}
const FFloorSpillState* ASpill::GetSpillState() const
{
	const auto* GS = ObservedState.Get();
	return GS ? GS->FloorHazards.Spills.FindByPredicate([this](const FFloorSpillState& S) { return S.Id == Id; }) : nullptr;
}
void ASpill::Refresh()
{
	const auto* S = GetSpillState();
	SetActorHiddenInGame(!S); SetActorEnableCollision(S != nullptr);
	if (S)
	{
		SetActorLocation(S->Location);
		Body->SetRelativeScale3D(FVector(S->RadiusCm / 50.f, S->RadiusCm / 50.f, .02f));
		Marker->SetBoxExtent(FVector(S->RadiusCm, S->RadiusCm, 30));
		// The crosshair prompt says what E does (M5-5); the label keeps to what the puddle is.
		Label->SetText(FText::FromString(FString::Printf(TEXT("SPILL\n%d L"), FMath::FloorToInt(S->Litres))));
	}
	if (bPuddlePresent != (S != nullptr))
	{
		bPuddlePresent = S != nullptr; GetWorldTimerManager().ClearTimer(SteamRestartTimer);
		if (bPuddlePresent) { SteamFX->Activate(true); } else { SteamFX->DeactivateImmediate(); }
	}
}
void ASpill::OnSteamFinished(UParticleSystemComponent* Finished)
{
	if (bPuddlePresent) { SteamRestartTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ASpill::RestartSteam); }
}
void ASpill::RestartSteam() { if (bPuddlePresent) { SteamFX->Activate(true); } }

FText ASpill::GetInteractPrompt_Implementation() const
{
	return GetSpillState() ? NSLOCTEXT("Airlock", "SqueegeeSpill", "Squeegee the spill (hold E)") : FText::GetEmpty();
}
float ASpill::GetInteractHoldSeconds_Implementation() const
{
	return GetSpillState() ? FFloorHazardRates().SqueegeeSeconds : 0.f;
}
void ASpill::Interact_Implementation(APawn* Pawn) {}
void ASpill::OnHoldComplete_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld()) { return; }
	if (auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>()) { GM->HazardSqueegee(Id, Pawn); }
}
