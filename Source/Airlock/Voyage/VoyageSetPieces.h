// Copyright Airlock project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Voyage/VoyageTypes.h"
enum class ESetPieceKind : uint8 { FuelLeak };
struct AIRLOCK_API FVoyageSetPiece
{
	float AtSeconds = 0;
	ESetPieceKind Kind = ESetPieceKind::FuelLeak;
	FText CaptainLine;
};
struct AIRLOCK_API FVoyageSetPieces
{
	static TArray<FVoyageSetPiece> Build(const FVoyageSpec& Spec);
};
