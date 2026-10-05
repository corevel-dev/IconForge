#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"

class FIconForgeStudio;
class UIconForgeSettings;

/** Camera used for rendering */
struct FIconForgeView
{
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator(-20.f, -135.f, 0.f);
	float TanHalfVFov = 0.f;
	bool bValid = false;
};

/** Orbit camera state (target + spherical coordinates) */
struct FIconForgeOrbit
{
	FVector Target = FVector::ZeroVector;
	float Yaw = -135.f;
	float Pitch = -20.f;
	float Distance = 300.f;
};

/** Returns true when the key was consumed. */
DECLARE_DELEGATE_RetVal_OneParam(bool, FIconForgeHotkey, const FKeyEvent&);

class FIconForgeViewportClient : public FEditorViewportClient
{
public:
	FIconForgeViewportClient(FIconForgeStudio& InStudio, UIconForgeSettings* InSettings, const TSharedRef<SEditorViewport>& InWidget);
	virtual void Tick(float DeltaSeconds) override;
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void OverridePostProcessSettings(FSceneView& View) override;
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoViewIndex = INDEX_NONE) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;

	// --- camera control (driven by SIconForgeViewport mouse handlers) ---
	void Orbit(const FVector2D& PixelDelta, bool bPrecise);
	void Pan(const FVector2D& PixelDelta, float ViewportHeightPx, bool bPrecise);
	void Zoom(float Steps, bool bPrecise);

	/** Fit mesh. bInstant = no smoothing (used for batch / mesh select). Returns final camera. */
	FIconForgeView FrameMesh(const FRotator* ForcedRotation = nullptr, bool bInstant = false);
	/** Jump to target immediately and return the exact camera that will be used for a shot */
	FIconForgeView SnapAndGetView();
	FRotator GetCameraRotation() const { return FRotator(Current.Pitch, Current.Yaw, 0.f); }
	FRotator GetDesiredRotation() const { return FRotator(Desired.Pitch, Desired.Yaw, 0.f); }
	float GetTanHalfVFov() const;

	/** Exact orbit state (used to restore the user's framing after a batch). */
	const FIconForgeOrbit& GetOrbit() const { return Desired; }
	void SetOrbit(const FIconForgeOrbit& Orbit, bool bInstant = true);

	/** FOV / resolution changed: forget the measured projection until the next rendered frame. */
	void ResetCachedFov() { RenderedTanV = 0.f; }

private:
	void ApplyCamera();
	FIconForgeView MakeView(const FIconForgeOrbit& O) const;
	float MinDistance() const;

	FIconForgeStudio& Studio;
	TWeakObjectPtr<UIconForgeSettings> Settings;
	FIconForgeOrbit Current;
	FIconForgeOrbit Desired;
	float RenderedTanV = 0.f;
};

class SIconForgeViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SIconForgeViewport) : _Studio(nullptr), _Settings(nullptr) {}
		SLATE_ARGUMENT(FIconForgeStudio*, Studio)
		SLATE_ARGUMENT(UIconForgeSettings*, Settings)
		/** Icon Forge hotkeys are offered here BEFORE the editor viewport commands (W/E/R/Space would be eaten otherwise). */
		SLATE_EVENT(FIconForgeHotkey, OnHotkey)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	TSharedPtr<FIconForgeViewportClient> GetClient() const { return Client; }

	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override;
	virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& G, const FPointerEvent& E) override;
	virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override;
	virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& E) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& G, const FPointerEvent& E) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	virtual void OnFocusViewportToSelection() override;

private:
	FText GetInfoText() const;
	FText GetSizeText() const;
	bool HasMesh() const;

	FIconForgeStudio* Studio = nullptr;
	UIconForgeSettings* Settings = nullptr;
	TSharedPtr<FIconForgeViewportClient> Client;
	FIconForgeHotkey OnHotkey;
	bool bDragging = false;
};
