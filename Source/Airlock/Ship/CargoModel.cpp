// Copyright Airlock project. All Rights Reserved.
#include "Ship/CargoModel.h"
#include "Ship/CargoLayouts.h"
#include "Ship/ShipAtmosphere.h"
#include "Ship/ShipGraph.h"
#include "Ship/FuelSystem.h"
#include "Voyage/IncidentSchedule.h"

bool FCargoCrateState::operator==(const FCargoCrateState& Other) const
{
	return Id == Other.Id && bVolatile == Other.bVolatile && State == Other.State
		&& Temperature == Other.Temperature && SlotId == Other.SlotId;
}

bool FCargoSlotState::operator==(const FCargoSlotState& Other) const
{
	return Id == Other.Id && bRack == Other.bRack && CrateId == Other.CrateId;
}

void FCargoModel::Build(const FCargoLayout& Layout)
{
	RackCoolFactor = 1.f;
	CrateStates.Reset();
	SlotStates.Reset();
	for (const auto& Spec : Layout.Crates)
	{
		FCargoCrateState Crate;
		Crate.Id = Spec.Id;
		Crate.bVolatile = Spec.bVolatile;
		CrateStates.Add(Crate);
	}
	for (const auto& Spec : Layout.Slots)
	{
		FCargoSlotState Slot;
		Slot.Id = Spec.Id;
		Slot.bRack = Spec.bRack;
		SlotStates.Add(Slot);
	}
}

bool FCargoModel::Tick(float DeltaSeconds, double VoyageElapsedSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) { return false; }
	bool bChanged = false;
	for (auto& Crate : CrateStates)
	{
		if (Crate.State == ECargoState::Vented || Crate.State == ECargoState::Burned) { continue; }
		const int32 Before = FMath::FloorToInt(Crate.Temperature);
		// An injected 100 vents even in a rack: cooling cannot undo an already reached threshold.
		if (Crate.Temperature < Rates.VentTemperature)
		{
			float Rate = 0.f;
			if (Crate.State == ECargoState::Racked) { Rate = -Rates.RackCool * RackCoolFactor; }
			else if (Crate.bVolatile && VoyageElapsedSeconds >= FIncidentSchedule::CalmSeconds)
			{
				if (Crate.State == ECargoState::Loose) { Rate = Rates.LooseHeat; }
				else if (Crate.State == ECargoState::Carried) { Rate = Rates.CarriedHeat; }
			}
			Crate.Temperature = FMath::Clamp(Crate.Temperature + Rate * DeltaSeconds, 0.f, 100.f);
		}
		if (Crate.Temperature >= Rates.VentTemperature)
		{
			Crate.State = ECargoState::Vented;
			bChanged = true;
		}
		bChanged |= Before != FMath::FloorToInt(Crate.Temperature);
	}
	return bChanged;
}

void FCargoModel::SetRackCoolFactor(float Factor)
{
	if (FMath::IsFinite(Factor)) { RackCoolFactor = FMath::Clamp(Factor, 0.f, 1.f); }
}

FCargoCrateState* FCargoModel::MutableCrate(FName Id)
{
	return CrateStates.FindByPredicate([Id](const auto& Crate) { return Crate.Id == Id; });
}
const FCargoCrateState* FCargoModel::FindCrate(FName Id) const
{
	return CrateStates.FindByPredicate([Id](const auto& Crate) { return Crate.Id == Id; });
}
FCargoSlotState* FCargoModel::MutableSlot(FName Id)
{
	return SlotStates.FindByPredicate([Id](const auto& Slot) { return Slot.Id == Id; });
}

bool FCargoModel::PickUp(FName Id)
{
	auto* Crate = MutableCrate(Id);
	if (!Crate || (Crate->State == ECargoState::Vented || Crate->State == ECargoState::Burned) || Crate->State == ECargoState::Carried) { return false; }
	if (auto* Slot = MutableSlot(Crate->SlotId)) { Slot->CrateId = NAME_None; }
	Crate->SlotId = NAME_None;
	Crate->State = ECargoState::Carried;
	return true;
}
bool FCargoModel::Consume(FName Id)
{
	auto* Crate = MutableCrate(Id);
	if (!Crate || Crate->State != ECargoState::Carried) { return false; }
	Crate->State = ECargoState::Burned; return true;
}
bool FCargoModel::PutDown(FName Id)
{
	auto* Crate = MutableCrate(Id);
	if (!Crate || Crate->State != ECargoState::Carried) { return false; }
	Crate->State = ECargoState::Loose;
	return true;
}
bool FCargoModel::Place(FName Id, FName SlotId)
{
	auto* Crate = MutableCrate(Id);
	auto* Slot = MutableSlot(SlotId);
	if (!Crate || Crate->State != ECargoState::Carried || !Slot || !Slot->CrateId.IsNone()) { return false; }
	Crate->State = Slot->bRack ? ECargoState::Racked : ECargoState::Secured;
	Crate->SlotId = SlotId;
	Slot->CrateId = Id;
	return true;
}
bool FCargoModel::Release(FName SlotId)
{
	auto* Slot = MutableSlot(SlotId);
	if (!Slot || Slot->bRack || Slot->CrateId.IsNone()) { return false; }
	auto* Crate = MutableCrate(Slot->CrateId);
	if (!Crate || Crate->State != ECargoState::Secured) { return false; }
	Crate->State = ECargoState::Loose;
	Crate->SlotId = NAME_None;
	Slot->CrateId = NAME_None;
	return true;
}
bool FCargoModel::SetTemperature(FName Id, float Temperature)
{
	auto* Crate = MutableCrate(Id);
	if (!Crate || !Crate->bVolatile || (Crate->State == ECargoState::Vented || Crate->State == ECargoState::Burned)
		|| !FMath::IsFinite(Temperature) || Temperature < 0 || Temperature > 100) { return false; }
	Crate->Temperature = Temperature;
	return true;
}

const TCHAR* FCargoModel::StateName(ECargoState State)
{
	switch (State)
	{
	case ECargoState::Loose: return TEXT("Loose");
	case ECargoState::Carried: return TEXT("Carried");
	case ECargoState::Secured: return TEXT("Secured");
	case ECargoState::Racked: return TEXT("Racked");
	case ECargoState::Vented: return TEXT("Vented");
	case ECargoState::Burned: return TEXT("Burned");
	default: return TEXT("Unknown");
	}
}
FString FCargoModel::Summary() const
{
	if (CrateStates.IsEmpty() && SlotStates.IsEmpty()) { return TEXT("Cargo empty"); }
	TArray<FString> Lines;
	for (const auto& Crate : CrateStates)
	{
		Lines.Add(FString::Printf(TEXT("Cargo %s %s temp=%d volatile=%d slot=%s"), *Crate.Id.ToString(),
			StateName(Crate.State), FMath::FloorToInt(Crate.Temperature), Crate.bVolatile, *Crate.SlotId.ToString()));
	}
	for (const auto& Slot : SlotStates)
	{
		Lines.Add(FString::Printf(TEXT("Cargo slot %s %s %s"), *Slot.Id.ToString(), Slot.bRack ? TEXT("rack") : TEXT("clamp"),
			Slot.CrateId.IsNone() ? TEXT("empty") : *Slot.CrateId.ToString()));
	}
	return FString::Join(Lines, TEXT("\n"));
}
FVoyageResult FCargoModel::Tally(const FCargoModel& Cargo, const FShipGraph& Ship, const FVoyageSpec& Spec,
	const FAtmosphereState* Air, int32 Blackouts, const FFuelState* Fuel)
{
	FVoyageResult Result;
	Result.Type = Spec.Type;
	Result.Seed = Spec.Seed;
	Result.CratesTotal = Cargo.Crates().Num();
	for (const auto& Crate : Cargo.Crates())
	{
		if (Crate.State == ECargoState::Burned) { ++Result.CratesBurned; }
		else if (Crate.State == ECargoState::Vented) { ++Result.CratesVented; }
		else if (Crate.State == ECargoState::Secured || Crate.State == ECargoState::Racked)
		{
			++Result.CratesIntact;
			Result.IntactCasks.Add(Crate.Id);
			if (Crate.bVolatile) { Result.IntactLightCasks.Add(Crate.Id); }
		}
		else { ++Result.CratesLoose; }
	}
	double Sum = 0;
	for (const auto& Module : Ship.Modules()) { Sum += Module.Condition; }
	Result.ShipCondition = Ship.Modules().IsEmpty() ? 0.f : FMath::Clamp(static_cast<float>(Sum / Ship.Modules().Num()), 0.f, 100.f);
	Result.Pay = PayPerIntactCrate * Result.CratesIntact
		+ (Result.CratesVented == 0 && Result.CratesLoose == 0 && Result.CratesBurned == 0 ? CleanManifestBonus : 0)
		+ FMath::RoundToInt(Result.ShipCondition * HullBonusPerPoint);
	Result.FuelCredit = FuelCreditPerIntactCask * Result.CratesIntact;
	Result.FuelSurplus = SurplusPerIntactCask * Result.CratesIntact;
	if (Air)
	{
		Result.DockPpm = Air->Ppm;
		Result.Blackouts = FMath::Max(0, Blackouts);
		Result.bCleanAir = Air->Ppm < FAtmosphereRates().CleanPpm;
		const int64 Pay = static_cast<int64>(Result.Pay) + (Result.bCleanAir ? CleanAirBonus : 0)
			- static_cast<int64>(BlackoutDock) * Result.Blackouts;
		Result.Pay = static_cast<int32>(FMath::Max<int64>(0, Pay));
	}
	if (Fuel)
	{
		Result.bHasFuelData = true; Result.bLateDock = Fuel->bExhausted;
		const int64 Pay = static_cast<int64>(Result.Pay) - static_cast<int64>(SootFine) * Result.CratesBurned
			- (Result.bLateDock ? LateDockFine : 0);
		Result.Pay = static_cast<int32>(FMath::Max<int64>(0, Pay));
	}
	return Result;
}
FString FVoyageResult::Summary() const
{
	FString Text = FString::Printf(TEXT("%d/%d crates intact, %d vented, %d loose, hull %.2f%%, pay %d"),
		CratesIntact, CratesTotal, CratesVented, CratesLoose, ShipCondition, Pay);
	if (DockPpm >= 0) { Text += FString::Printf(TEXT(", air %d ppm, %d blackouts"), FMath::FloorToInt(DockPpm), Blackouts); }
	if (bHasFuelData) { Text += FString::Printf(TEXT(", %d burned%s"), CratesBurned, bLateDock ? TEXT(", late") : TEXT("")); }
	if (bHasFloor) { Text += FString::Printf(TEXT(" | floor: %d/%d casks refined, %d L diesel, pay %d"), FloorCasksRefined, FloorCasksTotal, FloorDieselL, FloorPay); }
	return Text;
}
