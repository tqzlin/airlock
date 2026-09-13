// Copyright Airlock project. All Rights Reserved.
#include "Ship/StillControl.h"
#include "Ship/ShipVisuals.h"
#include "Ship/CargoCrate.h"
#include "Core/AirlockGameState.h"
#include "Core/AirlockGameMode.h"
#include "Core/AirlockCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AStillControl::AStillControl()
{
	bReplicates = true; bAlwaysRelevant = true;
	ControlRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ControlRoot")); SetRootComponent(ControlRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Body->SetupAttachment(ControlRoot);
	Marker = CreateDefaultSubobject<UBoxComponent>(TEXT("Marker")); Marker->SetupAttachment(ControlRoot);
	for (UPrimitiveComponent* Component : { static_cast<UPrimitiveComponent*>(Body), static_cast<UPrimitiveComponent*>(Marker) })
	{
		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Component->SetGenerateOverlapEvents(false);
	}
	Marker->SetHiddenInGame(true);
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(ControlRoot);
}
void AStillControl::Initialize(const FStillControlSpec& InSpec) { check(HasAuthority()); Spec = InSpec; }
void AStillControl::BeginPlay()
{
	Super::BeginPlay(); OnRep_Spec(); BindState();
	if (!ObservedState.IsValid()) { GetWorldTimerManager().SetTimer(BindTimer, this, &AStillControl::BindState, .25f, true); }
}
void AStillControl::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ObservedState.IsValid()) { ObservedState->OnShipGraphChanged.Remove(ChangedHandle); }
	GetWorldTimerManager().ClearAllTimersForObject(this); Super::EndPlay(Reason);
}
void AStillControl::OnRep_Spec()
{
	if (Spec.Id.IsNone()) { return; }
	if (!bVisualsReady)
	{
		ShipVisuals::LoadBody(Body, Spec.Mesh, FVector(1));
		// Actor scale belongs to the cosmetic mesh. Keep trace boxes and text in world centimetres.
		const bool bHopper = Spec.Kind == EStillControl::Hopper;
		Marker->SetRelativeScale3D(FVector::OneVector / Spec.Scale);
		Marker->SetBoxExtent(bHopper ? FVector(32,32,12) : FVector(20,20,45));
		Marker->SetRelativeLocation(FVector(0,0,bHopper ? 18 : 60) / Spec.Scale);
		const float Height = bHopper ? 65.f : IsRepeatingHold_Implementation() ? 98.f : 28.f;
		ShipVisuals::ConfigureLabelUnscaled(Label, bHopper ? 12.f : 8.f, Spec.LabelYaw, FLinearColor::White, FVector(0,-12,Height));
		bVisualsReady = true;
	}
	Refresh();
}
void AStillControl::BindState()
{
	if (ObservedState.IsValid()) { return; }
	if (auto* GS = GetWorld()->GetGameState<AAirlockGameState>())
	{
		ObservedState = GS; ChangedHandle = GS->OnShipGraphChanged.AddUObject(this, &AStillControl::Refresh);
		GetWorldTimerManager().ClearTimer(BindTimer); Refresh();
	}
}
void AStillControl::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(AStillControl, Spec, COND_InitialOnly);
}
const FStillState* AStillControl::GetStillState() const
{
	const auto* GS = GetWorld() ? GetWorld()->GetGameState<AAirlockGameState>() : nullptr;
	return GS && GS->RefineryHasFloor() ? GS->Refinery.Stills.FindByPredicate([this](const FStillState& S) { return S.Id == Spec.StillId; }) : nullptr;
}
bool AStillControl::KnobAction(ERefineryKnob& Knob, int32& Step) const
{
	switch (Spec.Kind)
	{
	case EStillControl::HeatMinus: Knob = ERefineryKnob::Heat; Step = -1; return true;
	case EStillControl::HeatPlus: Knob = ERefineryKnob::Heat; Step = 1; return true;
	case EStillControl::FeedMinus: Knob = ERefineryKnob::Feed; Step = -1; return true;
	case EStillControl::FeedPlus: Knob = ERefineryKnob::Feed; Step = 1; return true;
	case EStillControl::VentMinus: Knob = ERefineryKnob::Vent; Step = -1; return true;
	case EStillControl::VentPlus: Knob = ERefineryKnob::Vent; Step = 1; return true;
	default: return false;
	}
}
bool AStillControl::LeverAction(ERefineryLever& Lever) const
{
	switch (Spec.Kind)
	{
	case EStillControl::Reset: Lever = ERefineryLever::Reset; return true;
	case EStillControl::Purge: Lever = ERefineryLever::Purge; return true;
	case EStillControl::Scrape: Lever = ERefineryLever::Scrape; return true;
	case EStillControl::Repair: Lever = ERefineryLever::Repair; return true;
	case EStillControl::Calibrate: Lever = ERefineryLever::Calibrate; return true;
	default: return false;
	}
}
int32 AStillControl::KnobValue(const FStillState& State) const
{
	ERefineryKnob Knob; int32 Step;
	if (!KnobAction(Knob, Step)) { return 0; }
	return Knob == ERefineryKnob::Heat ? State.HeatSteps : Knob == ERefineryKnob::Feed ? State.FeedSteps : State.VentSteps;
}
void AStillControl::Refresh()
{
	const auto* S = GetStillState(); if (!bVisualsReady || !S) { return; }
	FString Second; FLinearColor Colour = FLinearColor::White;
	if (Spec.Kind == EStillControl::Hopper) { Second = FString::Printf(TEXT("%d queued"), S->Hopper.Num()); }
	else if (IsRepeatingHold_Implementation()) { Second = FString::Printf(TEXT("%d%%"), KnobValue(*S) * 5); }
	else { Second = LeverLine(*S, Colour); }
	Label->SetTextRenderColor(Colour.ToFColor(true));
	Label->SetText(FText::FromString(Spec.Label + TEXT("\n") + Second));
}
FString AStillControl::LeverLine(const FStillState& S, FLinearColor& OutColour) const
{
	// A lever's second line is its own reading, not the still's status (M5-5): amber means "pull me now".
	static const FLinearColor Amber(1.f, .65f, .1f);
	const FRefineryRates Rates;
	switch (Spec.Kind)
	{
	case EStillControl::Reset:
		if (S.Status == EStillStatus::Popped) { OutColour = Amber; return TEXT("hold E"); }
		return TEXT("no pop");
	case EStillControl::Purge:
		if (S.Status == EStillStatus::Flash) { OutColour = Amber; return TEXT("hold E"); }
		return TEXT("no flash");
	case EStillControl::Scrape:
		if (S.bFouled) { OutColour = Amber; }
		return FString::Printf(TEXT("coke %d%%"), FMath::FloorToInt(S.Coke));
	case EStillControl::Repair:
		if (S.Status == EStillStatus::Failed) { OutColour = Amber; return TEXT("hold E"); }
		return FString::Printf(TEXT("cond %d%%"), FMath::FloorToInt(S.Condition));
	case EStillControl::Calibrate:
		if (S.Calibration < Rates.CalWarn) { OutColour = Amber; }
		return FString::Printf(TEXT("cal %d%%"), FMath::FloorToInt(S.Calibration * 100.f));
	default: return FString();
	}
}
bool AStillControl::IsRepeatingHold_Implementation() const { ERefineryKnob Knob; int32 Step; return KnobAction(Knob, Step); }
float AStillControl::GetInteractHoldSeconds_Implementation() const
{
	if (IsRepeatingHold_Implementation()) { return .25f; }
	switch (Spec.Kind)
	{
	case EStillControl::Reset: return 4.f;
	case EStillControl::Purge: return 5.f;
	case EStillControl::Scrape: return 8.f;
	case EStillControl::Repair: return 12.f;
	case EStillControl::Calibrate: return 3.f;
	default: return 0.f;
	}
}
FText AStillControl::GetHoldRefusal_Implementation() const
{
	const auto* S = GetStillState(); if (!S) { return FText::FromString(TEXT("missing still")); }
	bool bAllowed = true;
	switch (Spec.Kind)
	{
	case EStillControl::Reset: bAllowed = S->Status == EStillStatus::Popped; break;
	case EStillControl::Purge: bAllowed = S->Status == EStillStatus::Flash; break;
	case EStillControl::Scrape: bAllowed = S->Status == EStillStatus::Idle && S->CurrentCask.IsNone() && S->Hopper.IsEmpty(); break;
	case EStillControl::Repair: bAllowed = S->Status == EStillStatus::Failed; break;
	case EStillControl::Calibrate: bAllowed = S->Status != EStillStatus::Failed; break;
	default: break;
	}
	return bAllowed ? FText::GetEmpty() : FText::FromString(RefineryLayouts::StatusName(S->Status));
}
FText AStillControl::GetInteractPrompt_Implementation() const
{
	const auto* S = GetStillState();
	if (Spec.Kind == EStillControl::Hopper)
	{
		const auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const auto* Crew = PC ? Cast<AAirlockCharacter>(PC->GetPawn()) : nullptr;
		if (Crew && Crew->CarriedCrate)
		{
			return FText::FromString(FString::Printf(TEXT("Load %s into %s"), *Crew->CarriedCrate->GetCargoId().ToString(),
				*Spec.StillId.ToString().Replace(TEXT("_"), TEXT(" "))));
		}
		return FText::FromString(FString::Printf(TEXT("Hopper %d/3 - carry a cask here"), S ? S->Hopper.Num() : 0));
	}
	ERefineryKnob Knob; int32 Step;
	if (KnobAction(Knob, Step))
	{
		FString Name = RefineryLayouts::KnobName(Knob); Name[0] = FChar::ToUpper(Name[0]);
		return FText::FromString(Name + (Step > 0 ? TEXT(" + (hold to turn)") : TEXT(" - (hold to turn)")));
	}
	const bool bAllowed = GetHoldRefusal_Implementation().IsEmpty();
	switch (Spec.Kind)
	{
	case EStillControl::Reset: return FText::FromString(bAllowed ? TEXT("Reset the trap (hold E)") : TEXT("Reset - the still is not popped"));
	case EStillControl::Purge: return FText::FromString(bAllowed ? TEXT("Purge the separator (hold E)") : TEXT("Purge - the still has not flashed"));
	case EStillControl::Scrape: return FText::FromString(bAllowed ? TEXT("Scrape the coke (hold E)") : TEXT("Scrape - the still is running"));
	case EStillControl::Repair: return FText::FromString(bAllowed ? TEXT("Repair the still (hold E)") : TEXT("Repair - the still has not failed"));
	case EStillControl::Calibrate: return FText::FromString(TEXT("Calibrate the trap (hold E)"));
	default: return FText::GetEmpty();
	}
}
void AStillControl::Interact_Implementation(APawn* Pawn)
{
	if (Spec.Kind == EStillControl::Hopper)
	{
		if (auto* Crew = Cast<AAirlockCharacter>(Pawn); Crew && Crew->CarriedCrate) { Crew->ServerLoadStill(this); }
	}
}
void AStillControl::OnHoldComplete_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld()) { return; }
	auto* GM = GetWorld()->GetAuthGameMode<AAirlockGameMode>();
	const auto* S = GetStillState(); if (!GM || !S) { return; }
	ERefineryKnob Knob; int32 Step;
	if (KnobAction(Knob, Step))
	{
		const int32 Value = KnobValue(*S);
		if ((Step < 0 && Value <= 0) || (Step > 0 && Value >= FRefineryRates().KnobSteps)) { return; }
		GM->RefineryStepKnob(Spec.StillId, Knob, Step);
	}
	else { ERefineryLever Lever; if (LeverAction(Lever)) { GM->RefineryLever(Spec.StillId, Lever); } }
}
