// Copyright Airlock project. All Rights Reserved.
#include "Voyage/VoyageSetPieces.h"
TArray<FVoyageSetPiece> FVoyageSetPieces::Build(const FVoyageSpec& Spec)
{
	FRandomStream Random(Spec.Seed ^ 0x5E7B1ECE);
	FVoyageSetPiece Entry;
	Entry.AtSeconds = Spec.DurationSeconds * Random.FRandRange(.40f, .55f);
	const FText Causes[] = {
		NSLOCTEXT("Airlock", "FuelValveFailed", "Captain {0} here. Feed valve's failed open - we're bleeding methane. Engineering, shut it."),
		NSLOCTEXT("Airlock", "FuelLineSplit", "Captain {0} here. Fuel line's split aft of the tank. Shut the feed, now."),
		NSLOCTEXT("Airlock", "FuelShortFilled", "Captain {0} here. The Rig short-filled us and now the feed's stuck open. Shut it before we lose the rest.")
	};
	Entry.CaptainLine = FText::Format(Causes[Random.RandRange(0,2)], FText::FromName(Spec.Captain));
	return { Entry };
}
