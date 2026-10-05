#pragma once

#include "CoreMinimal.h"
#include "PreviewScene.h"
#include "UObject/GCObject.h"
#include "PixelFormat.h"

class UStaticMesh;
class UStaticMeshComponent;
class UDirectionalLightComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UIconForgeSettings;
struct FPostProcessSettings;

/** Owns the preview world: mesh, 3 lights, capture component. Renders icons. */
class FIconForgeStudio : public FGCObject
{
public:
	FIconForgeStudio();
	virtual ~FIconForgeStudio() override;

	FPreviewScene& GetScene() { return *Scene; }
	void SetMesh(UStaticMesh* Mesh);
	UStaticMesh* GetMesh() const;
	FBoxSphereBounds GetBounds() const;

	void ApplySettings(const UIconForgeSettings& S, const FRotator& CameraRotation);
	void FillPostProcess(const UIconForgeSettings& S, FPostProcessSettings& PP) const;

	/** Renders final icon (BGRA, straight alpha) at S.Width x S.Height. TanHalfVFov = tan(vertical FOV / 2) */
	bool Render(const UIconForgeSettings& S, const FVector& CamLoc, const FRotator& CamRot, float TanHalfVFov, TArray<FColor>& OutPixels);

	/** Frees the (potentially large) render targets kept between shots of a batch. */
	void ReleaseTargets();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FIconForgeStudio"); }

private:
	UTextureRenderTarget2D* AcquireTarget(TObjectPtr<UTextureRenderTarget2D>& Slot, int32 W, int32 H, EPixelFormat Format, bool bLinear);

	TUniquePtr<FPreviewScene> Scene;
	TObjectPtr<UStaticMeshComponent> MeshComp = nullptr;
	TObjectPtr<UDirectionalLightComponent> Key = nullptr;
	TObjectPtr<UDirectionalLightComponent> Fill = nullptr;
	TObjectPtr<UDirectionalLightComponent> Rim = nullptr;
	TObjectPtr<USceneCaptureComponent2D> Capture = nullptr;
	TObjectPtr<UTextureRenderTarget2D> ColorTarget = nullptr;
	TObjectPtr<UTextureRenderTarget2D> MaskTarget = nullptr;
	float AppliedSky = -1.f;
};
