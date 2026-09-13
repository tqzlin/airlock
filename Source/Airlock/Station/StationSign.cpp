// Copyright Airlock project. All Rights Reserved.
#include "Station/StationSign.h"
#include "Ship/ShipVisuals.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"

AStationSign::AStationSign()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SignRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SignRoot"));
	SetRootComponent(SignRoot);
	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetupAttachment(SignRoot);
}
void AStationSign::Initialize(const FStationSignSpec& InSpec) { check(HasAuthority()); Spec = InSpec; }
void AStationSign::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Spec();
}
void AStationSign::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	Text->SetText(FText::FromString(Spec.Line2.IsEmpty() ? Spec.Line1 : Spec.Line1 + TEXT("\n") + Spec.Line2));
	ShipVisuals::ConfigureLabel(Text, Spec.WorldSize, GetActorRotation().Yaw, Spec.Colour, FVector::ZeroVector);
}
void AStationSign::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AStationSign, Spec, COND_InitialOnly);
}

const TArray<FStationSignSpec>& StationSigns::Find(FName SectionId)
{
	static const TArray<FStationSignSpec> Hub = []
	{
		TArray<FStationSignSpec> Signs;
		auto Sign = [&Signs](const TCHAR* Id, const TCHAR* Line1, const TCHAR* Line2, double X, double Y, double Z, double Yaw, float Size)
		{
			FStationSignSpec Spec;
			Spec.Id = Id; Spec.Line1 = Line1; Spec.Line2 = Line2;
			Spec.Transform = FTransform(FRotator(0, Yaw, 0), FVector(X, Y, Z));
			Spec.WorldSize = Size;
			Signs.Add(Spec);
		};
		// The Rig's identity on the hub's walls: over the departure board at Dock B and over the sealed Dock A.
		// Positions are placeholders on the hall's centre line until probed against the walls (M3-7 report).
		Sign(TEXT("Sign_DockB"), TEXT("VELL EXTRACTOR"), TEXT("DOCK B  -  HALDER SOLAR REFINERY RUNS"), -4520, 1560, 330, 180, 34.f);
		Sign(TEXT("Sign_DockA"), TEXT("VELL EXTRACTOR"), TEXT("DOCK A  -  NO VESSEL DOCKED"), -6760, 1560, 330, 0, 34.f);
		return Signs;
	}();
	// The refinery floor (M5-2): Halder Solar's sign over the airlock, the methane warning over the stills, the
	// scrubber panel's sign on the south side. Same placeholder heights as the Rig's.
	static const TArray<FStationSignSpec> Floor = []
	{
		TArray<FStationSignSpec> Signs;
		auto Sign = [&Signs](const TCHAR* Id, const TCHAR* Line1, const TCHAR* Line2, double X, double Y, double Z, double Yaw, float Size)
		{
			FStationSignSpec Spec;
			Spec.Id = Id; Spec.Line1 = Line1; Spec.Line2 = Line2;
			Spec.Transform = FTransform(FRotator(0, Yaw, 0), FVector(X, Y, Z));
			Spec.WorldSize = Size;
			Signs.Add(Spec);
		};
		Sign(TEXT("Sign_Floor"), TEXT("HALDER SOLAR"), TEXT("FLOOR 3  -  CRUDE INTAKE"), -6760, 1560, 330, 0, 34.f);
		Sign(TEXT("Sign_Stills"), TEXT("NO NAKED FLAME"), TEXT("METHANE"), -5400, 1900, 340, -90, 30.f);
		Sign(TEXT("Sign_Scrubbers"), TEXT("SCRUBBERS"), TEXT(""), -5400, 1150, 300, 90, 30.f);
		return Signs;
	}();
	static const TArray<FStationSignSpec> None;
	if (SectionId == TEXT("StationHub")) { return Hub; }
	return SectionId == TEXT("RefineryFloor") ? Floor : None;
}
