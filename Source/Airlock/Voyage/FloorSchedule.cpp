// Copyright Airlock project. All Rights Reserved.
#include "Voyage/FloorSchedule.h"

const TCHAR* FFloorSchedule::Speaker() { return TEXT("Okafor"); }

TArray<FVoyageAnnouncement> FFloorSchedule::Build()
{
	TArray<FVoyageAnnouncement> Entries = {
		{ 0.f, NSLOCTEXT("Airlock", "FloorStart", "Okafor, floor three. Your casks are on the pallets. Ten minutes on the slot - make them count.") },
		{ SlotSeconds * 0.5f, NSLOCTEXT("Airlock", "FloorHalf", "Halfway. Anything still on a pallet at the horn goes home as crude.") },
		{ SlotSeconds - 60.f, NSLOCTEXT("Airlock", "FloorMinute", "One minute.") },
		{ SlotSeconds, NSLOCTEXT("Airlock", "FloorHorn", "Horn. Slot's over - clear the floor.") },
	};
	Entries.StableSort([](const FVoyageAnnouncement& A, const FVoyageAnnouncement& B) { return A.AtSeconds < B.AtSeconds; });
	return Entries;
}
