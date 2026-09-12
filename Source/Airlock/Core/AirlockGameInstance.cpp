// Copyright Airlock project. All Rights Reserved.

#include "Core/AirlockGameInstance.h"
#include "Airlock.h"
#include "Engine/GameViewportClient.h"
#include "UI/SAirlockTransition.h"

bool UAirlockGameInstance::EnsureOverlay()
{
	if (Overlay.IsValid())
	{
		return true;
	}
	UGameViewportClient* Viewport = GetGameViewportClient();
	if (!Viewport)
	{
		return false; // dedicated server / no viewport yet
	}
	Overlay = SNew(SAirlockTransition);
	Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), /*ZOrder*/ 1000);
	return true;
}

void UAirlockGameInstance::ShowTransition(const FText& Title, const FText& Subtitle, float FadeSeconds)
{
	if (!EnsureOverlay())
	{
		return;
	}
	Overlay->SetTexts(Title, Subtitle);
	Overlay->FadeTo(1.f, FadeSeconds);
	bTransitionShown = true;
	UE_LOG(LogAirlock, Log, TEXT("Transition card: %s / %s"), *Title.ToString(), *Subtitle.ToString());
}

void UAirlockGameInstance::HideTransition(float FadeSeconds)
{
	if (!Overlay.IsValid() || !bTransitionShown)
	{
		return;
	}
	bTransitionShown = false;
	Overlay->FadeTo(0.f, FadeSeconds);
}

void UAirlockGameInstance::Shutdown()
{
	if (Overlay.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameViewportClient())
		{
			Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
		}
		Overlay.Reset();
	}
	Super::Shutdown();
}
