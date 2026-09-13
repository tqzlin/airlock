// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StationSign.generated.h"

class UTextRenderComponent;

/** One code-spawned sign: two lines of text facing the spec's yaw. Spawned by the game mode from StationSigns. */
USTRUCT()
struct AIRLOCK_API FStationSignSpec
{
	GENERATED_BODY()
	UPROPERTY() FName Id;
	UPROPERTY() FString Line1;
	UPROPERTY() FString Line2;
	/** Location of the sign's centre; the text faces the transform's forward (+X at yaw 0). */
	UPROPERTY() FTransform Transform;
	UPROPERTY() float WorldSize = 40.f;
	UPROPERTY() FLinearColor Colour = FLinearColor(0.55f, 0.85f, 1.f);
};

UCLASS()
class AIRLOCK_API AStationSign : public AActor
{
	GENERATED_BODY()
public:
	AStationSign();
	void Initialize(const FStationSignSpec& InSpec);
	FName GetSignId() const { return Spec.Id; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SignRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Text;
protected:
	virtual void BeginPlay() override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Spec) FStationSignSpec Spec;
	UFUNCTION() void OnRep_Spec();
};

namespace StationSigns
{
	/** The signs of a section, or an empty array. Positions live here, never in a map. */
	AIRLOCK_API const TArray<FStationSignSpec>& Find(FName SectionId);
}
