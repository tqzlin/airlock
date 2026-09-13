// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Voyage/VoyageSchedule.h"

/** The refinery floor's slot: its length and Okafor's timed lines, pure like FVoyageSchedule (M5-2). */
struct AIRLOCK_API FFloorSchedule
{
	/** Ten minutes on the floor (design note section 7). */
	static constexpr float SlotSeconds = 600.f;
	/** The floor boss's name, as the log and the caption print it. */
	static const TCHAR* Speaker();
	/** Okafor at 0, 300, 540 and 600 s, stable-sorted like the captain's schedule. */
	static TArray<FVoyageAnnouncement> Build();
};
