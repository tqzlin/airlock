// Copyright Airlock project. All Rights Reserved.
#include "Ship/FloorHazards.h"

bool FFloorSpillState::operator==(const FFloorSpillState& Other) const
{
	return Id == Other.Id && StillId == Other.StillId && Location.Equals(Other.Location, 0.f) && Litres == Other.Litres && RadiusCm == Other.RadiusCm;
}

bool FFloorHazardState::operator==(const FFloorHazardState& Other) const
{
	return Spills == Other.Spills && SpillsTotal == Other.SpillsTotal && LitresSpilled == Other.LitresSpilled
		&& LitresCleared == Other.LitresCleared && Stumbles == Other.Stumbles;
}

void FFloorHazards::Reset()
{
	Floor = FFloorHazardState();
	NextId = 1;
}

float FFloorHazards::RadiusFor(float Litres) const
{
	if (!FMath::IsFinite(Litres) || Litres <= 0.f) { return 0.f; }
	return FMath::Min(Rates.RadiusMaxCm, Rates.RadiusBaseCm + Rates.RadiusPerLitre * Litres);
}

FFloorSpillState* FFloorHazards::MutableSpill(FName Id)
{
	return Floor.Spills.FindByPredicate([Id](const FFloorSpillState& S) { return S.Id == Id; });
}

const FFloorSpillState* FFloorHazards::Find(FName Id) const
{
	return Floor.Spills.FindByPredicate([Id](const FFloorSpillState& S) { return S.Id == Id; });
}

FName FFloorHazards::Spill(FName StillId, const FVector& Location, float Litres)
{
	if (!FMath::IsFinite(Litres) || Litres <= 0.f || Location.ContainsNaN()) { return NAME_None; }
	++Floor.SpillsTotal;
	Floor.LitresSpilled += Litres;
	// The nearest puddle within reach takes the litres; the centre stays where the first spill landed.
	FFloorSpillState* Nearest = nullptr;
	float NearestDistance = Rates.MergeDistanceCm;
	for (FFloorSpillState& S : Floor.Spills)
	{
		const float Distance = FVector::Dist2D(S.Location, Location);
		if (Distance <= NearestDistance && FMath::Abs(S.Location.Z - Location.Z) <= Rates.ZToleranceCm) { Nearest = &S; NearestDistance = Distance; }
	}
	if (Nearest)
	{
		Nearest->Litres += Litres;
		Nearest->RadiusCm = RadiusFor(Nearest->Litres);
		return Nearest->Id;
	}
	FFloorSpillState S;
	S.Id = FName(*FString::Printf(TEXT("S%d"), NextId++));
	S.StillId = StillId;
	S.Location = Location;
	S.Litres = Litres;
	S.RadiusCm = RadiusFor(Litres);
	Floor.Spills.Add(S);
	return S.Id;
}

bool FFloorHazards::Tick(float DeltaSeconds, float& OutPpm)
{
	OutPpm = 0.f;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f || Floor.Spills.IsEmpty()) { return false; }
	bool bChanged = false;
	float Evaporated = 0.f;
	for (int32 I = Floor.Spills.Num() - 1; I >= 0; --I)
	{
		FFloorSpillState& S = Floor.Spills[I];
		const float Gone = FMath::Min(S.Litres, Rates.EvaporationLps * DeltaSeconds);
		const float LitresBefore = Published(S.Litres);
		const float RadiusBefore = Published(S.RadiusCm);
		S.Litres -= Gone;
		Evaporated += Gone;
		if (S.Litres <= KINDA_SMALL_NUMBER)
		{
			Floor.Spills.RemoveAt(I);
			bChanged = true;
			continue;
		}
		S.RadiusCm = RadiusFor(S.Litres);
		bChanged |= Published(S.Litres) != LitresBefore || Published(S.RadiusCm) != RadiusBefore;
	}
	OutPpm = Evaporated * Rates.PpmPerLitreEvaporated;
	return bChanged;
}

bool FFloorHazards::Squeegee(FName Id)
{
	FFloorSpillState* S = MutableSpill(Id);
	if (!S) { return false; }
	const float Taken = FMath::Min(S->Litres, Rates.SqueegeeLitres);
	S->Litres -= Taken;
	Floor.LitresCleared += Taken;
	if (S->Litres <= KINDA_SMALL_NUMBER)
	{
		Floor.Spills.RemoveAll([Id](const FFloorSpillState& Other) { return Other.Id == Id; });
	}
	else { S->RadiusCm = RadiusFor(S->Litres); }
	return true;
}

FName FFloorHazards::Contains(const FVector& Location) const
{
	if (Location.ContainsNaN()) { return NAME_None; }
	for (const FFloorSpillState& S : Floor.Spills)
	{
		if (FVector::Dist2D(S.Location, Location) <= S.RadiusCm && FMath::Abs(S.Location.Z - Location.Z) <= Rates.ZToleranceCm) { return S.Id; }
	}
	return NAME_None;
}

FString FFloorHazards::Summary() const
{
	if (Floor.Spills.IsEmpty()) { return TEXT("No spills"); }
	TArray<FString> Parts;
	for (const FFloorSpillState& S : Floor.Spills)
	{
		Parts.Add(FString::Printf(TEXT("%s by %s %d L r=%d"), *S.Id.ToString(), *S.StillId.ToString(), FMath::FloorToInt(S.Litres), FMath::FloorToInt(S.RadiusCm)));
	}
	return FString::Printf(TEXT("Spills %d: %s"), Floor.Spills.Num(), *FString::Join(Parts, TEXT("; ")));
}
