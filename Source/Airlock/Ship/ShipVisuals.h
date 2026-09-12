// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "Airlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace ShipVisuals
{
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
