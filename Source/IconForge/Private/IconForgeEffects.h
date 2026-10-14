#pragma once

#include "CoreMinimal.h"

class UIconForgeSettings;

/**
 * CPU image effects applied to a rendered icon (colour grading, retro pixelation, palettes, dithering,
 * outlines, shadow, glow, gradient backgrounds, frames, CRT). Pure functions: no UObjects are created,
 * so the same code drives both the live preview and the final saved icon (what you see = what you get).
 */
namespace IconForgeFX
{
	/**
	 * In:  straight-alpha pixels (W x H) exactly as rendered, transparent background.
	 * Out: final pixels. W/H change when RetroFX.bOutputNativePixels is on.
	 * Also composites the background (Output solid colour or Stylize gradient).
	 */
	void Process(const UIconForgeSettings& S, TArray<FColor>& InOutPixels, int32& InOutW, int32& InOutH);

	/** Output size Process() will produce for the given settings. */
	FIntPoint GetOutputSize(const UIconForgeSettings& S);

	/** True when any effect differs from the neutral defaults. */
	bool HasAnyEffect(const UIconForgeSettings& S);
}
