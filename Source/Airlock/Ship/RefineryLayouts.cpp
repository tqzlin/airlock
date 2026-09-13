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
