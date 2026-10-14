#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "IconForgeSettings.generated.h"

UENUM()
enum class EIconForgeCameraPreset : uint8 { Front, Back, Left, Right, Top, ThreeQuarter, Isometric };

UENUM()
enum class EIconForgeLightPreset : uint8 { Studio, Soft, Dramatic, CoolRim, Warm, Flat };

/** Colour reduction mode (retro look). */
UENUM()
enum class EIconForgePalette : uint8
{
	None,
	Posterize       UMETA(ToolTip = "N levels per channel (Posterize Levels). 32 = SNES / PS1 15-bit colour"),
	GameBoy         UMETA(DisplayName = "Game Boy (4)"),
	GameBoyPocket   UMETA(DisplayName = "Game Boy Pocket (4)"),
	NES             UMETA(DisplayName = "NES (54)"),
	PICO8           UMETA(DisplayName = "PICO-8 (16)"),
	C64             UMETA(DisplayName = "Commodore 64 (16)"),
	CGA             UMETA(DisplayName = "CGA mode 4 (4)"),
	EGA             UMETA(DisplayName = "EGA (16)"),
	Sweetie16       UMETA(DisplayName = "Sweetie 16"),
	VirtualBoy      UMETA(DisplayName = "Virtual Boy (4)"),
	Grayscale4      UMETA(DisplayName = "Grayscale (4)"),
	OneBit          UMETA(DisplayName = "1-bit (B/W)"),
	Auto            UMETA(DisplayName = "Auto (best N colours from the image)"),
	Custom          UMETA(DisplayName = "Custom (Custom Palette)")
};

UENUM()
enum class EIconForgeDither : uint8
{
	None,
	Bayer2          UMETA(DisplayName = "Ordered 2x2"),
	Bayer4          UMETA(DisplayName = "Ordered 4x4 (PS1 style)"),
	Bayer8          UMETA(DisplayName = "Ordered 8x8"),
	FloydSteinberg  UMETA(DisplayName = "Floyd-Steinberg"),
	Atkinson        UMETA(DisplayName = "Atkinson (classic Mac)")
};

UENUM()
enum class EIconForgeBackground : uint8
{
	FromOutput      UMETA(DisplayName = "Output setting (transparent / solid)"),
	LinearGradient  UMETA(DisplayName = "Linear gradient"),
	RadialGradient  UMETA(DisplayName = "Radial gradient (inventory slot)")
};

/** One-click looks (buttons in the FX row). */
UENUM()
enum class EIconForgeFXPreset : uint8
{
	Custom, None, GameBoy, NES, PICO8, SNES, PS1, CGA, OneBitMac, VirtualBoy, Arcade, CleanUI, Legendary, Toon
};

// ------------------------------------------------------------------ Effects (applied on the CPU after rendering)

USTRUCT()
struct FIconForgeColorFX
{
	GENERATED_BODY()

	/** Multiplies the colour (1 = unchanged). Use it to match the icon to your UI. */
	UPROPERTY(EditAnywhere, Category = "Color", meta = (ClampMin = 0, ClampMax = 3))
	float Brightness = 1.f;
	/** <1 = brighter mid-tones, >1 = darker mid-tones */
	UPROPERTY(EditAnywhere, Category = "Color", meta = (ClampMin = 0.2, ClampMax = 3))
	float Gamma = 1.f;
	UPROPERTY(EditAnywhere, Category = "Color", meta = (ClampMin = -180, ClampMax = 180))
	float HueShift = 0.f;
	UPROPERTY(EditAnywhere, Category = "Color", meta = (ClampMin = 0, ClampMax = 3))
	float Vibrance = 1.f;
	UPROPERTY(EditAnywhere, Category = "Color", meta = (HideAlphaChannel))
	FLinearColor Tint = FLinearColor(1.f, 0.6f, 0.25f);
	UPROPERTY(EditAnywhere, Category = "Color", meta = (ClampMin = 0, ClampMax = 1))
	float TintAmount = 0.f;
	/** Unsharp mask: crisper texture detail on small icons */
	UPROPERTY(EditAnywhere, Category = "Color", meta = (ClampMin = 0, ClampMax = 3))
	float Sharpen = 0.f;
};

USTRUCT()
struct FIconForgeRetroFX
{
	GENERATED_BODY()

	/** Size of one "art pixel" in output pixels (1 = off). 512 px icon + 16 = 32x32 sprite. */
	UPROPERTY(EditAnywhere, Category = "Retro", meta = (ClampMin = 1, ClampMax = 64, UIMin = 1, UIMax = 32))
	int32 PixelSize = 1;
	/** Save the small sprite itself (e.g. 32x32) instead of scaling it back up to Width x Height */
	UPROPERTY(EditAnywhere, Category = "Retro")
	bool bOutputNativePixels = false;
	/** Hard alpha (no semi-transparent edges): classic sprite look */
	UPROPERTY(EditAnywhere, Category = "Retro")
	bool bHardAlpha = false;
	UPROPERTY(EditAnywhere, Category = "Retro", meta = (ClampMin = 0.01, ClampMax = 0.99, EditCondition = "bHardAlpha"))
	float AlphaThreshold = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Retro")
	EIconForgePalette Palette = EIconForgePalette::None;
	UPROPERTY(EditAnywhere, Category = "Retro", meta = (ClampMin = 2, ClampMax = 64))
	int32 PosterizeLevels = 8;
	/** Number of colours for the Auto palette */
	UPROPERTY(EditAnywhere, Category = "Retro", meta = (ClampMin = 2, ClampMax = 64))
	int32 AutoColors = 8;
	UPROPERTY(EditAnywhere, Category = "Retro")
	TArray<FLinearColor> CustomPalette;
	/** Stretch the image brightness over the whole palette (essential for Game Boy / 1-bit) */
	UPROPERTY(EditAnywhere, Category = "Retro")
	bool bNormalizeLuminance = false;
	/** Map by brightness only (Game Boy, Virtual Boy, 1-bit look better) instead of nearest colour */
	UPROPERTY(EditAnywhere, Category = "Retro")
	bool bMatchByLuminance = false;

	UPROPERTY(EditAnywhere, Category = "Retro")
	EIconForgeDither Dither = EIconForgeDither::None;
	UPROPERTY(EditAnywhere, Category = "Retro", meta = (ClampMin = 0, ClampMax = 2))
	float DitherStrength = 1.f;
};

USTRUCT()
struct FIconForgeStylizeFX
{
	GENERATED_BODY()

	/** Outline thickness in art pixels (0 = off) */
	UPROPERTY(EditAnywhere, Category = "Stylize|Outline", meta = (ClampMin = 0, ClampMax = 32))
	int32 OutlineThickness = 0;
	UPROPERTY(EditAnywhere, Category = "Stylize|Outline")
	FLinearColor OutlineColor = FLinearColor(0.02f, 0.02f, 0.03f, 1.f);
	/** Second outline outside the first one (sticker / rarity look) */
	UPROPERTY(EditAnywhere, Category = "Stylize|Outline", meta = (ClampMin = 0, ClampMax = 32))
	int32 OuterOutlineThickness = 0;
	UPROPERTY(EditAnywhere, Category = "Stylize|Outline")
	FLinearColor OuterOutlineColor = FLinearColor::White;
	/** Dark line along the inside edge of the object (cartoon / pixel-art shading) */
	UPROPERTY(EditAnywhere, Category = "Stylize|Outline", meta = (ClampMin = 0, ClampMax = 1))
	float InnerEdgeDarken = 0.f;

	UPROPERTY(EditAnywhere, Category = "Stylize|Shadow")
	bool bDropShadow = false;
	/** Offset in output pixels */
	UPROPERTY(EditAnywhere, Category = "Stylize|Shadow", meta = (EditCondition = "bDropShadow"))
	FIntPoint ShadowOffset = FIntPoint(6, 8);
	UPROPERTY(EditAnywhere, Category = "Stylize|Shadow", meta = (ClampMin = 0, ClampMax = 64, EditCondition = "bDropShadow"))
	int32 ShadowBlur = 6;
	UPROPERTY(EditAnywhere, Category = "Stylize|Shadow", meta = (EditCondition = "bDropShadow"))
	FLinearColor ShadowColor = FLinearColor(0.f, 0.f, 0.f, 0.6f);

	UPROPERTY(EditAnywhere, Category = "Stylize|Glow", meta = (ClampMin = 0, ClampMax = 4))
	float GlowIntensity = 0.f;
	UPROPERTY(EditAnywhere, Category = "Stylize|Glow", meta = (ClampMin = 1, ClampMax = 96))
	int32 GlowRadius = 16;
	UPROPERTY(EditAnywhere, Category = "Stylize|Glow", meta = (HideAlphaChannel))
	FLinearColor GlowColor = FLinearColor(1.f, 0.55f, 0.1f);

	UPROPERTY(EditAnywhere, Category = "Stylize|Background")
	EIconForgeBackground Background = EIconForgeBackground::FromOutput;
	UPROPERTY(EditAnywhere, Category = "Stylize|Background")
	FLinearColor BackgroundA = FLinearColor(0.35f, 0.18f, 0.05f, 1.f);
	UPROPERTY(EditAnywhere, Category = "Stylize|Background")
	FLinearColor BackgroundB = FLinearColor(0.06f, 0.03f, 0.02f, 1.f);
	/** Linear gradient direction (0 = top to bottom) */
	UPROPERTY(EditAnywhere, Category = "Stylize|Background", meta = (ClampMin = -180, ClampMax = 180))
	float GradientAngle = 0.f;

	/** Border around the whole icon in output pixels (0 = off) */
	UPROPERTY(EditAnywhere, Category = "Stylize|Frame", meta = (ClampMin = 0, ClampMax = 64))
	int32 FrameThickness = 0;
	UPROPERTY(EditAnywhere, Category = "Stylize|Frame")
	FLinearColor FrameColor = FLinearColor(1.f, 0.6f, 0.15f, 1.f);
	/** Rounded corners in output pixels (also clips the background) */
	UPROPERTY(EditAnywhere, Category = "Stylize|Frame", meta = (ClampMin = 0, ClampMax = 512))
	int32 CornerRadius = 0;
};

USTRUCT()
struct FIconForgeCRTFX
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "CRT", meta = (ClampMin = 0, ClampMax = 1))
	float Scanlines = 0.f;
	/** Scanline period in output pixels */
	UPROPERTY(EditAnywhere, Category = "CRT", meta = (ClampMin = 2, ClampMax = 32))
	int32 ScanlinePeriod = 3;
	/** RGB aperture grille */
	UPROPERTY(EditAnywhere, Category = "CRT", meta = (ClampMin = 0, ClampMax = 1))
	float PhosphorMask = 0.f;
	/** Red/blue channel offset in output pixels */
	UPROPERTY(EditAnywhere, Category = "CRT", meta = (ClampMin = 0, ClampMax = 16))
	float ChromaticAberration = 0.f;
	/** Barrel distortion of a curved tube */
	UPROPERTY(EditAnywhere, Category = "CRT", meta = (ClampMin = 0, ClampMax = 0.5))
	float Curvature = 0.f;
	UPROPERTY(EditAnywhere, Category = "CRT", meta = (ClampMin = 0, ClampMax = 1))
	float Vignette = 0.f;
	UPROPERTY(EditAnywhere, Category = "CRT", meta = (ClampMin = 0, ClampMax = 0.5))
	float Noise = 0.f;
};

/**
 * All Icon Forge parameters, edited in the Settings panel.
 * Persisted as JSON (Saved/IconForge/Session.json) so class defaults stay "factory defaults"
 * and user presets can be saved/loaded as files (Saved/IconForge/Presets/*.json).
 */
UCLASS()
class ICONFORGE_API UIconForgeSettings : public UObject
{
	GENERATED_BODY()
public:
	// ---------- Output ----------
	UPROPERTY(EditAnywhere, Category = "1 Output", meta = (ClampMin = 16, ClampMax = 4096, UIMin = 16, UIMax = 2048))
	int32 Width = 512;

	UPROPERTY(EditAnywhere, Category = "1 Output", meta = (ClampMin = 16, ClampMax = 4096, UIMin = 16, UIMax = 2048))
	int32 Height = 512;

	/** Render N times bigger and downscale: smooth edges. Automatically reduced if the render would exceed 8192 px. */
	UPROPERTY(EditAnywhere, Category = "1 Output", meta = (ClampMin = 1, ClampMax = 4))
	int32 SuperSampling = 2;

	UPROPERTY(EditAnywhere, Category = "1 Output")
	bool bTransparentBackground = true;

	UPROPERTY(EditAnywhere, Category = "1 Output", meta = (EditCondition = "!bTransparentBackground"))
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.06f, 1.f);

	/** {Name} = mesh name, {BaseName} = mesh name without SM_/S_ prefix, {W}/{H} = resolution */
	UPROPERTY(EditAnywhere, Category = "1 Output")
	FString NamePattern = TEXT("T_{Name}_Icon");

	/** Off = never replace an existing icon, add _2, _3... instead */
	UPROPERTY(EditAnywhere, Category = "1 Output")
	bool bOverwriteExisting = true;

	UPROPERTY(EditAnywhere, Category = "1 Output")
	bool bSavePNG = true;

	/** Empty = <Project>/Saved/Icons. Relative paths are relative to the project folder. */
	UPROPERTY(EditAnywhere, Category = "1 Output", meta = (EditCondition = "bSavePNG"))
	FDirectoryPath PNGDirectory;

	UPROPERTY(EditAnywhere, Category = "1 Output")
	bool bCreateTextureAsset = true;

	/** Empty = /Game/Icons */
	UPROPERTY(EditAnywhere, Category = "1 Output", meta = (EditCondition = "bCreateTextureAsset", ContentDir))
	FDirectoryPath TextureAssetPath;

	UPROPERTY(EditAnywhere, Category = "1 Output", meta = (EditCondition = "bCreateTextureAsset"))
	bool bSaveTextureAssetToDisk = true;

	// ---------- Camera ----------
	/** Horizontal field of view of the icon camera */
	UPROPERTY(EditAnywhere, Category = "2 Camera", meta = (ClampMin = 5, ClampMax = 120))
	float FieldOfView = 30.f;

	/** >1 = more empty space around the object */
	UPROPERTY(EditAnywhere, Category = "2 Camera", meta = (ClampMin = 0.5, ClampMax = 3))
	float FramingPadding = 1.05f;

	UPROPERTY(EditAnywhere, Category = "2 Camera")
	bool bAutoFrameOnSelect = true;

	/** Batch: keep current viewport angle but re-frame each mesh */
	UPROPERTY(EditAnywhere, Category = "2 Camera")
	bool bAutoFrameInBatch = true;

	UPROPERTY(EditAnywhere, Category = "2 Camera")
	FRotator MeshRotation = FRotator::ZeroRotator;

	/** Rule-of-thirds grid and centre cross in the viewport (never rendered into the icon). G toggles. */
	UPROPERTY(EditAnywhere, Category = "2 Camera")
	bool bShowGuides = true;

	// ---------- Camera controls ----------
	/** Degrees per mouse pixel multiplier (LMB drag) */
	UPROPERTY(EditAnywhere, Category = "2 Camera|Controls", meta = (ClampMin = 0.05, ClampMax = 5))
	float OrbitSensitivity = 1.f;

	/** 1 = object follows the cursor exactly. Lower = slower pan (MMB or Shift+LMB) */
	UPROPERTY(EditAnywhere, Category = "2 Camera|Controls", meta = (ClampMin = 0.05, ClampMax = 3))
	float PanSensitivity = 0.5f;

	/** Wheel / RMB drag zoom speed */
	UPROPERTY(EditAnywhere, Category = "2 Camera|Controls", meta = (ClampMin = 0.05, ClampMax = 5))
	float ZoomSensitivity = 1.f;

	/** Speed multiplier while holding Ctrl (precise adjustments) */
	UPROPERTY(EditAnywhere, Category = "2 Camera|Controls", meta = (ClampMin = 0.05, ClampMax = 1))
	float PrecisionMultiplier = 0.25f;

	UPROPERTY(EditAnywhere, Category = "2 Camera|Controls")
	bool bInvertOrbitY = false;

	/** Camera smoothly glides to target (0 = instant) */
	UPROPERTY(EditAnywhere, Category = "2 Camera|Controls", meta = (ClampMin = 0, ClampMax = 40))
	float CameraSmoothing = 18.f;

	// ---------- Lighting ----------
	/** Lights are defined relative to the camera (yaw 0 = from the camera, 180 = from behind the object). */
	UPROPERTY(EditAnywhere, Category = "3 Lighting")
	bool bLightsFollowCamera = true;

	UPROPERTY(EditAnywhere, Category = "3 Lighting|Key")
	FRotator KeyRotation = FRotator(-40.f, 40.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "3 Lighting|Key", meta = (ClampMin = 0))
	float KeyIntensity = 5.f;
	UPROPERTY(EditAnywhere, Category = "3 Lighting|Key", meta = (HideAlphaChannel))
	FLinearColor KeyColor = FLinearColor(1.f, 0.96f, 0.9f);
	UPROPERTY(EditAnywhere, Category = "3 Lighting|Key")
	bool bKeyShadows = true;

	UPROPERTY(EditAnywhere, Category = "3 Lighting|Fill")
	FRotator FillRotation = FRotator(-10.f, -60.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "3 Lighting|Fill", meta = (ClampMin = 0))
	float FillIntensity = 1.8f;
	UPROPERTY(EditAnywhere, Category = "3 Lighting|Fill", meta = (HideAlphaChannel))
	FLinearColor FillColor = FLinearColor(0.75f, 0.85f, 1.f);

	UPROPERTY(EditAnywhere, Category = "3 Lighting|Rim")
	FRotator RimRotation = FRotator(-25.f, 170.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "3 Lighting|Rim", meta = (ClampMin = 0))
	float RimIntensity = 4.f;
	UPROPERTY(EditAnywhere, Category = "3 Lighting|Rim", meta = (HideAlphaChannel))
	FLinearColor RimColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "3 Lighting|Ambient", meta = (ClampMin = 0))
	float SkyIntensity = 1.f;

	// ---------- Post ----------
	UPROPERTY(EditAnywhere, Category = "4 Post Process", meta = (ClampMin = -6, ClampMax = 6))
	float ExposureBias = 0.f;
	UPROPERTY(EditAnywhere, Category = "4 Post Process", meta = (ClampMin = 0, ClampMax = 2))
	float BloomIntensity = 0.f;
	UPROPERTY(EditAnywhere, Category = "4 Post Process", meta = (ClampMin = 0, ClampMax = 2))
	float Saturation = 1.f;
	UPROPERTY(EditAnywhere, Category = "4 Post Process", meta = (ClampMin = 0, ClampMax = 2))
	float Contrast = 1.f;
	/** Screen-space ambient occlusion in creases (0 = off) */
	UPROPERTY(EditAnywhere, Category = "4 Post Process", meta = (ClampMin = 0, ClampMax = 1))
	float AmbientOcclusion = 0.5f;

	// ---------- Effects (CPU, after rendering; see live preview in RESULT) ----------
	UPROPERTY(EditAnywhere, Category = "5 Color", meta = (ShowOnlyInnerProperties))
	FIconForgeColorFX ColorFX;
	UPROPERTY(EditAnywhere, Category = "6 Retro", meta = (ShowOnlyInnerProperties))
	FIconForgeRetroFX RetroFX;
	UPROPERTY(EditAnywhere, Category = "7 Stylize", meta = (ShowOnlyInnerProperties))
	FIconForgeStylizeFX StylizeFX;
	UPROPERTY(EditAnywhere, Category = "8 CRT", meta = (ShowOnlyInnerProperties))
	FIconForgeCRTFX CRTFX;

	// ---------- Session state (not shown in the Settings panel) ----------
	UPROPERTY()
	bool bProjectContentOnly = true;
	UPROPERTY()
	bool bHideLODMeshes = true;
	/** Last orbit angle, restored when the tool is reopened */
	UPROPERTY()
	FRotator LastCameraRotation = FRotator(-20.f, -135.f, 0.f);
	/** Re-render a small preview with all effects whenever something changes */
	UPROPERTY()
	bool bLiveFXPreview = true;
	UPROPERTY()
	EIconForgeFXPreset ActiveFXPreset = EIconForgeFXPreset::None;

	FString ResolvePNGDirectory() const;
	FString ResolveTexturePath() const;
	FString MakeName(const FString& MeshName) const;
	/** Supersampling actually used (clamped so the render target stays <= 8192 px). */
	int32 GetEffectiveSuperSampling() const;
	float GetAspect() const { return (float)FMath::Max(1, Width) / (float)FMath::Max(1, Height); }

	bool SaveToFile(const FString& Path) const;
	bool LoadFromFile(const FString& Path);
	void ResetToDefaults();
	void ApplyLightPreset(EIconForgeLightPreset Preset);
	/** Resets all effects, then applies a look (may also change background / supersampling). */
	void ApplyFXPreset(EIconForgeFXPreset Preset);
	void ResetEffects();
	/** True when the icon is meant to look pixelated: texture asset gets Nearest filtering. */
	bool WantsPixelFiltering() const { return RetroFX.PixelSize > 1; }

	static FString GetSessionFile();
	static FString GetPresetDir();
	static FString SanitizeName(const FString& In);
};
