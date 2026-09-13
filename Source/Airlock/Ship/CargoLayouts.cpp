// Copyright Airlock project. All Rights Reserved.
#include "Ship/CargoLayouts.h"

namespace
{
	// The hold's spots (M3-4 probed the first four crates and three clamps; M3-7 added rows for the bigger
	// contracts). Every centre must settle on the west hall's Z -65 floor, off the Y 1560 walkway and clear of the
	// clamp controller at (-6300, 1900) - `verify-refinery.ps1 -ProbePositions` checks them all.
	const FVector2D CrateSpots[12] = {
		{ -6100, 1700 }, { -5600, 1700 }, { -5300, 1700 }, { -5000, 1700 },
		{ -5850, 1850 }, { -5450, 1850 }, { -5150, 1850 }, { -4800, 1850 },
		{ -6100, 2000 }, { -5700, 2000 }, { -5300, 2000 }, { -4900, 2000 } };
	const FVector2D ClampSpots[9] = {
		{ -6200, 1400 }, { -5700, 1400 }, { -5200, 1400 }, { -4700, 1400 },
		{ -6200, 1250 }, { -5825, 1250 }, { -5450, 1250 }, { -5075, 1250 }, { -4700, 1250 } };
	const TCHAR* PalletMesh = TEXT("/Game/ModSci_EngiProps/Meshes/SM_PlasticPallet.SM_PlasticPallet");
	const TCHAR* ShelfMesh = TEXT("/Game/StarterBundle/ModularScifiProps/Meshes/SM_Shelf_A_v1.SM_Shelf_A_v1");
	const TCHAR* CaseMesh = TEXT("/Game/ModSci_EngiProps/Meshes/SM_Case_A.SM_Case_A"); // measured 120.49 x 67.55 x 51.00 cm
}

FCargoLayout CargoLayouts::Build(FName SectionId, int32 CaskCount, int32 VolatileCount)
{
	FCargoLayout Layout;
	if (SectionId != TEXT("Hauler_Mk1")) { return Layout; }
	Layout.SectionId = SectionId;
	const int32 Casks = FMath::Clamp(CaskCount, 1, static_cast<int32>(UE_ARRAY_COUNT(CrateSpots)));
	const int32 Clamps = FMath::Clamp(Casks * 3 / 4, 1, static_cast<int32>(UE_ARRAY_COUNT(ClampSpots)));
	for (int32 I = 0; I < Clamps; ++I)
	{
		FCargoSlotSpec Slot;
		Slot.Id = FName(*FString::Printf(TEXT("Clamp_%d"), I + 1));
		Slot.Transform = FTransform(FVector(ClampSpots[I].X, ClampSpots[I].Y, -65));
		Slot.Mesh = FSoftObjectPath(PalletMesh);
		Layout.Slots.Add(Slot);
	}
	FCargoSlotSpec Rack;
	Rack.Id = TEXT("ColdRack");
	Rack.bRack = true;
	// Shelf pivot is at a corner: yaw 90 extends +182 along Y. Keep its far edge south of the walkway.
	Rack.Transform = FTransform(FRotator(0, 90, 0), FVector(-5000, 1300, -65));
	Rack.Mesh = FSoftObjectPath(ShelfMesh);
	Layout.Slots.Add(Rack);
	int32 Volatile = 0;
	for (int32 I = 0; I < Casks; ++I)
	{
		FCargoCrateSpec Crate;
		Crate.Id = FName(*FString::Printf(TEXT("C%d"), I + 1));
		// Every second cask is volatile until the contract's count is reached (C2, C4, ... - today's pattern).
		Crate.bVolatile = (I % 2 == 1) && Volatile < VolatileCount;
		Volatile += Crate.bVolatile;
		Crate.Transform = FTransform(FVector(CrateSpots[I].X, CrateSpots[I].Y, -65));
		Crate.Mesh = FSoftObjectPath(CaseMesh);
		Layout.Crates.Add(Crate);
	}
	return Layout;
}

const FCargoLayout* CargoLayouts::Find(FName SectionId)
{
	// The solo manifest of the design's 8.1 (four casks, two volatile, three clamps): what every proof books.
	static const FCargoLayout Hauler = Build(TEXT("Hauler_Mk1"), 4, 2);
	return SectionId == Hauler.SectionId ? &Hauler : nullptr;
}
