// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "Airlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

class UTextRenderComponent;
class UMaterialInterface;

namespace ShipVisuals
{
/**
 * A world-facing, centred label. The offset stays in the owner's frame. Since M5-5 every label wears the engine's
 * unlit text material (`/Engine/EngineMaterials/UnlitText`), so it reads the same white from the dark side of a
 * still as under a work light; the default lit material left the floor's labels grey (the default stays when the
 * engine material fails to load).
 */
AIRLOCK_API void ConfigureLabel(UTextRenderComponent* Label, float WorldSize, float FacingYaw,
	FLinearColor Colour, FVector RelativeOffset);

/**
 * The same policy in world centimetres for a label on a scaled owner (a still at 0.8, a valve at 0.3): the text
 * size and the offset are what the crew measures, whatever the actor's scale (M5-5; the still controls' rule).
 */
AIRLOCK_API void ConfigureLabelUnscaled(UTextRenderComponent* Label, float WorldSize, float FacingYaw,
	FLinearColor Colour, FVector WorldOffset);

/** The unlit text material every label wears; null when the engine asset is missing (then the default stays). */
AIRLOCK_API UMaterialInterface* LabelMaterial();

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
