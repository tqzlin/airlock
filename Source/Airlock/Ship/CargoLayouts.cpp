// Copyright Airlock project. All Rights Reserved.
#include "Ship/CargoLayouts.h"

const FCargoLayout* CargoLayouts::Find(FName SectionId)
{
	static const FCargoLayout Hauler = []
	{
		FCargoLayout Layout;
		Layout.SectionId = TEXT("Hauler_Mk1");
		// Probed with a falling pawn: these centres settle on the west hall's Z -65 floor.
		const double ClampX[] = { -6200, -5700, -5200, -5000 };
		const double CrateX[] = { -6100, -5600, -5300, -5000 };
		for (int32 I = 0; I < 4; ++I)
		{
			FCargoSlotSpec Slot;
			Slot.Id = I == 3 ? FName(TEXT("ColdRack")) : FName(*FString::Printf(TEXT("Clamp_%d"), I + 1));
			Slot.bRack = I == 3;
			// Shelf pivot is at a corner: yaw 90 extends +182 along Y. Keep its far edge south of the walkway.
			Slot.Transform = FTransform(FRotator(0, Slot.bRack ? 90 : 0, 0), FVector(ClampX[I], Slot.bRack ? 1300 : 1400, -65));
			Slot.Mesh = FSoftObjectPath(Slot.bRack
				? TEXT("/Game/StarterBundle/ModularScifiProps/Meshes/SM_Shelf_A_v1.SM_Shelf_A_v1")
				: TEXT("/Game/ModSci_EngiProps/Meshes/SM_PlasticPallet.SM_PlasticPallet"));
			Layout.Slots.Add(Slot);
			FCargoCrateSpec Crate;
			Crate.Id = FName(*FString::Printf(TEXT("C%d"), I + 1));
			Crate.bVolatile = I == 1 || I == 3;
			Crate.Transform = FTransform(FVector(CrateX[I], 1700, -65));
			// Measured 120.49 x 67.55 x 51.00 cm; fits the crate-size contract.
			Crate.Mesh = FSoftObjectPath(TEXT("/Game/ModSci_EngiProps/Meshes/SM_Case_A.SM_Case_A"));
			Layout.Crates.Add(Crate);
		}
		return Layout;
	}();
	return SectionId == Hauler.SectionId ? &Hauler : nullptr;
}
