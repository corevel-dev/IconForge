#include "Style/IconForgeStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> FIconForgeStyle::StyleSet;

FLinearColor FIconForgeStyle::Hex(uint32 RGB, float Alpha)
{
	FLinearColor C(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, 255));
	C.A = Alpha;
	return C;
}

FSlateFontInfo FIconForgeStyle::Font(FName Weight, int32 Size)
{
	return FCoreStyle::GetDefaultFontStyle(Weight, Size);
}

const ISlateStyle& FIconForgeStyle::Get()
{
	if (!StyleSet.IsValid()) { Initialize(); }
	return *StyleSet;
}

FName FIconForgeStyle::GetStyleSetName() { static FName Name(TEXT("IconForgeStyle")); return Name; }
const FSlateBrush* FIconForgeStyle::Brush(FName Name) { return Get().GetBrush(Name); }

void FIconForgeStyle::Initialize()
{
	if (StyleSet.IsValid()) { return; }
	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());
	FSlateStyleSet& S = *StyleSet;

	const FLinearColor White = FLinearColor::White;
	const FLinearColor Clear = FLinearColor::Transparent;
	FLinearColor AccentSoft = Accent(); AccentSoft.A = 0.18f;
	FLinearColor AccentMid  = Accent(); AccentMid.A  = 0.32f;

	// ---- Surfaces ----
	S.Set("IconForge.White",       new FSlateColorBrush(White));
	S.Set("IconForge.Background",  new FSlateColorBrush(Background()));
	S.Set("IconForge.Panel",       new FSlateRoundedBoxBrush(Panel(), 10.f, Border(), 1.f));
	S.Set("IconForge.Raised",      new FSlateRoundedBoxBrush(Raised(), 8.f, Border(), 1.f));
	S.Set("IconForge.Stage",       new FSlateRoundedBoxBrush(Background(), 8.f, Border(), 1.f));
	S.Set("IconForge.Rounded",     new FSlateRoundedBoxBrush(White, 10.f));
	S.Set("IconForge.Rounded.Sm",  new FSlateRoundedBoxBrush(White, 5.f));
	S.Set("IconForge.Pill",        new FSlateRoundedBoxBrush(White, 9.f));
	S.Set("IconForge.Circle",      new FSlateRoundedBoxBrush(White, 5.f));
	S.Set("IconForge.Outline",     new FSlateRoundedBoxBrush(Clear, 7.f, Accent(), 2.f));
	S.Set("IconForge.Outline.Dim", new FSlateRoundedBoxBrush(Clear, 7.f, Border(), 1.f));
	S.Set("IconForge.Logo",        new FSlateRoundedBoxBrush(Accent(), 7.f));
	S.Set("IconForge.Divider",     new FSlateColorBrush(Border()));

	// ---- Images from <Plugin>/Resources ----
	{
		// Plugin is installed in <Project>/Plugins/IconForge (no "Projects" module dependency needed)
		const FString DonatePng = FPaths::ProjectPluginsDir() / TEXT("IconForge/Resources/DonationAlerts.png");
		if (FPaths::FileExists(DonatePng))
		{
			S.Set("IconForge.Donate", new FSlateImageBrush(DonatePng, FVector2D(16.f, 18.6f)));
		}
		else
		{
			S.Set("IconForge.Donate", new FSlateRoundedBoxBrush(Forge(), 4.f, FVector2D(14.f, 14.f)));
		}
	}

	// ---- Buttons ----
	auto MakeButton = [](const FLinearColor& N, const FLinearColor& NB, const FLinearColor& H, const FLinearColor& HB,
		const FLinearColor& P, const FLinearColor& PB, float R, const FMargin& Pad)
	{
		return FButtonStyle()
			.SetNormal (FSlateRoundedBoxBrush(N, R, NB, 1.f))
			.SetHovered(FSlateRoundedBoxBrush(H, R, HB, 1.f))
			.SetPressed(FSlateRoundedBoxBrush(P, R, PB, 1.f))
			.SetDisabled(FSlateRoundedBoxBrush(Panel(), R, Border(), 1.f))
			.SetNormalPadding(Pad)
			.SetPressedPadding(FMargin(Pad.Left, Pad.Top + 1.f, Pad.Right, Pad.Bottom - 1.f));
	};
	S.Set("IconForge.Button",        MakeButton(Raised(), Border(), Hover(), BorderHi(), Panel(), Accent(), 7.f, FMargin(12.f, 6.f)));
	S.Set("IconForge.Button.Small",  MakeButton(Raised(), Border(), Hover(), BorderHi(), Panel(), Accent(), 6.f, FMargin(8.f, 4.f)));
	S.Set("IconForge.Button.Accent", MakeButton(AccentSoft, Accent(), AccentMid, Accent(), AccentSoft, Accent(), 7.f, FMargin(12.f, 6.f)));
	S.Set("IconForge.Button.Ghost",  MakeButton(Clear, Clear, Raised(), Border(), Panel(), Accent(), 7.f, FMargin(4.f, 4.f)));

	// The big "Shot!" button: solid accent
	const FButtonStyle Primary = FButtonStyle()
		.SetNormal (FSlateRoundedBoxBrush(Accent(), 8.f))
		.SetHovered(FSlateRoundedBoxBrush(Hex(0x95A2FF), 8.f))
		.SetPressed(FSlateRoundedBoxBrush(Hex(0x6676F0), 8.f))
		.SetDisabled(FSlateRoundedBoxBrush(Raised(), 8.f, Border(), 1.f))
		.SetNormalPadding(FMargin(18.f, 7.f))
		.SetPressedPadding(FMargin(18.f, 8.f, 18.f, 6.f));
	S.Set("IconForge.Button.Primary", Primary);

	// ---- Chips (toggle buttons) ----
	const FCheckBoxStyle Chip = FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
		.SetUncheckedImage(FSlateRoundedBoxBrush(Panel(), 10.f, Border(), 1.f))
		.SetUncheckedHoveredImage(FSlateRoundedBoxBrush(Raised(), 10.f, BorderHi(), 1.f))
		.SetUncheckedPressedImage(FSlateRoundedBoxBrush(Panel(), 10.f, BorderHi(), 1.f))
		.SetCheckedImage(FSlateRoundedBoxBrush(AccentSoft, 10.f, Accent(), 1.f))
		.SetCheckedHoveredImage(FSlateRoundedBoxBrush(AccentMid, 10.f, Accent(), 1.f))
		.SetCheckedPressedImage(FSlateRoundedBoxBrush(AccentSoft, 10.f, Accent(), 1.f))
		.SetPadding(FMargin(9.f, 3.f));
	S.Set("IconForge.Chip", Chip);

	// ---- Typography ----
	auto MakeText = [](FName Weight, int32 Size, const FLinearColor& Color)
	{
		return FTextBlockStyle().SetFont(Font(Weight, Size)).SetColorAndOpacity(Color);
	};
	S.Set("IconForge.Text.Display", MakeText("Bold",    17, Text()));
	S.Set("IconForge.Text.Title",   MakeText("Bold",    13, Text()));
	S.Set("IconForge.Text.H2",      MakeText("Bold",    10, Text()));
	S.Set("IconForge.Text.Body",    MakeText("Regular", 10, Text()));
	S.Set("IconForge.Text.Muted",   MakeText("Regular",  9, TextMuted()));
	S.Set("IconForge.Text.Caption", MakeText("Bold",     8, TextDim()));
	S.Set("IconForge.Text.Mono",    MakeText("Mono",     9, TextMuted()));
	S.Set("IconForge.Text.Button",  MakeText("Bold",     9, Text()));
	S.Set("IconForge.Text.Shot",    MakeText("Bold",    12, Background()));

	FSlateStyleRegistry::RegisterSlateStyle(S);
}

void FIconForgeStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}
