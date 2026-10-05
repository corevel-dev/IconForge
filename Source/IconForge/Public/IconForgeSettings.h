#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "IconForgeSettings.generated.h"

UENUM()
enum class EIconForgeCameraPreset : uint8 { Front, Back, Left, Right, Top, ThreeQuarter, Isometric };

UENUM()
enum class EIconForgeLightPreset : uint8 { Studio, Soft, Dramatic, CoolRim, Warm, Flat };

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

	// ---------- Session state (not shown in the Settings panel) ----------
	UPROPERTY()
	bool bProjectContentOnly = true;
	UPROPERTY()
	bool bHideLODMeshes = true;
	/** Last orbit angle, restored when the tool is reopened */
	UPROPERTY()
	FRotator LastCameraRotation = FRotator(-20.f, -135.f, 0.f);

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

	static FString GetSessionFile();
	static FString GetPresetDir();
	static FString SanitizeName(const FString& In);
};
