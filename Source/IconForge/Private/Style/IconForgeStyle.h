#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * Design system of Icon Forge (same visual language as Causality Engine):
 * dark low-contrast surfaces, one saturated accent, rounded geometry, generous spacing.
 */
class FIconForgeStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static const ISlateStyle& Get();
	static FName GetStyleSetName();
	static const FSlateBrush* Brush(FName Name);

	static FLinearColor Hex(uint32 RGB, float Alpha = 1.f);
	static FSlateFontInfo Font(FName Weight, int32 Size);

	// Palette
	static FLinearColor Background()  { return Hex(0x0B0D11); }
	static FLinearColor Panel()       { return Hex(0x12151B); }
	static FLinearColor Raised()      { return Hex(0x191D25); }
	static FLinearColor Hover()       { return Hex(0x232A36); }
	static FLinearColor Border()      { return Hex(0x262C38); }
	static FLinearColor BorderHi()    { return Hex(0x3A4252); }
	static FLinearColor Text()        { return Hex(0xE9EDF3); }
	static FLinearColor TextMuted()   { return Hex(0x9AA3B2); }
	static FLinearColor TextDim()     { return Hex(0x5D6676); }
	static FLinearColor Accent()      { return Hex(0x7C8CFF); }
	static FLinearColor Live()        { return Hex(0x3DDC97); }
	static FLinearColor Warn()        { return Hex(0xFFB547); }
	static FLinearColor Bad()         { return Hex(0xFF5C6C); }
	static FLinearColor Forge()       { return Hex(0xFF8A3D); }

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
