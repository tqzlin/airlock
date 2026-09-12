// Copyright Airlock project. All Rights Reserved.

#include "Ship/AirlockDoor.h"
#include "Airlock.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Kit standard (Docs/KitStandard.md): the actor origin is the centre of the opening on the walking surface.
	const FVector KitDoorwayOffset(0.f, -180.f, 0.f);   // SM_Doorway_Small_A runs 0..360 along +Y
	const FVector KitFrameFrontOffset(0.f, 0.f, 0.f);
	const FVector KitFrameBackOffset(-70.f, 0.f, 0.f);  // second frame on the far face of the 80 cm wall
	const FVector KitLeafOffset(-35.f, 0.f, 0.f);       // leaf centred in the wall thickness
	const FVector VolumeCentre(-35.f, 0.f, 150.f);
	const FVector ProximityExtent(220.f, 80.f, 150.f);  // same reach as the pack's Door_A_BP trigger
	const FVector TransitionExtent(12.f, 80.f, 150.f);

	// Fallback (packs not installed): engine cube pieces with the same clear opening (166 x 275) and wall slot.
	const TCHAR* FallbackCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FVector FallbackLeafOffset(-35.f, 0.f, 137.5f);
}

AAirlockDoor::AAirlockDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;

	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	DoorRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(DoorRoot);

	Doorway = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Doorway"));
	Doorway->SetupAttachment(DoorRoot);
	Doorway->SetRelativeLocation(KitDoorwayOffset);
	Doorway->SetMobility(EComponentMobility::Static);
	Doorway->SetCollisionProfileName(TEXT("BlockAll"));

	FrameFront = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameFront"));
	FrameFront->SetupAttachment(DoorRoot);
	FrameFront->SetRelativeLocation(KitFrameFrontOffset);
	FrameFront->SetMobility(EComponentMobility::Static);
	FrameFront->SetCollisionProfileName(TEXT("NoCollision"));

	FrameBack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameBack"));
	FrameBack->SetupAttachment(DoorRoot);
	FrameBack->SetRelativeLocation(KitFrameBackOffset);
	FrameBack->SetMobility(EComponentMobility::Static);
	FrameBack->SetCollisionProfileName(TEXT("NoCollision"));

	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(DoorRoot);
	Leaf->SetRelativeLocation(KitLeafOffset);
	Leaf->SetMobility(EComponentMobility::Movable);
	Leaf->SetCollisionProfileName(TEXT("BlockAll"));

	LeafGlass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeafGlass"));
	LeafGlass->SetupAttachment(Leaf);
	LeafGlass->SetMobility(EComponentMobility::Movable);
	LeafGlass->SetCollisionProfileName(TEXT("NoCollision"));

	LeafPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeafPanel"));
	LeafPanel->SetupAttachment(Leaf);
	LeafPanel->SetMobility(EComponentMobility::Movable);
	LeafPanel->SetCollisionProfileName(TEXT("NoCollision"));

	ProximityVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ProximityVolume"));
	ProximityVolume->SetupAttachment(DoorRoot);
	ProximityVolume->SetRelativeLocation(VolumeCentre);
	ProximityVolume->InitBoxExtent(ProximityExtent);
	ProximityVolume->SetCollisionProfileName(TEXT("Trigger"));

	TransitionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TransitionVolume"));
	TransitionVolume->SetupAttachment(DoorRoot);
	TransitionVolume->SetRelativeLocation(VolumeCentre);
	TransitionVolume->InitBoxExtent(TransitionExtent);
	TransitionVolume->SetCollisionProfileName(TEXT("Trigger"));

	DoorwayMesh   = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ModSci_Engineer/Meshes/SM_Doorway_Small_A.SM_Doorway_Small_A")));
	FrameMesh     = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ModSci_Engineer/Meshes/SM_Doorway_Small_A3.SM_Doorway_Small_A3")));
	LeafMesh      = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ModSci_Engineer/Meshes/SM_Door_Small_A.SM_Door_Small_A")));
	LeafGlassMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ModSci_Engineer/Meshes/SM_Door_Small_A_Glass.SM_Door_Small_A_Glass")));
	LeafPanelMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/ModSci_Engineer/Meshes/SM_Door_Small_UI.SM_Door_Small_UI")));

	TargetSectionId = TEXT("Unset");
	LeafClosedLocation = KitLeafOffset;
}

void AAirlockDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshes();
}

void AAirlockDoor::BeginPlay()
{
	Super::BeginPlay();
	if (Leaf->GetStaticMesh() == nullptr)
	{
		// Spawned at runtime (placed actors already carry their meshes and static components must not be moved in-game).
		ApplyMeshes();
	}

	ProximityVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &AAirlockDoor::OnProximityBegin);
	ProximityVolume->OnComponentEndOverlap.AddUniqueDynamic(this, &AAirlockDoor::OnProximityEnd);
	TransitionVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &AAirlockDoor::OnTransitionBegin);
}

void AAirlockDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAirlockDoor, bIsOpen);
}

void AAirlockDoor::ApplyMeshes()
{
	auto Load = [](const TSoftObjectPtr<UStaticMesh>& Ptr) -> UStaticMesh*
	{
		return Ptr.IsNull() ? nullptr : Ptr.LoadSynchronous();
	};

	UStaticMesh* DoorwayAsset = Load(DoorwayMesh);
	UStaticMesh* FrameAsset = Load(FrameMesh);
	UStaticMesh* LeafAsset = Load(LeafMesh);
	bUsingFallbackMeshes = (DoorwayAsset == nullptr || LeafAsset == nullptr);

	if (!bUsingFallbackMeshes)
	{
		auto Place = [](UStaticMeshComponent* Comp, const FVector& Location, const FVector& Scale)
		{
			if (!Comp->GetRelativeLocation().Equals(Location) || !Comp->GetRelativeScale3D().Equals(Scale))
			{
				Comp->SetRelativeLocation(Location);
				Comp->SetRelativeScale3D(Scale);
			}
		};
		Doorway->SetStaticMesh(DoorwayAsset);
		Place(Doorway, KitDoorwayOffset, FVector::OneVector);
		Doorway->SetVisibility(true);
		Doorway->SetCollisionProfileName(TEXT("BlockAll"));

		FrameFront->SetStaticMesh(FrameAsset);
		Place(FrameFront, KitFrameFrontOffset, FVector::OneVector);
		FrameBack->SetStaticMesh(FrameAsset);
		Place(FrameBack, KitFrameBackOffset, FVector::OneVector);

		Leaf->SetStaticMesh(LeafAsset);
		Leaf->SetRelativeScale3D(FVector::OneVector);
		LeafGlass->SetStaticMesh(Load(LeafGlassMesh));
		LeafPanel->SetStaticMesh(Load(LeafPanelMesh));
		LeafClosedLocation = KitLeafOffset;
	}
	else
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, FallbackCubePath);

		// No wall slot without the kit: two jambs (97 wide each) leave the standard 166 cm opening in a 360 cm span.
		Doorway->SetStaticMesh(nullptr);
		Doorway->SetVisibility(false);
		Doorway->SetCollisionProfileName(TEXT("NoCollision"));

		FrameFront->SetStaticMesh(Cube);
		FrameFront->SetRelativeLocation(FVector(-35.f, 131.5f, 195.f));
		FrameFront->SetRelativeScale3D(FVector(0.8f, 0.97f, 3.9f));
		FrameFront->SetCollisionProfileName(TEXT("BlockAll"));
		FrameBack->SetStaticMesh(Cube);
		FrameBack->SetRelativeLocation(FVector(-35.f, -131.5f, 195.f));
		FrameBack->SetRelativeScale3D(FVector(0.8f, 0.97f, 3.9f));
		FrameBack->SetCollisionProfileName(TEXT("BlockAll"));

		Leaf->SetStaticMesh(Cube);
		Leaf->SetRelativeScale3D(FVector(0.23f, 1.66f, 2.75f));
		LeafGlass->SetStaticMesh(nullptr);
		LeafPanel->SetStaticMesh(nullptr);
		LeafClosedLocation = FallbackLeafOffset;
	}

	UpdateLeaf();
}

void AAirlockDoor::OnProximityBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority())
	{
		return;
	}
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		PawnsNearby.AddUnique(Pawn);
		SetOpen(true);
	}
}

void AAirlockDoor::OnProximityEnd(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (!HasAuthority())
	{
		return;
	}
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		PawnsNearby.Remove(Pawn);
	}
	PawnsNearby.RemoveAll([](const TWeakObjectPtr<APawn>& P) { return !P.IsValid(); });
	if (PawnsNearby.Num() == 0)
	{
		SetOpen(false);
	}
}

void AAirlockDoor::OnTransitionBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority())
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	UE_LOG(LogAirlock, Log, TEXT("Airlock %s: %s crossed the threshold towards section '%s'"),
		*GetName(), *Pawn->GetName(), *TargetSectionId.ToString());
	OnTransitionRequested.Broadcast(this, Pawn, TargetSectionId);
}

void AAirlockDoor::SetOpen(bool bOpen)
{
	if (bIsOpen != bOpen)
	{
		bIsOpen = bOpen;
		OnRep_IsOpen();
	}
}

void AAirlockDoor::OnRep_IsOpen()
{
	SetActorTickEnabled(true);
}

void AAirlockDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bIsOpen ? 1.f : 0.f;
	SlideAlpha = FMath::FInterpConstantTo(SlideAlpha, Target, DeltaSeconds, 1.f / FMath::Max(OpenTime, 0.05f));
	UpdateLeaf();

	if (FMath::IsNearlyEqual(SlideAlpha, Target))
	{
		SetActorTickEnabled(false);
	}
}

void AAirlockDoor::UpdateLeaf()
{
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, SlideAlpha, 2.f);
	Leaf->SetRelativeLocation(LeafClosedLocation + FVector(0.f, 0.f, OpenDistance * Eased));
}
