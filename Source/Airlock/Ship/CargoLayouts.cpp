// Copyright Airlock project. All Rights Reserved.
#include "Ship/CargoLayouts.h"

const FCargoLayout* CargoLayouts::Find(FName SectionId)
{
	static const FCargoLayout Hauler = []
	{
		FCargoLayout Layout;
		Layout.SectionId = TEXT("Hauler_Mk1");
		for (int32 I = 0; I < 4; ++I)
		{
			FCargoSlotSpec Slot;
			Slot.Id = I == 3 ? FName(TEXT("ColdRack")) : FName(*FString::Printf(TEXT("Clamp_%d"), I + 1));
			Slot.bRack = I == 3;
			Slot.Transform = FTransform(FRotator(0, Slot.bRack ? 90 : 0, 0), FVector(-6100 + I * 400, 1400, -65));
			Slot.Mesh = FSoftObjectPath(Slot.bRack
				? TEXT("/Game/StarterBundle/ModularScifiProps/Meshes/SM_Shelf_A_v1.SM_Shelf_A_v1")
				: TEXT("/Game/ModSci_EngiProps/Meshes/SM_PlasticPallet.SM_PlasticPallet"));
			Layout.Slots.Add(Slot);
			FCargoCrateSpec Crate;
			Crate.Id = FName(*FString::Printf(TEXT("C%d"), I + 1));
			Crate.bVolatile = I == 1 || I == 3;
			Crate.Transform = FTransform(FVector(-6000 + I * 400, 1700, -65));
			// Measured 120.49 x 67.55 x 51.00 cm; fits the crate-size contract.
			Crate.Mesh = FSoftObjectPath(TEXT("/Game/ModSci_EngiProps/Meshes/SM_Case_A.SM_Case_A"));
			Layout.Crates.Add(Crate);
		}
		return Layout;
	}();
	return SectionId == Hauler.SectionId ? &Hauler : nullptr;
}
