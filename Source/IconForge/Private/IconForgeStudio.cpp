#include "IconForgeStudio.h"
#include "IconForgeSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Scene.h"
#include "StaticMeshCompiler.h"
#include "TextureCompiler.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "Misc/ScopeExit.h"

static UDirectionalLightComponent* MakeLight(FPreviewScene& Scene)
{
	UDirectionalLightComponent* L = NewObject<UDirectionalLightComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	L->SetMobility(EComponentMobility::Movable);
	L->SetCastShadows(true);
	L->bAffectsWorld = true;
	Scene.AddComponent(L, FTransform::Identity);
	return L;
}

FIconForgeStudio::FIconForgeStudio()
{
	Scene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues()
		.SetCreatePhysicsScene(false)
		.ShouldSimulatePhysics(false)
		.SetLightBrightness(0.f)
		.SetSkyBrightness(1.f));

	MeshComp = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	MeshComp->SetMobility(EComponentMobility::Movable);
	Scene->AddComponent(MeshComp, FTransform::Identity);

	Key = MakeLight(*Scene);
	Key->ForwardShadingPriority = 10;   // avoid "Multiple directional lights are competing" warning
	Fill = MakeLight(*Scene);
	Fill->SetCastShadows(false);
	Rim = MakeLight(*Scene);
	Rim->SetCastShadows(false);

	Capture = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyComponent(MeshComp);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->ShowFlags.SetGrain(false);
	Capture->ShowFlags.SetVignette(false);
	Capture->ShowFlags.SetEyeAdaptation(false);
	Scene->AddComponent(Capture, FTransform::Identity);
}

FIconForgeStudio::~FIconForgeStudio()
{
	ReleaseTargets();
}

void FIconForgeStudio::AddReferencedObjects(FReferenceCollector& C)
{
	C.AddReferencedObject(MeshComp);
	C.AddReferencedObject(Key);
	C.AddReferencedObject(Fill);
	C.AddReferencedObject(Rim);
	C.AddReferencedObject(Capture);
	C.AddReferencedObject(ColorTarget);
	C.AddReferencedObject(MaskTarget);
}

void FIconForgeStudio::SetMesh(UStaticMesh* Mesh)
{
	if (Mesh) { FStaticMeshCompilingManager::Get().FinishCompilation({ Mesh }); }
	MeshComp->SetStaticMesh(Mesh);
	MeshComp->MarkRenderStateDirty();
	MeshComp->UpdateBounds();
}

UStaticMesh* FIconForgeStudio::GetMesh() const { return MeshComp->GetStaticMesh(); }
FBoxSphereBounds FIconForgeStudio::GetBounds() const { return MeshComp->Bounds; }

void FIconForgeStudio::ApplySettings(const UIconForgeSettings& S, const FRotator& CamRot)
{
	MeshComp->SetWorldRotation(S.MeshRotation);
	MeshComp->UpdateBounds();

	const float YawOffset = S.bLightsFollowCamera ? CamRot.Yaw : 0.f;
	auto Set = [&](UDirectionalLightComponent* L, const FRotator& R, float I, const FLinearColor& Col)
	{
		// The setters below are no-ops when the value did not change, so this is cheap to call every frame
		L->SetWorldRotation(FRotator(R.Pitch, R.Yaw + YawOffset, R.Roll));
		L->SetIntensity(I);
		L->SetLightColor(Col);
		L->SetVisibility(I > 0.f);
	};
	// Relative to camera: yaw 0 = light comes from the camera, 180 = from behind the object (rim)
	Set(Key, S.KeyRotation, S.KeyIntensity, S.KeyColor);
	Key->SetCastShadows(S.bKeyShadows);
	Set(Fill, S.FillRotation, S.FillIntensity, S.FillColor);
	Set(Rim, S.RimRotation, S.RimIntensity, S.RimColor);

	// SetSkyBrightness can trigger a sky recapture: only call it when the value really changes
	if (!FMath::IsNearlyEqual(AppliedSky, S.SkyIntensity))
	{
		AppliedSky = S.SkyIntensity;
		Scene->SetSkyBrightness(S.SkyIntensity);
	}
}

void FIconForgeStudio::FillPostProcess(const UIconForgeSettings& S, FPostProcessSettings& PP) const
{
	PP.bOverride_AutoExposureMethod = true;               PP.AutoExposureMethod = AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true; PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_AutoExposureBias = true;                 PP.AutoExposureBias = S.ExposureBias;
	PP.bOverride_BloomIntensity = true;                   PP.BloomIntensity = S.BloomIntensity;
	PP.bOverride_ColorSaturation = true;                  PP.ColorSaturation = FVector4(1, 1, 1, S.Saturation);
	PP.bOverride_ColorContrast = true;                    PP.ColorContrast = FVector4(1, 1, 1, S.Contrast);
	PP.bOverride_VignetteIntensity = true;                PP.VignetteIntensity = 0.f;
	PP.bOverride_MotionBlurAmount = true;                 PP.MotionBlurAmount = 0.f;
	PP.bOverride_AmbientOcclusionIntensity = true;        PP.AmbientOcclusionIntensity = S.AmbientOcclusion;

	// Lumen needs many frames to converge: a single capture would be noisy / dark and differ from the viewport.
	// Icons are lit by the 3 lights + sky only, which is deterministic and identical in viewport and output.
	PP.bOverride_DynamicGlobalIlluminationMethod = true;  PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PP.bOverride_ReflectionMethod = true;                 PP.ReflectionMethod = EReflectionMethod::None;
}

UTextureRenderTarget2D* FIconForgeStudio::AcquireTarget(TObjectPtr<UTextureRenderTarget2D>& Slot, int32 W, int32 H, EPixelFormat Format, bool bLinear)
{
	if (!Slot || Slot->SizeX != W || Slot->SizeY != H)
	{
		if (Slot) { Slot->ReleaseResource(); }
		Slot = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
		Slot->ClearColor = FLinearColor::Transparent;
		Slot->InitCustomFormat(W, H, Format, bLinear);
		Slot->UpdateResourceImmediate(true);
	}
	return Slot;
}

void FIconForgeStudio::ReleaseTargets()
{
	// The tab can be destroyed during editor shutdown, after UObjects were torn down
	if (IsEngineExitRequested() || !UObjectInitialized())
	{
		ColorTarget = nullptr; MaskTarget = nullptr;
		return;
	}
	if (IsValid(Capture)) { Capture->TextureTarget = nullptr; }
	if (IsValid(ColorTarget)) { ColorTarget->ReleaseResource(); }
	if (IsValid(MaskTarget))  { MaskTarget->ReleaseResource(); }
	ColorTarget = nullptr;
	MaskTarget = nullptr;
}

bool FIconForgeStudio::Render(const UIconForgeSettings& S, const FVector& CamLoc, const FRotator& CamRot, float TanHalfVFov, TArray<FColor>& Out)
{
	if (!MeshComp->GetStaticMesh() || TanHalfVFov <= 0.f) { return false; }

	// Make sure everything is ready: shaders, textures, mesh, full-res mips.
	if (GShaderCompilingManager) { GShaderCompilingManager->FinishAllCompilation(); }
	FTextureCompilingManager::Get().FinishAllCompilation();
	MeshComp->SetForcedLodModel(1); // 1 = LOD0
	ON_SCOPE_EXIT { MeshComp->SetForcedLodModel(0); Capture->TextureTarget = nullptr; };   // also on early return
	MeshComp->PrestreamTextures(30.f, true);
	IStreamingManager::Get().StreamAllResources(5.f);
	ApplySettings(S, CamRot);
	FlushRenderingCommands();

	const int32 SS = S.GetEffectiveSuperSampling();
	const int32 W = S.Width * SS, H = S.Height * SS;

	UTextureRenderTarget2D* RTColor = AcquireTarget(ColorTarget, W, H, PF_B8G8R8A8, false);
	UTextureRenderTarget2D* RTMask  = AcquireTarget(MaskTarget, W, H, PF_FloatRGBA, true);
	if (!RTColor || !RTMask) { return false; }

	Capture->SetWorldLocationAndRotation(CamLoc, CamRot);
	// Same vertical coverage as the viewport => identical framing
	Capture->FOVAngle = FMath::RadiansToDegrees(2.f * FMath::Atan(TanHalfVFov * S.GetAspect()));
	Capture->PostProcessSettings = FPostProcessSettings();
	FillPostProcess(S, Capture->PostProcessSettings);
	Capture->PostProcessBlendWeight = 1.f;

	// Pass 1: final tonemapped colour (background = black -> premultiplied colour)
	Capture->TextureTarget = RTColor;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->CaptureScene();
	TArray<FColor> Color;
	if (FTextureRenderTargetResource* R = RTColor->GameThread_GetRenderTargetResource()) { R->ReadPixels(Color); }

	// Pass 2: HDR scene colour, alpha channel = inverse opacity -> coverage mask
	Capture->TextureTarget = RTMask;
	Capture->CaptureSource = SCS_SceneColorHDR;
	Capture->CaptureScene();
	TArray<FFloat16Color> Mask;
	if (FTextureRenderTargetResource* R = RTMask->GameThread_GetRenderTargetResource()) { R->ReadFloat16Pixels(Mask); }

	if (Color.Num() != W * H || Mask.Num() != W * H) { return false; }

	for (int32 i = 0; i < Color.Num(); ++i)
	{
		const float A = FMath::Clamp(1.f - Mask[i].A.GetFloat(), 0.f, 1.f);
		Color[i].A = (uint8)FMath::RoundToInt(A * 255.f);
	}

	// Downscale premultiplied data (no dark fringes)
	TArray<FColor> Small;
	if (SS > 1) { FImageUtils::ImageResize(W, H, Color, S.Width, S.Height, Small, false, false); }
	else { Small = MoveTemp(Color); }
	if (Small.Num() != S.Width * S.Height) { return false; }

	const FColor Bg = S.BackgroundColor.ToFColor(true);
	Out.SetNumUninitialized(Small.Num());
	for (int32 i = 0; i < Small.Num(); ++i)
	{
		const FColor& P = Small[i];
		const float A = P.A / 255.f;
		FColor R;
		if (S.bTransparentBackground)
		{
			const float Inv = A > 0.002f ? 1.f / A : 0.f;
			R.R = (uint8)FMath::Clamp(FMath::RoundToInt(P.R * Inv), 0, 255);
			R.G = (uint8)FMath::Clamp(FMath::RoundToInt(P.G * Inv), 0, 255);
			R.B = (uint8)FMath::Clamp(FMath::RoundToInt(P.B * Inv), 0, 255);
			R.A = P.A;
		}
		else
		{
			R.R = (uint8)FMath::Clamp(FMath::RoundToInt(P.R + Bg.R * (1.f - A)), 0, 255);
			R.G = (uint8)FMath::Clamp(FMath::RoundToInt(P.G + Bg.G * (1.f - A)), 0, 255);
			R.B = (uint8)FMath::Clamp(FMath::RoundToInt(P.B + Bg.B * (1.f - A)), 0, 255);
			R.A = 255;
		}
		Out[i] = R;
	}
	return true;
}
