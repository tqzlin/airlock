// Copyright Airlock project. All Rights Reserved.

#include "Station/VoyageBoard.h"
#include "Airlock.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/AirlockPlayerController.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"

namespace
{
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FVector PanelLocation(0.f, 0.f, 160.f);      // SM_WVD_Panel_A is 120 wide x 22 thick x 78 tall, centred
	const FRotator PanelRotation(0.f, 90.f, 0.f);      // panel's local -Y face -> world +X
	const FVector StandLocation(0.f, 0.f, 60.f);
	const FVector StandScale(0.12f, 0.12f, 1.2f);
}

AVoyageBoard::AVoyageBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // purely local UI trigger; the booking itself goes through a server RPC on the controller

	BoardRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoardRoot"));
	SetRootComponent(BoardRoot);

	Stand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stand"));
	Stand->SetupAttachment(BoardRoot);
	Stand->SetRelativeLocation(StandLocation);
	Stand->SetRelativeScale3D(StandScale);
	Stand->SetCollisionProfileName(TEXT("BlockAll"));

	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(BoardRoot);
	Panel->SetRelativeLocation(PanelLocation);
	Panel->SetRelativeRotation(PanelRotation);
	Panel->SetCollisionProfileName(TEXT("BlockAll"));

	PanelMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ModSciInteriors/Meshes/SM_WVD_Panel_A.SM_WVD_Panel_A")));
}

void AVoyageBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void AVoyageBoard::BeginPlay()
{
	Super::BeginPlay();
	if (Panel->GetStaticMesh() == nullptr)
	{
		ApplyMeshes();
	}
}

void AVoyageBoard::ApplyMeshes()
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	Stand->SetStaticMesh(Cube);

	UStaticMesh* PanelAsset = PanelMesh.IsNull() ? nullptr : PanelMesh.LoadSynchronous();
	if (PanelAsset)
	{
		Panel->SetStaticMesh(PanelAsset);
		Panel->SetRelativeScale3D(FVector::OneVector);
	}
	else
	{
		Panel->SetStaticMesh(Cube);
		Panel->SetRelativeScale3D(FVector(1.2f, 0.1f, 0.8f)); // 120 x 10 x 80 slab
	}
}

void AVoyageBoard::Interact_Implementation(APawn* InstigatorPawn)
{
	AAirlockPlayerController* PC = InstigatorPawn ? Cast<AAirlockPlayerController>(InstigatorPawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	UE_LOG(LogAirlock, Log, TEXT("VoyageBoard %s used by %s"), *GetName(), *InstigatorPawn->GetName());
	PC->OpenVoyageBoard();
}

FText AVoyageBoard::GetInteractPrompt_Implementation() const
{
	return NSLOCTEXT("Airlock", "VoyageBoardPrompt", "View departures");
}
