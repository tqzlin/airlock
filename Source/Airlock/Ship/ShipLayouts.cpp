// Copyright Airlock project. All Rights Reserved.
#include "Ship/ShipLayouts.h"

namespace ShipLayouts
{
const FShipLayout* Find(FName SectionId)
{
	static const FShipLayout Hauler = []
	{
		FShipLayout Layout;
		Layout.SectionId = TEXT("Hauler_Mk1");
		// Z is the walking surface at that spot (probe: agent script 167). West hall glass floor -65, bridge 22.
		auto Module = [&Layout](const TCHAR* Id, EShipModuleType Type, double X, double Y, double Z, double Yaw, const TCHAR* Mesh)
		{
			FShipModuleSpec Spec;
			Spec.Id = Id; Spec.Type = Type;
			Spec.Transform = FTransform(FRotator(0, Yaw, 0), FVector(X, Y, Z));
			Spec.Mesh = FSoftObjectPath(Mesh);
			Layout.Modules.Add(Spec);
		};
		Module(TEXT("PowerCore"), EShipModuleType::PowerCore, -6300, 1900, -65, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_PowerCells_Wall.SM_PowerCells_Wall"));
		Module(TEXT("CoolantLoop"), EShipModuleType::CoolantLoop, -5900, 1900, -65, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_WVC_Tank_A.SM_WVC_Tank_A"));
		Module(TEXT("LifeSupport"), EShipModuleType::LifeSupport, -5500, 1900, -65, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_ElectricDistribBox.SM_ElectricDistribBox"));
		Module(TEXT("CargoClamps"), EShipModuleType::CargoClamps, -5100, 1900, -65, 180, TEXT("/Game/ModSci_Engineer/Meshes/SM_StorageAreaNetwork.SM_StorageAreaNetwork"));
		Module(TEXT("Engines"), EShipModuleType::Engines, -4700, 1900, -65, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_EnergyNode_A.SM_EnergyNode_A"));
		Module(TEXT("Nav"), EShipModuleType::Nav, 1800, 700, 22, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_WVD_Server.SM_WVD_Server"));
		Module(TEXT("Galley"), EShipModuleType::Galley, -4700, 1220, -65, 0, TEXT("/Game/StarterBundle/ModularScifiProps/Meshes/SM_Cabinet_A.SM_Cabinet_A"));
		auto Line = [&Layout](const TCHAR* Id, EShipLineKind Kind, const TCHAR* From, const TCHAR* To, double X, const TCHAR* Mesh)
		{
			FShipLineSpec Spec;
			Spec.Id = Id; Spec.Kind = Kind; Spec.From = From; Spec.To = To;
			Spec.Transform = FTransform(FVector(X, 1220, -65));
			Spec.Mesh = FSoftObjectPath(Mesh);
			Layout.Lines.Add(Spec);
		};
		Line(TEXT("Coolant_1"), EShipLineKind::Coolant, TEXT("CoolantLoop"), TEXT("PowerCore"), -6100, TEXT("/Game/ModSci_Engineer/Meshes/SM_Pipe_Medium.SM_Pipe_Medium"));
		Line(TEXT("Coolant_2"), EShipLineKind::Coolant, TEXT("CoolantLoop"), TEXT("Engines"), -5700, TEXT("/Game/ModSci_Engineer/Meshes/SM_Pipe_Medium.SM_Pipe_Medium"));
		Line(TEXT("Power_1"), EShipLineKind::Power, TEXT("PowerCore"), TEXT("LifeSupport"), -5300, TEXT("/Game/ModSci_EngiProps/Meshes/SM_Wire_Wall_A.SM_Wire_Wall_A"));
		Line(TEXT("Power_2"), EShipLineKind::Power, TEXT("PowerCore"), TEXT("CargoClamps"), -4900, TEXT("/Game/ModSci_EngiProps/Meshes/SM_Wire_Wall_A.SM_Wire_Wall_A"));
		return Layout;
	}();
	return SectionId == Hauler.SectionId ? &Hauler : nullptr;
}
}
