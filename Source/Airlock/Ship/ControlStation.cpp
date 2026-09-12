// Copyright Airlock project. All Rights Reserved.
#include "Ship/ControlStation.h"
#include "Airlock.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Core/AirlockCharacter.h"
#include "Net/UnrealNetwork.h"

AControlStation::AControlStation()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	SetRootComponent(StationRoot);
	Marker = CreateDefaultSubobject<UBoxComponent>(TEXT("Marker"));
	Marker->SetupAttachment(StationRoot);
	Marker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Marker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Marker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Marker->SetGenerateOverlapEvents(false);
	Marker->SetHiddenInGame(true);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(StationRoot);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SeatLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SeatLight"));
	SeatLight->SetupAttachment(StationRoot);
	SeatLight->SetIntensityUnits(ELightUnits::Candelas);
	SeatLight->SetCastShadows(false);
}

void AControlStation::Initialize(const FControlStationSpec& InSpec, FName InCaptain)
{
	check(HasAuthority());
	Spec = InSpec;
	Captain = InCaptain;
}

void AControlStation::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Spec();
}

void AControlStation::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		Marker->SetBoxExtent(MarkerExtent);
		Marker->SetRelativeLocation(FVector(0, 0, MarkerHeight));
		Label->SetWorldSize(LabelSize);
		Label->SetTextRenderColor(LabelColour);
		Label->SetRelativeLocation(FVector(0, 0, LabelHeight));
		Label->SetRelativeRotation(FRotator(0, Spec.LabelYaw - Spec.Transform.Rotator().Yaw, 0));
		SeatLight->SetRelativeLocation(FVector(0, 0, LightHeight));
		SeatLight->SetAttenuationRadius(LightRadius);
		SeatLight->SetIntensity(LightIntensity);
		bVisualsReady = true;
	}
	Refresh();
}

void AControlStation::OnRep_Occupant() { Refresh(); }

void AControlStation::Refresh()
{
	if (!bVisualsReady) { return; }
	const FString Who = IsValid(Occupant)
		? FString::Printf(TEXT("%s at the controls"), *Occupant->GetName())
		: FString::Printf(TEXT("Captain %s has the conn"), *Captain.ToString());
	Label->SetText(FText::FromString(FString::Printf(TEXT("CONTROL STATION\n%s"), *Who)));
	SeatLight->SetLightColor(IsValid(Occupant) ? OccupiedColour : FreeColour);
}

bool AControlStation::ServerSeat(APawn* Pawn)
{
	check(HasAuthority());
	if (!IsValid(Pawn)) { return false; }
	if (IsValid(Occupant) && Occupant != Pawn) { return false; }
	Occupant = Pawn;
	ForceNetUpdate();
	Refresh();
	return true;
}

void AControlStation::ServerVacate(APawn* Pawn)
{
	check(HasAuthority());
	if (Occupant != Pawn) { return; }
	Occupant = nullptr;
	ForceNetUpdate();
	Refresh();
}

void AControlStation::Interact_Implementation(APawn* InstigatorPawn)
{
	// Runs on the pressing machine; the character's server RPC decides.
	if (AAirlockCharacter* Crew = Cast<AAirlockCharacter>(InstigatorPawn))
	{
		if (Crew->GetSeatedStation() == this) { Crew->ServerStandUp(); }
		else { Crew->ServerSitAt(this); }
	}
}

FText AControlStation::GetInteractPrompt_Implementation() const
{
	if (IsValid(Occupant))
	{
		return FText::Format(NSLOCTEXT("Airlock", "StationTaken", "{0} is at the controls"), FText::FromString(Occupant->GetName()));
	}
	return NSLOCTEXT("Airlock", "StationFree", "Take the control station (E)");
}

void AControlStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AControlStation, Spec, COND_InitialOnly);
	DOREPLIFETIME(AControlStation, Captain);
	DOREPLIFETIME(AControlStation, Occupant);
}
