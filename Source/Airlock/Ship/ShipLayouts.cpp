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
		// Level pass (2026-09-12): the ship's stations, a run apart on foot (design doc 8.1 lever 4). Every spot was
		// probed in PIE (AirlockWarp + AirlockWhere); Z is the walking surface there.
		//   Cargo hold   = the west hall, glass floor Z -65 (PlayerStart_Ship at its west end; Astra's clamps/crates
		//                  use X -6100..-4800 on Y 1400 / 1700).
		//   Main corridor = X -4300..-1400 along Y 1560, walkway Z -68, wall faces at Y 1349 (south) / 1771 (north).
		//   Engineering  = the L-corridor the pack signs "CENTRAL ENGINEERING": X -1531..-1106 (wall faces), Y 230..950,
		//                  floor Z 22, entered from the main corridor's east end; the junction at (-1320, -20) below it.
		//   Galley stub  = the junction's closed west side, floor Z 20.
		//   Bridge       = Command Center room east of X 980, floor Z 22 (dais 28), terminal at (2010, 240).
		// The Interiors room north of the hall (X -5600..-4206, Y 2400..3140) is a vista: a 90 cm sill, glass and a
		// 1.4 m trench keep it off the walkable map (grid probe 2026-09-12). LabelYaw = world yaw a label faces.
		auto Module = [&Layout](const TCHAR* Id, EShipModuleType Type, double X, double Y, double Z, double Yaw, double LabelYaw, const TCHAR* Mesh, double Scale = 1.0)
		{
			FShipModuleSpec Spec;
			Spec.Id = Id; Spec.Type = Type;
			Spec.Transform = FTransform(FRotator(0, Yaw, 0), FVector(X, Y, Z), FVector(Scale));
			Spec.Mesh = FSoftObjectPath(Mesh);
			Spec.LabelYaw = LabelYaw;
			Layout.Modules.Add(Spec);
		};
		// Engineering: the core band (mesh runs 670 along its local -X, 34 deep in +Y, 115 tall) lies along the west
		// wall for the corridor's whole length (yaw 90: local -X -> world -Y, depth towards the wall); the coolant tank
		// (287 cm round, scaled 0.6) hugs the east wall; life support is boxed on the east wall; the engine node sits
		// on the floor by the east wall at the north end.
		Module(TEXT("PowerCore"), EShipModuleType::PowerCore, -1497, 900, 22, 90, 0, TEXT("/Game/ModSciInteriors/Meshes/SM_PowerCells_Wall.SM_PowerCells_Wall"));
		Module(TEXT("CoolantLoop"), EShipModuleType::CoolantLoop, -1192, 420, 22, 0, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_WVC_Tank_A.SM_WVC_Tank_A"), 0.6);
		Module(TEXT("LifeSupport"), EShipModuleType::LifeSupport, -1106, 760, 110, 0, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_ElectricDistribBox.SM_ElectricDistribBox"));
		// Cargo hold: the clamp controller on the hall's north side, west end.
		Module(TEXT("CargoClamps"), EShipModuleType::CargoClamps, -6300, 1900, -65, 180, -90, TEXT("/Game/ModSci_Engineer/Meshes/SM_StorageAreaNetwork.SM_StorageAreaNetwork"));
		Module(TEXT("Engines"), EShipModuleType::Engines, -1160, 880, 22, 0, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_EnergyNode_A.SM_EnergyNode_A"));
		// Bridge: nav rack on the north side of the room; galley cabinet in the junction's dead-end west stub.
		Module(TEXT("Nav"), EShipModuleType::Nav, 1800, 700, 22, 180, 180, TEXT("/Game/ModSciInteriors/Meshes/SM_WVD_Server.SM_WVD_Server"));
		Module(TEXT("Galley"), EShipModuleType::Galley, -1420, -20, 20, 0, 0, TEXT("/Game/StarterBundle/ModularScifiProps/Meshes/SM_Cabinet_A.SM_Cabinet_A"));
		auto Line = [&Layout](const TCHAR* Id, EShipLineKind Kind, const TCHAR* From, const TCHAR* To, double X, double Y, double Z, double Yaw, double LabelYaw, const TCHAR* Mesh)
		{
			FShipLineSpec Spec;
			Spec.Id = Id; Spec.Kind = Kind; Spec.From = From; Spec.To = To;
			Spec.Transform = FTransform(FRotator(0, Yaw, 0), FVector(X, Y, Z));
			Spec.Mesh = FSoftObjectPath(Mesh);
			Spec.LabelYaw = LabelYaw;
			Layout.Lines.Add(Spec);
		};
		// Pipes run 0..360 along their local X, centred on Y/Z. Coolant_1 runs along engineering's west wall in front of
		// the core band; Power_1 climbs the east wall next to life support; Coolant_2 (south wall) and Power_2 (north
		// wall) are in the main corridor, so a repair is a run between the hold and engineering.
		Line(TEXT("Coolant_1"), EShipLineKind::Coolant, TEXT("CoolantLoop"), TEXT("PowerCore"), -1478, 250, 62, 90, 0, TEXT("/Game/ModSci_Engineer/Meshes/SM_Pipe_Medium.SM_Pipe_Medium"));
		Line(TEXT("Coolant_2"), EShipLineKind::Coolant, TEXT("CoolantLoop"), TEXT("Engines"), -3500, 1360, 40, 0, 90, TEXT("/Game/ModSci_Engineer/Meshes/SM_Pipe_Medium.SM_Pipe_Medium"));
		Line(TEXT("Power_1"), EShipLineKind::Power, TEXT("PowerCore"), TEXT("LifeSupport"), -1110, 450, 100, 90, 180, TEXT("/Game/ModSci_EngiProps/Meshes/SM_Wire_Wall_A.SM_Wire_Wall_A"));
		Line(TEXT("Power_2"), EShipLineKind::Power, TEXT("PowerCore"), TEXT("CargoClamps"), -2700, 1768, 100, -90, -90, TEXT("/Game/ModSci_EngiProps/Meshes/SM_Wire_Wall_A.SM_Wire_Wall_A"));
		// The control-room seat: inside the U of the bridge terminal (map mesh at 2010,240 yaw 180) on its dais, facing
		// the screens (+X); standing up puts you 140 cm back towards the door.
		FControlStationSpec Bridge;
		Bridge.Id = TEXT("Bridge");
		Bridge.Transform = FTransform(FRotator(0, 0, 0), FVector(1930, 240, 28));
		Bridge.LabelYaw = 180;
		Layout.Stations.Add(Bridge);
		return Layout;
	}();
	return SectionId == Hauler.SectionId ? &Hauler : nullptr;
}
}
