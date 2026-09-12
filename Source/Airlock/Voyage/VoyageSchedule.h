// Copyright Airlock project. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Voyage/VoyageTypes.h"

/** One captain transmission, timed from departure. */
struct AIRLOCK_API FVoyageAnnouncement
{
	float AtSeconds;
	FText Line;
};

/** Pure schedule and clock-move rules, shared by the server and automation tests. */
struct AIRLOCK_API FVoyageSchedule
{
	/** Equal timestamps preserve departure / mid-voyage / one-minute / docking list order. */
	static TArray<FVoyageAnnouncement> Build(const FVoyageSpec& Spec);

	/** Advance a cursor past due entries; moving time backwards never replays a fired line. */
	static int32 FindNextUnfiredEntry(const TArray<FVoyageAnnouncement>& Entries, int32 NextEntry, double ElapsedSeconds);

	/** Reject non-finite requests without changing OutSeconds; otherwise clamp to the voyage duration. */
	static bool TryClampRemainingSeconds(float RequestedSeconds, float DurationSeconds, float& OutSeconds);
};
