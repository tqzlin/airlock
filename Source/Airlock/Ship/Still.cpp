// Copyright Airlock project. All Rights Reserved.
#include "Ship/Still.h"
#include "Core/AirlockGameState.h"
#include "Ship/ShipVisuals.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

AStill::AStill()
{
	BathGauge = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BathGauge")); BathGauge->SetupAttachment(FixtureRoot);
	KnobGauge = CreateDefaultSubobject<UTextRenderComponent>(TEXT("KnobGauge")); KnobGauge->SetupAttachment(FixtureRoot);
	BatchGauge = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BatchGauge")); BatchGauge->SetupAttachment(FixtureRoot);
	PopLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PopLight")); PopLight->SetupAttachment(FixtureRoot);
	PopLight->SetRelativeLocation(FVector(0, 0, 230)); PopLight->SetLightColor(FLinearColor::White);
	PopLight->SetIntensityUnits(ELightUnits::Candelas); PopLight->SetIntensity(300.f); PopLight->SetAttenuationRadius(500.f);
	PopLight->SetCastShadows(false); PopLight->SetVisibility(false);
	PopFX = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("PopFX")); PopFX->SetupAttachment(FixtureRoot);
	PopFX->SetRelativeLocation(FVector(0, 0, 230)); PopFX->SetRelativeScale3D(FVector(2.5)); PopFX->bAutoActivate = false;
}

void AStill::Refresh()
{
	Super::Refresh();
	const auto* GS = FloorState.Get();
	if (!bVisualsReady || !GS || !GS->RefineryHasFloor()) { return; }
	const FStillState* S = GS->Refinery.Stills.FindByPredicate([this](const FStillState& St) { return St.Id == GetFixtureId(); });
	if (!S) { return; }
	if (!bPopVisualsReady)
	{
		if (auto* Steam = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/ModSciInteriors/Particles/PS_SteamDoor.PS_SteamDoor"))) { PopFX->SetTemplate(Steam); }
		bPopVisualsReady = true;
	}
	// A replicated transition triggers the effect on either side; a first Popped snapshot is only a baseline.
	if (bHasLastStatus && LastStatus == EStillStatus::Running && S->Status == EStillStatus::Popped)
	{
		PopLight->SetVisibility(true); PopFX->Activate(true);
		GetWorldTimerManager().SetTimer(PopTimer, this, &AStill::EndPopBurst, .5f, false);
	}
	LastStatus = S->Status; bHasLastStatus = true;
	const FRefineryRates Rates;
	ShipVisuals::ConfigureLabel(BathGauge, 18.f, GetSpec().LabelYaw,
		S->PressureBar >= Rates.WarnBar ? FLinearColor::Red : FLinearColor::White, FVector(0, -125, 215));
	ShipVisuals::ConfigureLabel(KnobGauge, 18.f, GetSpec().LabelYaw, FLinearColor::White, FVector(0, -125, 195));
	ShipVisuals::ConfigureLabel(BatchGauge, 18.f, GetSpec().LabelYaw,
		S->Calibration < Rates.CalWarn ? FLinearColor(1.f, .65f, .1f) : FLinearColor::White, FVector(0, -125, 175));
	BathGauge->SetText(FText::FromString(FString::Printf(TEXT("T %d K   P %.2f bar"), FMath::FloorToInt(S->TempK), S->PressureBar)));
	KnobGauge->SetText(FText::FromString(FString::Printf(TEXT("heat %d%%  feed %d%%  vent %d%%"), S->HeatSteps * 5, S->FeedSteps * 5, S->VentSteps * 5)));
	FString Batch;
	if (S->Status == EStillStatus::Running)
	{
		const float Progress = FMath::Clamp(1.f - S->ChargeL / Rates.CaskLitres, 0.f, 1.f);
		Batch = FString::Printf(TEXT("%s %d%%"), *S->CurrentCask.ToString(), FMath::FloorToInt(Progress * 100.f));
	}
	else { Batch = S->Status == EStillStatus::Idle ? TEXT("idle") : FString(RefineryLayouts::StatusName(S->Status)).ToUpper(); }
	BatchGauge->SetText(FText::FromString(FString::Printf(TEXT("%s   cal %d%%"), *Batch, FMath::FloorToInt(S->Calibration * 100.f))));
}

void AStill::EndPopBurst() { PopLight->SetVisibility(false); PopFX->Deactivate(); }
void AStill::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(PopTimer); Super::EndPlay(Reason);
}
