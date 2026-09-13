// Copyright Airlock project. All Rights Reserved.

#include "Voyage/VoyageSchedule.h"

TArray<FVoyageAnnouncement> FVoyageSchedule::Build(const FVoyageSpec& Spec)
{
	const FText Captain = FText::FromName(Spec.Captain);
	FText MidVoyage;
	switch (Spec.Type)
	{
	case EVoyageType::Standard:
		MidVoyage = NSLOCTEXT("Airlock", "CaptainStandard", "Captain {0} here. Halfway to the refinery. Check the rack, check the air.");
		break;
	case EVoyageType::Priority:
		MidVoyage = NSLOCTEXT("Airlock", "CaptainPriority", "Captain {0} here. Priority manifest - twelve casks and the refinery's counting. Nobody sleeps.");
		break;
	default:
		MidVoyage = NSLOCTEXT("Airlock", "CaptainMilkRun", "Captain {0} here. Light load today. Keep the casks cold and we're home for supper.");
		break;
	}

	TArray<FVoyageAnnouncement> Entries = {
		{ 0.f, FText::Format(NSLOCTEXT("Airlock", "CaptainDeparture", "Captain {0}, MV Corrin. We're underway. Sludge casks are loose on the deck - clamp them, or rack them, the rack's cold."), Captain) },
		{ Spec.DurationSeconds * 0.5f, FText::Format(MidVoyage, Captain) },
		{ FMath::Max(0.f, Spec.DurationSeconds - 60.f), FText::Format(NSLOCTEXT("Airlock", "CaptainOneMinute", "Captain {0} here. One minute to dock. Secure your gear."), Captain) },
		{ Spec.DurationSeconds, FText::Format(NSLOCTEXT("Airlock", "CaptainDocking", "Captain {0} here. Docking at the refinery. Their crew's coming aboard - then home."), Captain) }
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
