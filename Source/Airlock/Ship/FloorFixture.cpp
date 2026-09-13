// Copyright Airlock project. All Rights Reserved.
#include "Ship/FloorFixture.h"
#include "Ship/ShipVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"

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
}

void AFloorFixture::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		ShipVisuals::LoadBody(Body, Spec.Mesh, FallbackScale);
		ShipVisuals::ConfigureLabel(Label, LabelSize, Spec.LabelYaw, FLinearColor::White, FVector(0, 0, Spec.LabelHeight));
		bVisualsReady = true;
	}
	Refresh();
}

void AFloorFixture::OnRep_Line2() { Refresh(); }

void AFloorFixture::Refresh()
{
	if (!bVisualsReady) { return; }
	const FString Second = Line2Override.IsEmpty() ? Spec.Line2 : Line2Override;
	Label->SetText(FText::FromString(Second.IsEmpty() ? Spec.Line1 : Spec.Line1 + TEXT("\n") + Second));
}

void AFloorFixture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFloorFixture, Spec, COND_InitialOnly);
	DOREPLIFETIME(AFloorFixture, Line2Override);
}
