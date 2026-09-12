// Copyright Airlock project. All Rights Reserved.

#include "Voyage/VoyageSchedule.h"

TArray<FVoyageAnnouncement> FVoyageSchedule::Build(const FVoyageSpec& Spec)
{
	const FText Captain = FText::FromName(Spec.Captain);
	FText MidVoyage;
	switch (Spec.Type)
	{
	case EVoyageType::Pirates:
		MidVoyage = NSLOCTEXT("Airlock", "CaptainPirates", "Captain {0} here. Pirate contacts ahead. Keep clear of the weapons deck.");
		break;
	case EVoyageType::Derelict:
		MidVoyage = NSLOCTEXT("Airlock", "CaptainDerelict", "Captain {0} here. Derelict in range. Salvage crew, check your seals.");
		break;
	default:
		MidVoyage = NSLOCTEXT("Airlock", "CaptainMining", "Captain {0} here. We're in the ore field. Mining crew, prepare the holds.");
		break;
	}

	TArray<FVoyageAnnouncement> Entries = {
		{ 0.f, FText::Format(NSLOCTEXT("Airlock", "CaptainDeparture", "Captain {0} here. We're underway. Welcome aboard, crew."), Captain) },
		{ Spec.DurationSeconds * 0.5f, FText::Format(MidVoyage, Captain) },
		{ FMath::Max(0.f, Spec.DurationSeconds - 60.f), FText::Format(NSLOCTEXT("Airlock", "CaptainOneMinute", "Captain {0} here. One minute to dock. Secure your gear."), Captain) },
		{ Spec.DurationSeconds, FText::Format(NSLOCTEXT("Airlock", "CaptainDocking", "Captain {0} here. Docking now. Welcome back to the station."), Captain) }
	};
	Entries.StableSort([](const FVoyageAnnouncement& A, const FVoyageAnnouncement& B)
	{
		return A.AtSeconds < B.AtSeconds;
	});
	return Entries;
}

int32 FVoyageSchedule::FindNextUnfiredEntry(const TArray<FVoyageAnnouncement>& Entries, int32 NextEntry, double ElapsedSeconds)
{
	while (Entries.IsValidIndex(NextEntry) && Entries[NextEntry].AtSeconds <= ElapsedSeconds)
	{
		++NextEntry;
	}
	return NextEntry;
}

bool FVoyageSchedule::TryClampRemainingSeconds(float RequestedSeconds, float DurationSeconds, float& OutSeconds)
{
	if (!FMath::IsFinite(RequestedSeconds))
	{
		return false;
	}
	OutSeconds = FMath::Clamp(RequestedSeconds, 0.f, DurationSeconds);
	return true;
}
