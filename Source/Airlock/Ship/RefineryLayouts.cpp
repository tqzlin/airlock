// Copyright Airlock project. All Rights Reserved.
#include "Ship/RefineryLayouts.h"

namespace
{
	const TCHAR* FloorTankMesh = TEXT("/Game/ModSciInteriors/Meshes/SM_WVC_Tank_A.SM_WVC_Tank_A");
	const TCHAR* FloorPalletMesh = TEXT("/Game/ModSci_EngiProps/Meshes/SM_PlasticPallet.SM_PlasticPallet");
	const TCHAR* FloorCabinetMesh = TEXT("/Game/StarterBundle/ModularScifiProps/Meshes/SM_Cabinet_A.SM_Cabinet_A");
}

FName RefineryLayouts::FloorSectionId()
{
	static const FName Id(TEXT("RefineryFloor"));
	return Id;
}

const FRefineryLayout* RefineryLayouts::Find(FName SectionId)
{
	static const FRefineryLayout Floor = []
	{
		FRefineryLayout Layout;
		Layout.SectionId = RefineryLayouts::FloorSectionId();
		// The floor is a copy of the Interiors pack's second hall (the hub's footprint: X -6980..-4300 along Y 1560,
		// the airlock at the west end). Probed 2026-09-13 (`verify-floor.ps1 -ProbePositions`): the main deck's walking
		// surface is Z -65 (feet), the walkway trench at Y 1560 is -70, and both ends are platforms at Z 22 (the west
		// one from the airlock to about X -6300 with railings, the east one past about X -4700).
		auto Fixture = [&Layout](const TCHAR* Id, EFloorFixtureKind Kind, double X, double Y, double Z, double Yaw, double Scale,
			double LabelYaw, double LabelHeight, const TCHAR* Mesh, const TCHAR* Line1, const TCHAR* Line2)
		{
			FFloorFixtureSpec Spec;
			Spec.Id = Id; Spec.Kind = Kind;
			Spec.Transform = FTransform(FRotator(0, Yaw, 0), FVector(X, Y, Z), FVector(Scale));
			Spec.Mesh = FSoftObjectPath(Mesh);
			Spec.LabelYaw = LabelYaw; Spec.LabelHeight = LabelHeight;
			Spec.Line1 = Line1; Spec.Line2 = Line2;
			Layout.Fixtures.Add(Spec);
		};
		// Three stills in a row along the north side, three metres apart, labels facing the walkway (-Y).
		Fixture(TEXT("Still_1"), EFloorFixtureKind::Still, -5800, 1900, -65, 0, 0.8, -90, 250, FloorTankMesh, TEXT("STILL 1"), TEXT("idle"));
		Fixture(TEXT("Still_2"), EFloorFixtureKind::Still, -5400, 1900, -65, 0, 0.8, -90, 250, FloorTankMesh, TEXT("STILL 2"), TEXT("idle"));
		Fixture(TEXT("Still_3"), EFloorFixtureKind::Still, -5000, 1900, -65, 0, 0.8, -90, 250, FloorTankMesh, TEXT("STILL 3"), TEXT("idle"));
		// The product tank at the east end, the drain mid-walkway, the bunk by the airlock.
		Fixture(TEXT("Tank"), EFloorFixtureKind::Tank, -4650, 1900, 22, 0, 1.0, -90, 300, FloorTankMesh, TEXT("DIESEL"), TEXT("0 L"));
		Fixture(TEXT("Drain"), EFloorFixtureKind::Drain, -5400, 1560, -70, 0, 0.6, 90, 60, FloorPalletMesh, TEXT("DRAIN"), TEXT(""));
		Fixture(TEXT("Bunk"), EFloorFixtureKind::Bunk, -6650, 1250, 22, 90, 1.0, 90, 150, FloorCabinetMesh, TEXT("BUNK"), TEXT("empty"));
		return Layout;
	}();
	return SectionId == Floor.SectionId ? &Floor : nullptr;
}

int32 RefineryLayouts::StillCount(const FRefineryLayout& Layout)
{
	int32 Count = 0;
	for (const FFloorFixtureSpec& Spec : Layout.Fixtures) { Count += Spec.Kind == EFloorFixtureKind::Still; }
	return Count;
}

const TCHAR* RefineryLayouts::KnobName(ERefineryKnob Knob)
{
	switch (Knob)
	{
	case ERefineryKnob::Heat: return TEXT("heat");
	case ERefineryKnob::Feed: return TEXT("feed");
	case ERefineryKnob::Vent: return TEXT("vent");
	default: return TEXT("unknown");
	}
}

bool RefineryLayouts::ParseKnob(const FString& Text, ERefineryKnob& OutKnob)
{
	for (const ERefineryKnob Knob : { ERefineryKnob::Heat, ERefineryKnob::Feed, ERefineryKnob::Vent })
	{
		if (Text.Equals(KnobName(Knob), ESearchCase::IgnoreCase)) { OutKnob = Knob; return true; }
	}
	return false;
}

const TCHAR* RefineryLayouts::LeverName(ERefineryLever Lever)
{
	switch (Lever)
	{
	case ERefineryLever::Reset: return TEXT("Reset");
	case ERefineryLever::Purge: return TEXT("Purge");
	case ERefineryLever::Scrape: return TEXT("Scrape");
	case ERefineryLever::Repair: return TEXT("Repair");
	case ERefineryLever::Calibrate: return TEXT("Calibrate");
	default: return TEXT("Unknown");
	}
}

const TCHAR* RefineryLayouts::LeverDone(ERefineryLever Lever)
{
	switch (Lever)
	{
	case ERefineryLever::Reset: return TEXT("reset");
	case ERefineryLever::Purge: return TEXT("purged");
	case ERefineryLever::Scrape: return TEXT("scraped");
	case ERefineryLever::Repair: return TEXT("repaired");
	case ERefineryLever::Calibrate: return TEXT("calibrated");
	default: return TEXT("unknown");
	}
}

bool RefineryLayouts::ParseLever(const FString& Text, ERefineryLever& OutLever)
{
	for (const ERefineryLever Lever : { ERefineryLever::Reset, ERefineryLever::Purge, ERefineryLever::Scrape, ERefineryLever::Repair, ERefineryLever::Calibrate })
	{
		if (Text.Equals(LeverName(Lever), ESearchCase::IgnoreCase)) { OutLever = Lever; return true; }
	}
	return false;
}

const TCHAR* RefineryLayouts::StatusName(EStillStatus Status)
{
	switch (Status)
	{
	case EStillStatus::Idle: return TEXT("Idle");
	case EStillStatus::Running: return TEXT("Running");
	case EStillStatus::Popped: return TEXT("Popped");
	case EStillStatus::Flash: return TEXT("Flash");
	case EStillStatus::Failed: return TEXT("Failed");
	default: return TEXT("Unknown");
	}
}

TArray<FStillControlSpec> RefineryLayouts::StillControls(const FFloorFixtureSpec& Still)
{
	TArray<FStillControlSpec> Controls;
	if (Still.Kind != EFloorFixtureKind::Still) { return Controls; }
	const FSoftObjectPath Valve(TEXT("/Game/ModSci_Engineer/Meshes/SM_PipeValve_A.SM_PipeValve_A"));
	const FSoftObjectPath Panel(TEXT("/Game/ModSciInteriors/Meshes/SM_WVD_Panel_A.SM_WVD_Panel_A"));
	auto Add = [&](EStillControl Kind, const TCHAR* Suffix, FVector Offset, float Yaw,
		FSoftObjectPath Mesh, FVector Scale, const TCHAR* Label)
	{
		FStillControlSpec S;
		// The east-end props raise Still 3's original vent/repair spots by 149-163 cm (PIE probe).
		// Bring its working rows onto the clear walkway; keep the already-clear hopper beside the row.
		if (Still.Id == TEXT("Still_3") && Kind != EStillControl::Hopper) { Offset.Y = -300; }
		// The clear east walkway ramps up with X: feet -16 at VentPlus and +15 at the trap.
		if (Still.Id == TEXT("Still_3") && Kind == EStillControl::VentPlus) { Offset.Z = 50; }
		if (Still.Id == TEXT("Still_3") && Kind == EStillControl::Calibrate) { Offset.Z = 190; }
		// At the original height the front lever labels overlap the prescribed batch gauge.
		if (Still.Id != TEXT("Still_3") && Kind >= EStillControl::Reset && Kind <= EStillControl::Repair) { Offset.Z = 85; }
		S.Kind = Kind; S.StillId = Still.Id; S.Id = FName(*(Still.Id.ToString() + TEXT("_") + Suffix));
		S.Offset = Offset; S.Yaw = Yaw; S.Mesh = Mesh; S.Scale = Scale; S.Label = Label;
		const float WorldYaw = Still.Transform.Rotator().Yaw + Yaw;
		S.Transform = FTransform(FRotator(0, WorldYaw, 0),
			Still.Transform.GetLocation() + Still.Transform.GetRotation().RotateVector(Offset), Scale);
		S.LabelYaw = WorldYaw - 90.f;
		Controls.Add(S);
	};
	Add(EStillControl::Hopper, TEXT("Hopper"), FVector(-200,-200,0), 0, FSoftObjectPath(FloorPalletMesh), FVector(.6), TEXT("HOPPER"));
	Add(EStillControl::HeatMinus, TEXT("HeatMinus"), FVector(-120,-200,0), 0, Valve, FVector(.3,.3,.15), TEXT("HEAT -"));
	Add(EStillControl::HeatPlus, TEXT("HeatPlus"), FVector(-80,-200,0), 0, Valve, FVector(.3,.3,.15), TEXT("HEAT +"));
	Add(EStillControl::FeedMinus, TEXT("FeedMinus"), FVector(-20,-200,0), 0, Valve, FVector(.3,.3,.15), TEXT("FEED -"));
	Add(EStillControl::FeedPlus, TEXT("FeedPlus"), FVector(20,-200,0), 0, Valve, FVector(.3,.3,.15), TEXT("FEED +"));
	Add(EStillControl::VentMinus, TEXT("VentMinus"), FVector(80,-200,0), 0, Valve, FVector(.3,.3,.15), TEXT("VENT -"));
	Add(EStillControl::VentPlus, TEXT("VentPlus"), FVector(120,-200,0), 0, Valve, FVector(.3,.3,.15), TEXT("VENT +"));
	Add(EStillControl::Reset, TEXT("Reset"), FVector(-75,-130,110), 0, Panel, FVector(.35), TEXT("RESET"));
	Add(EStillControl::Purge, TEXT("Purge"), FVector(-25,-130,110), 0, Panel, FVector(.35), TEXT("PURGE"));
	Add(EStillControl::Scrape, TEXT("Scrape"), FVector(25,-130,110), 0, Panel, FVector(.35), TEXT("SCRAPE"));
	Add(EStillControl::Repair, TEXT("Repair"), FVector(75,-130,110), 0, Panel, FVector(.35), TEXT("REPAIR"));
	Add(EStillControl::Calibrate, TEXT("Calibrate"), FVector(150,-60,110), 90, Panel, FVector(.35), TEXT("TRAP"));
	return Controls;
}
