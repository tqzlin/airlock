// Copyright Airlock project. All Rights Reserved.
#include "Ship/FuelLayouts.h"

const FFuelLayout* FuelLayouts::Find(FName SectionId)
{
	static const FFuelLayout Hauler = []
	{
		FFuelLayout L; L.SectionId = TEXT("Hauler_Mk1");
		const auto Fixture = [](FName Id, FVector Position, const TCHAR* Mesh, float Scale, float Yaw)
		{
			FFuelFixtureSpec S; S.Id = Id; S.Transform = FTransform(FRotator(0, Yaw, 0), Position);
			S.Mesh = FSoftObjectPath(Mesh); S.Scale = FVector(Scale); return S;
		};
		// The wheel projects along mesh -Y; yaw -90 faces it toward the -X walkway.
		// CENTRAL ENGINEERING east side, floor 22. Both mesh paths confirmed through AssetTools.exists.
		// Pawn probes: all four X=-1300 walkway spots remain at their requested XY, feet Z=22..23.807.
		// The narrow valve centres depenetrate a capsule slightly away from the wall; the fixtures
		// themselves only block Visibility. The intake centre settles exactly at (-1200,600,118).
		L.FeedValve = Fixture(TEXT("FeedValve"), FVector(-1130,280,22), TEXT("/Game/ModSci_Engineer/Meshes/SM_PipeValve_A.SM_PipeValve_A"), .3f, -90);
		L.BypassValve = Fixture(TEXT("BypassValve"), FVector(-1130,700,22), TEXT("/Game/ModSci_Engineer/Meshes/SM_PipeValve_A.SM_PipeValve_A"), .3f, -90);
		L.Intake = Fixture(TEXT("FuelIntake"), FVector(-1200,600,22), TEXT("/Game/ModSci_EngiProps/Meshes/SM_PlasticPallet.SM_PlasticPallet"), .6f, 0);
		return L;
	}();
	return SectionId == Hauler.SectionId ? &Hauler : nullptr;
}
