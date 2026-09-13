// Copyright Airlock project. All Rights Reserved.

#include "Voyage/VoyageSubsystem.h"
#include "Airlock.h"
#include "Core/AirlockSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"

void UVoyageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadSave();
	UE_LOG(LogAirlock, Log, TEXT("VoyageSubsystem initialised: base fuel %d t, runs %d"), GetBaseFuelTonnes(), GetRunsCompleted());
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
	// The refining run in three sizes (M3-7): one ship, more casks, more crew slots.
	Spec.ShipClassId = TEXT("Hauler_Mk1");
	switch (Type)
	{
	case EVoyageType::Standard:	Spec.CaskCount = 8;  Spec.VolatileCount = 4; Spec.CrewCapacity = 4; break;
	case EVoyageType::Priority:	Spec.CaskCount = 12; Spec.VolatileCount = 6; Spec.CrewCapacity = 6; break;
	default:					Spec.CaskCount = 4;  Spec.VolatileCount = 2; Spec.CrewCapacity = 2; break;
	}
	return Spec;
}

TArray<FVoyageSpec> UVoyageSubsystem::GetOfferedVoyages(int32 DaySeed) const
{
	TArray<FVoyageSpec> Offers;
	Offers.Add(MakeVoyageSpec(EVoyageType::MilkRun, DaySeed));
	Offers.Add(MakeVoyageSpec(EVoyageType::Standard, DaySeed));
	Offers.Add(MakeVoyageSpec(EVoyageType::Priority, DaySeed));
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
	return (FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetDays();
}

void UVoyageSubsystem::SetLastVoyageResult(const FVoyageResult& Result)
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && World->GetNetMode() == NM_Client) { return; }
	LastVoyageResult = Result;
	bHasLastVoyageResult = true;
	// The refinery's surplus comes home in the hauler's tanks (M3-7): the base's stock only ever rises.
	if (!Save) { ReloadSave(); }
	Save->BaseFuelTonnes += FMath::Max(0, Result.FuelSurplus);
	++Save->RunsCompleted;
	SaveNow();
	UE_LOG(LogAirlock, Log, TEXT("Base fuel %d t (+%d this run), runs %d"), Save->BaseFuelTonnes, FMath::Max(0, Result.FuelSurplus), Save->RunsCompleted);
}

void UVoyageSubsystem::SetShipResult(const FVoyageResult& Result)
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && World->GetNetMode() == NM_Client) { return; }
	ShipResult = Result;
	bHasShipResult = true;
}

bool UVoyageSubsystem::GetShipResult(FVoyageResult& OutResult) const
{
	if (!bHasShipResult) { return false; }
	OutResult = ShipResult;
	return true;
}

void UVoyageSubsystem::ClearShipResult()
{
	ShipResult = FVoyageResult();
	bHasShipResult = false;
}

void UVoyageSubsystem::ReloadSave()
{
	Save = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		Save = Cast<UAirlockSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	}
	if (!Save) { Save = Cast<UAirlockSaveGame>(UGameplayStatics::CreateSaveGameObject(UAirlockSaveGame::StaticClass())); }
}
bool UVoyageSubsystem::ResetSave()
{
	const bool bDeleted = !UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0) || UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
	Save = Cast<UAirlockSaveGame>(UGameplayStatics::CreateSaveGameObject(UAirlockSaveGame::StaticClass()));
	UE_LOG(LogAirlock, Log, TEXT("Save reset: base fuel %d t, runs %d"), GetBaseFuelTonnes(), GetRunsCompleted());
	return bDeleted && Save != nullptr;
}
int32 UVoyageSubsystem::GetBaseFuelTonnes() const { return Save ? Save->BaseFuelTonnes : UAirlockSaveGame::DefaultBaseFuelTonnes; }
int32 UVoyageSubsystem::GetRunsCompleted() const { return Save ? Save->RunsCompleted : 0; }
void UVoyageSubsystem::SetBaseFuelTonnes(int32 Tonnes)
{
	if (!Save) { ReloadSave(); }
	Save->BaseFuelTonnes = FMath::Max(0, Tonnes);
	SaveNow();
}
void UVoyageSubsystem::SaveNow()
{
	if (Save && !UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0)) { UE_LOG(LogAirlock, Warning, TEXT("Save to slot %s failed"), *SaveSlotName); }
}
bool UVoyageSubsystem::GetLastVoyageResult(FVoyageResult& OutResult) const
{
	if (!bHasLastVoyageResult) { return false; }
	OutResult = LastVoyageResult;
	return true;
}
