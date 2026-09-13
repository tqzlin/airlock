// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "Airlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

class UTextRenderComponent;

namespace ShipVisuals
{
/** A world-facing, centred label. The offset stays in the owner's frame. */
AIRLOCK_API void ConfigureLabel(UTextRenderComponent* Label, float WorldSize, float FacingYaw,
	FLinearColor Colour, FVector RelativeOffset);

/** Optional case emissive. Probes the case material once per process; never edits the source material. */
AIRLOCK_API void ApplyCargoHeat(UStaticMeshComponent* Body, float Heat);

inline void LoadBody(UStaticMeshComponent* Body, const FSoftObjectPath& Path, const FVector& FallbackScale)
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(Path.TryLoad());
	if (!Mesh)
	{
		UE_LOG(LogAirlock, Warning, TEXT("Ship mesh %s failed to load; using cube"), *Path.ToString());
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Body->SetRelativeScale3D(FallbackScale);
		Body->SetRelativeLocation(FVector(0, 0, 50.f * FallbackScale.Z));
	}
	Body->SetStaticMesh(Mesh);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetGenerateOverlapEvents(false);
}
}
