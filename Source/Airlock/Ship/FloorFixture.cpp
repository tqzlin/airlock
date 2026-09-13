// Copyright Airlock project. All Rights Reserved.
#include "Ship/FloorFixture.h"
#include "Core/AirlockGameState.h"
#include "Ship/ShipVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AFloorFixture::AFloorFixture()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	FixtureRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FixtureRoot"));
	SetRootComponent(FixtureRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(FixtureRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(FixtureRoot);
}

void AFloorFixture::Initialize(const FFloorFixtureSpec& InSpec)
{
	check(HasAuthority());
	Spec = InSpec;
	Line2Override = InSpec.Line2;
}

void AFloorFixture::SetLine2(const FString& InLine2)
{
	if (!HasAuthority() || Line2Override == InLine2) { return; }
	Line2Override = InLine2;
	ForceNetUpdate();
	OnRep_Line2();
}

void AFloorFixture::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Spec();
	BindState();
	if (!FloorState.IsValid()) { GetWorldTimerManager().SetTimer(BindTimer, this, &AFloorFixture::BindState, .25f, true); }
}

void AFloorFixture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AAirlockGameState* State = FloorState.Get()) { State->OnShipGraphChanged.Remove(StateChangedHandle); }
	FloorState.Reset();
	Super::EndPlay(EndPlayReason);
}

void AFloorFixture::BindState()
{
	if (FloorState.IsValid()) { GetWorldTimerManager().ClearTimer(BindTimer); return; }
	AAirlockGameState* State = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	if (!State) { return; }
	FloorState = State;
	StateChangedHandle = State->OnShipGraphChanged.AddUObject(this, &AFloorFixture::Refresh);
	GetWorldTimerManager().ClearTimer(BindTimer);
	Refresh();
}

void AFloorFixture::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		ShipVisuals::LoadBody(Body, Spec.Mesh, FallbackScale);
		// World centimetres (M5-5): the spec's height and the 22 cm text mean what they say on a scaled fixture too.
		ShipVisuals::ConfigureLabelUnscaled(Label, LabelSize, Spec.LabelYaw, FLinearColor::White, FVector(0, 0, Spec.LabelHeight));
		bVisualsReady = true;
	}
	Refresh();
}

void AFloorFixture::OnRep_Line2() { Refresh(); }

FString AFloorFixture::StateLine(const FFloorFixtureSpec& InSpec, const FRefineryState& Refinery)
{
	if (InSpec.Kind == EFloorFixtureKind::Tank) { return FString::Printf(TEXT("%d L"), FMath::FloorToInt(Refinery.TankDieselL)); }
	if (InSpec.Kind != EFloorFixtureKind::Still) { return FString(); }
	const FStillState* Still = Refinery.Stills.FindByPredicate([&InSpec](const FStillState& S) { return S.Id == InSpec.Id; });
	if (!Still) { return FString(); }
	FString Line;
	switch (Still->Status)
	{
	case EStillStatus::Running:
	{
		const float Progress = FMath::Clamp(1.f - Still->ChargeL / FRefineryRates().CaskLitres, 0.f, 1.f);
		Line = FString::Printf(TEXT("%s %d%% %dK %.1fbar"), *Still->CurrentCask.ToString(), FMath::FloorToInt(Progress * 100.f),
			FMath::FloorToInt(Still->TempK), Still->PressureBar);
		break;
	}
	case EStillStatus::Popped: Line = TEXT("POPPED"); break;
	case EStillStatus::Flash: Line = TEXT("FLASH"); break;
	case EStillStatus::Failed: Line = TEXT("FAILED"); break;
	default:
		Line = Still->Hopper.IsEmpty() ? FString::Printf(TEXT("idle %dK"), FMath::FloorToInt(Still->TempK))
			: FString::Printf(TEXT("idle +%d %dK"), Still->Hopper.Num(), FMath::FloorToInt(Still->TempK));
		break;
	}
	if (Still->bFouled) { Line += TEXT(" fouled"); }
	return Line;
}

FString AFloorFixture::CurrentLabel() const { return Label ? Label->Text.ToString() : FString(); }

void AFloorFixture::Refresh()
{
	if (!bVisualsReady) { return; }
	FString Second;
	if (const AAirlockGameState* State = FloorState.Get()) { if (State->RefineryHasFloor()) { Second = StateLine(Spec, State->Refinery); } }
	if (Second.IsEmpty()) { Second = Line2Override.IsEmpty() ? Spec.Line2 : Line2Override; }
	Label->SetText(FText::FromString(Second.IsEmpty() ? Spec.Line1 : Spec.Line1 + TEXT("\n") + Second));
}

void AFloorFixture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFloorFixture, Spec, COND_InitialOnly);
	DOREPLIFETIME(AFloorFixture, Line2Override);
}
