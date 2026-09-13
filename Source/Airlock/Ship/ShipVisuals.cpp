// Copyright Airlock project. All Rights Reserved.
#include "Ship/ShipVisuals.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Actor.h"

void ShipVisuals::ConfigureLabel(UTextRenderComponent* Label, float WorldSize, float FacingYaw,
	FLinearColor Colour, FVector RelativeOffset)
{
	Label->SetWorldSize(WorldSize);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetRelativeLocation(RelativeOffset);
	const float OwnerYaw = Label->GetOwner() ? Label->GetOwner()->GetActorRotation().Yaw : 0.f;
	Label->SetRelativeRotation(FRotator(0, FacingYaw - OwnerYaw, 0));
	Label->SetTextRenderColor(Colour.ToFColor(true));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
