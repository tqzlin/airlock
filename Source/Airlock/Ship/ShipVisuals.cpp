// Copyright Airlock project. All Rights Reserved.
#include "Ship/ShipVisuals.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

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

void ShipVisuals::ApplyCargoHeat(UStaticMeshComponent* Body, float Heat)
{
	static bool bLoggedSkip = false;
	auto LogSkip = [](const TCHAR* Reason)
	{
		if (!bLoggedSkip) { UE_LOG(LogAirlock, Log, TEXT("Cargo emissive skipped: %s"), Reason); bLoggedSkip = true; }
	};
	if (!Body->GetStaticMesh()) { return; }
	if (Body->GetStaticMesh()->GetPathName() == TEXT("/Engine/BasicShapes/Cube.Cube"))
	{
		LogSkip(TEXT("cube fallback"));
		return;
	}
	UMaterialInterface* Material = Body->GetMaterial(0);
	if (!Material) { return; } // Retry when the body has a material.
	static bool bProbed = false;
	static TArray<FMaterialParameterInfo> HeatScalars;
	static TArray<FMaterialParameterInfo> HeatVectors;
	if (!bProbed)
	{
		TArray<FMaterialParameterInfo> Scalars, Vectors;
		TArray<FGuid> Ids;
		Material->GetAllScalarParameterInfo(Scalars, Ids);
		Material->GetAllVectorParameterInfo(Vectors, Ids);
		TArray<FString> ScalarNames, VectorNames;
		auto IsHeat = [](FName Name) { return Name == TEXT("Emissive Light") || Name == TEXT("Emissive Boost"); };
		for (const auto& Info : Scalars)
		{
			ScalarNames.Add(Info.Name.ToString());
			if (IsHeat(Info.Name)) { HeatScalars.Add(Info); }
		}
		for (const auto& Info : Vectors)
		{
			VectorNames.Add(Info.Name.ToString());
			if (IsHeat(Info.Name)) { HeatVectors.Add(Info); }
		}
		UE_LOG(LogAirlock, Log, TEXT("Cargo material %s parameters: scalar=[%s] vector=[%s]"),
			*Material->GetPathName(), *FString::Join(ScalarNames, TEXT(", ")), *FString::Join(VectorNames, TEXT(", ")));
		bProbed = true;
	}
	if (HeatScalars.IsEmpty() && HeatVectors.IsEmpty())
	{
		LogSkip(TEXT("no Emissive Light or Emissive Boost parameter"));
		return;
	}
	UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
	if (!Dynamic) { Dynamic = Body->CreateDynamicMaterialInstance(0); }
	if (!Dynamic) { return; }
	Heat = FMath::Clamp(Heat, 0.f, 1.f);
	for (const auto& Info : HeatScalars) { Dynamic->SetScalarParameterValueByInfo(Info, Heat); }
	for (const auto& Info : HeatVectors) { Dynamic->SetVectorParameterValueByInfo(Info, FLinearColor(Heat, Heat, Heat)); }
}
