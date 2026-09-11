// Copyright Airlock project. All Rights Reserved.

#include "Ship/AirlockDoor.h"
#include "Airlock.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

AAirlockDoor::AAirlockDoor()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->InitBoxExtent(FVector(60.f, 110.f, 130.f));
	TriggerVolume->SetCollisionProfileName(TEXT("Trigger"));
	SetRootComponent(TriggerVolume);

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(TriggerVolume);
	Frame->SetCollisionProfileName(TEXT("NoCollision"));
	Frame->SetRelativeScale3D(FVector(0.2f, 2.4f, 2.8f)); // a thin slab standing in for the door frame
	Frame->SetRelativeLocation(FVector(0.f, 0.f, 10.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Frame->SetStaticMesh(CubeMesh.Object);
	}

	TargetSectionId = TEXT("Unset");
}

void AAirlockDoor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	UE_LOG(LogAirlock, Log, TEXT("Airlock %s: %s requested transition to section '%s'"),
		*GetName(), *Pawn->GetName(), *TargetSectionId.ToString());
	OnTransitionRequested.Broadcast(Pawn, TargetSectionId);
}
