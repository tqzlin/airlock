// Copyright Airlock project. All Rights Reserved.
#include "Voyage/IncidentSchedule.h"
#include "Ship/ShipLayouts.h"
#include "Ship/CargoLayouts.h"
#include "Math/RandomStream.h"

TArray<FName> FIncidentSchedule::Targets(const FShipLayout& Layout, EIncidentType Type, const FCargoLayout* Cargo)
{
	TArray<FName> Result;
	if (Type == EIncidentType::CargoShift && Cargo)
	{
		for (const auto& Slot : Cargo->Slots) { if (!Slot.bRack) { Result.Add(Slot.Id); } }
	}
	if (Type == EIncidentType::ModuleDamage)
	{
		for (const FShipModuleSpec& Module : Layout.Modules)
		{
			if (Module.Type == EShipModuleType::PowerCore || Module.Type == EShipModuleType::LifeSupport ||
				Module.Type == EShipModuleType::CargoClamps || Module.Type == EShipModuleType::Engines)
			{
				Result.Add(Module.Id);
			}
		}
	}
	else if (Type == EIncidentType::CoolantLeak || Type == EIncidentType::PowerShort)
	{
		const EShipLineKind Kind = Type == EIncidentType::CoolantLeak ? EShipLineKind::Coolant : EShipLineKind::Power;
		for (const FShipLineSpec& Line : Layout.Lines)
		{
			if (Line.Kind == Kind) { Result.Add(Line.Id); }
		}
	}
	return Result;
}

const TCHAR* FIncidentSchedule::TypeName(EIncidentType Type)
{
	switch (Type)
	{
	case EIncidentType::CoolantLeak: return TEXT("CoolantLeak");
	case EIncidentType::PowerShort: return TEXT("PowerShort");
	case EIncidentType::ModuleDamage: return TEXT("ModuleDamage");
	case EIncidentType::CargoShift: return TEXT("CargoShift");
	default: return TEXT("Unknown");
	}
}

FText FIncidentSchedule::MakeCaptainLine(EIncidentType Type, FName Target, FName Captain)
{
	FText Pattern;
	switch (Type)
	{
	case EIncidentType::CoolantLeak:
		Pattern = NSLOCTEXT("Airlock", "IncidentLeak", "Captain {0} here. Pressure drop on the coolant loop - find the leak at {1}."); break;
	case EIncidentType::PowerShort:
		Pattern = NSLOCTEXT("Airlock", "IncidentShort", "Captain {0} here. Breaker trip on the {1} feed."); break;
	case EIncidentType::ModuleDamage:
		Pattern = NSLOCTEXT("Airlock", "IncidentDamage", "Captain {0} here. {1} is taking damage, get on it."); break;
	case EIncidentType::CargoShift:
		Pattern = NSLOCTEXT("Airlock", "IncidentCargoShift", "Captain {0} here. Hold on - cargo's shifted in the hold, {1} let go."); break;
	default: return FText::GetEmpty();
	}
	return FText::Format(Pattern, FText::FromName(Captain), FText::FromName(Target));
}

TArray<FIncidentSpec> FIncidentSchedule::Build(const FVoyageSpec& Spec, const FShipLayout& Layout, int32 CrewCount, const FCargoLayout* Cargo)
{
	TArray<FIncidentSpec> Entries;
	if (!FMath::IsFinite(Spec.DurationSeconds) || Spec.DurationSeconds < 0.f) { return Entries; }
	FRandomStream Random(Spec.Seed ^ SeedSalt);
	// Saturate before adding, including extreme console/test inputs. Explicit count examples: 1->4, 4->7, 9->8.
	const int32 Count = BaseIncidents + FMath::Clamp(CrewCount, 1, MaxIncidents - BaseIncidents + 1) - 1;
	const float Duration = Spec.DurationSeconds;
	const bool bShort = Duration < ShortWindowThreshold;
	const float Start = bShort ? Duration * ShortWindowStartFraction : CalmSeconds;
	const float End = bShort ? Duration * ShortWindowEndFraction : Duration - DockSeconds;
	const float Window = End - Start;
	const float ReferenceWindow = ReferenceDuration - CalmSeconds - DockSeconds;
	const float Scale = FMath::Min(1.f, Window / ReferenceWindow);
	// Short clocks map the reference curve into their middle 50%. For tight intermediate durations,
	// prioritize the window and count, compressing gaps only when the requested spacing cannot fit.
	const float PairMin = bShort ? Start + Window * (ClimaxStartFraction * ReferenceDuration - CalmSeconds) / ReferenceWindow
		: FMath::Clamp(Duration * ClimaxStartFraction, Start, End);
	const float PairMax = bShort ? Start + Window * (ClimaxEndFraction * ReferenceDuration - CalmSeconds) / ReferenceWindow
		: FMath::Clamp(Duration * ClimaxEndFraction, PairMin, End);
	const float PairGap = FMath::Min(Random.FRandRange(ClimaxMinGapSeconds, ClimaxMaxGapSeconds) * (bShort ? Scale : 1.f), PairMax - PairMin);
	const float PairStart = Random.FRandRange(PairMin, PairMax - PairGap);
	const float Gap = FMath::Min(MinGapSeconds * (bShort ? Scale : 1.f), (PairStart - Start) / (Count - 2));
	const float Slack = FMath::Max(0.f, PairStart - Start - Gap * (Count - 2));
	const int32 ClampCount = Targets(Layout, EIncidentType::CargoShift, Cargo).Num();
	// No cargo means no extra random draw: the M3-3 stream and every resulting field are preserved.
	const int32 CargoClimaxIndex = ClampCount > 0 ? Count - ClimaxEntryCount + Random.RandRange(0, ClimaxEntryCount - 1) : INDEX_NONE;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FIncidentSpec Entry;
		if (Index == Count - 2) { Entry.AtSeconds = PairStart; }
		else if (Index == Count - 1) { Entry.AtSeconds = PairStart + PairGap; }
		else
		{
			// Jitter within disjoint slack bands preserves the minimum gap without retries.
			const float Fraction = Index == 0 ? 0.f : (Index - 1 + Random.FRand()) / (Count - 2);
			Entry.AtSeconds = Start + Index * Gap + Fraction * Slack;
		}
		TArray<EIncidentType> AvailableTypes;
		const FName Previous = Entries.IsEmpty() ? NAME_None : Entries.Last().TargetId;
		for (const EIncidentType Type : { EIncidentType::CoolantLeak, EIncidentType::PowerShort, EIncidentType::ModuleDamage })
		{
			TArray<FName> Valid = Targets(Layout, Type);
			Valid.Remove(Previous);
			if (!Valid.IsEmpty() && (Index > 0 || Type == EIncidentType::CoolantLeak)) { AvailableTypes.Add(Type); }
		}
		if (Index == CargoClimaxIndex) { AvailableTypes = { EIncidentType::CargoShift }; }
		else if (Index > 0 && Index < Count - ClimaxEntryCount && ClampCount >= CargoShiftMidMinClamps
			&& Entry.AtSeconds >= CargoShiftMinFraction * Duration)
		{
			AvailableTypes.Add(EIncidentType::CargoShift);
		}
		// A layout incapable of a teaching leak or non-repeating targets has no valid schedule.
		if (AvailableTypes.IsEmpty()) { return {}; }
		Entry.Type = AvailableTypes[Random.RandRange(0, AvailableTypes.Num() - 1)];
		TArray<FName> Valid = Targets(Layout, Entry.Type, Cargo);
		Valid.Remove(Previous);
		if (Valid.IsEmpty()) { return {}; }
		Entry.TargetId = Valid[Random.RandRange(0, Valid.Num() - 1)];
		Entry.Magnitude = Entry.Type == EIncidentType::ModuleDamage ? Random.FRandRange(MinDamage, MaxDamage) : 0.f;
		Entry.CaptainLine = MakeCaptainLine(Entry.Type, Entry.TargetId, Spec.Captain);
		Entries.Add(Entry);
	}
	return Entries;
}

int32 FIncidentSchedule::FindNextUnfired(const TArray<FIncidentSpec>& Entries, int32 NextEntry, double ElapsedSeconds)
{
	NextEntry = FMath::Clamp(NextEntry, 0, Entries.Num());
	while (Entries.IsValidIndex(NextEntry) && Entries[NextEntry].AtSeconds <= ElapsedSeconds) { ++NextEntry; }
	return NextEntry;
}
