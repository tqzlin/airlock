// Copyright Airlock project. All Rights Reserved.

#include "Voyage/VoyageSubsystem.h"
#include "Airlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"

void UVoyageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogAirlock, Log, TEXT("VoyageSubsystem initialised"));
}

FVoyageSpec UVoyageSubsystem::MakeVoyageSpec(EVoyageType Type, int32 Seed)
{
	FVoyageSpec Spec;
	Spec.Type = Type;
	Spec.Seed = Seed;
	Spec.DurationSeconds = 1200.f;

	// Fold the type into the seed so the three offers of one day differ from each other.
	const uint32 Mixed = HashCombine(GetTypeHash(Seed), GetTypeHash(static_cast<uint8>(Type)));
	FRandomStream Rng(static_cast<int32>(Mixed));

	static const TCHAR* Captains[] = { TEXT("Okonkwo"), TEXT("Varga"), TEXT("Lindqvist"), TEXT("Haddad"), TEXT("Reyes"), TEXT("Tanaka") };
	Spec.Captain = FName(Captains[Rng.RandRange(0, UE_ARRAY_COUNT(Captains) - 1)]);
	Spec.CrewCapacity = Rng.RandRange(4, 8);

	switch (Type)
	{
	case EVoyageType::Mining:	Spec.ShipClassId = TEXT("Hauler_Mk1");	break;
	case EVoyageType::Pirates:	Spec.ShipClassId = TEXT("Corvette_Mk1");	break;
	case EVoyageType::Derelict:	Spec.ShipClassId = TEXT("Salvage_Mk1");	break;
	default:					Spec.ShipClassId = TEXT("Hauler_Mk1");	break;
	}
	return Spec;
}

TArray<FVoyageSpec> UVoyageSubsystem::GetOfferedVoyages(int32 DaySeed) const
{
	TArray<FVoyageSpec> Offers;
	Offers.Add(MakeVoyageSpec(EVoyageType::Mining, DaySeed));
	Offers.Add(MakeVoyageSpec(EVoyageType::Pirates, DaySeed));
	Offers.Add(MakeVoyageSpec(EVoyageType::Derelict, DaySeed));
	return Offers;
}

bool UVoyageSubsystem::RequestVoyage(const FVoyageSpec& Spec, FGuid& OutVoyageId)
{
	OutVoyageId.Invalidate();

	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (World && World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAirlock, Warning, TEXT("RequestVoyage called on a client - ignored"));
		return false;
	}

	OutVoyageId = FGuid::NewGuid();
	ActiveVoyages.Add(OutVoyageId, Spec);
	PendingVoyageId = OutVoyageId; // MVP: one crew, one voyage at a time
	UE_LOG(LogAirlock, Log, TEXT("Voyage %s requested: type=%d ship=%s captain=%s crew=%d duration=%.0fs (active=%d)"),
		*OutVoyageId.ToString(), static_cast<int32>(Spec.Type), *Spec.ShipClassId.ToString(), *Spec.Captain.ToString(),
		Spec.CrewCapacity, Spec.DurationSeconds, ActiveVoyages.Num());
	return true;
}

bool UVoyageSubsystem::GetPendingVoyage(FVoyageSpec& OutSpec) const
{
	if (const FVoyageSpec* Found = ActiveVoyages.Find(PendingVoyageId))
	{
		OutSpec = *Found;
		return true;
	}
	return false;
}

void UVoyageSubsystem::ClearPendingVoyage()
{
	if (PendingVoyageId.IsValid())
	{
		UE_LOG(LogAirlock, Log, TEXT("Voyage %s complete"), *PendingVoyageId.ToString());
		ActiveVoyages.Remove(PendingVoyageId);
		PendingVoyageId.Invalidate();
	}
}

int32 UVoyageSubsystem::TodaySeed()
{
	return static_cast<int32>(FDateTime::UtcNow().GetDate().GetTicks() / ETimespan::TicksPerDay);
}
