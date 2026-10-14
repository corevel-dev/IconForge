#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "IContentBrowserSingleton.h"
#include "UObject/StrongObjectPtr.h"
#include "IconForgeSettings.h"
#include "IconForgeStudio.h"
#include "SIconForgeViewport.h"

class SIconForgeViewport;
class IDetailsView;
class UTexture2D;
class UPackage;
class SBox;
class SWrapBox;
struct FAssetData;
struct FIconForgeView;

/** One rendered icon kept in the history strip */
struct FIconForgeShot
{
	TStrongObjectPtr<UTexture2D> Texture;
	TSharedPtr<FSlateBrush> Brush;
	FString Name;
	FString MeshName;
	FString PngFile;
	FString AssetObjectPath;
	FString Error;
	int32 Width = 0;
	int32 Height = 0;
	FDateTime Time;
};

class SIconForgeEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SIconForgeEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SIconForgeEditor() override;

	void SelectMesh(const FAssetData& Asset);
	/** Content Browser "Make Icon": first mesh goes on stage, all of them get selected for Batch. */
	void OpenAssets(const TArray<FAssetData>& Assets);

	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& E) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry& G, const double CurrentTime, const float DeltaTime) override;

private:
	// ---- UI ----
	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildMeshPanel();
	TSharedRef<SWidget> BuildAssetPicker();
	void RebuildAssetPicker();
	TSharedRef<SWidget> BuildViewportPanel();
	TSharedRef<SWidget> BuildCameraBar();
	TSharedRef<SWidget> BuildQuickSettings();
	TSharedRef<SWidget> BuildSettingsPanel();
	TSharedRef<SWidget> BuildResultPanel();
	TSharedRef<SWidget> BuildPresetMenu();
	TSharedRef<SWidget> BuildFXPresets();
	void ApplyFXPreset(EIconForgeFXPreset P);
	void RebuildHistoryStrip();

	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	FText GetBatchLabel() const;
	int32 GetSelectionCount() const;

	// ---- Actions ----
	bool HandleHotkey(const FKeyEvent& E);
	FReply OnShot();
	FReply OnBatchShot();
	FReply OnFrame();
	FReply OnOpenFolder();
	FReply OnShowInContentBrowser();
	FReply OnCopyPath();
	FReply OnClearHistory();
	FReply OnResetSettings();
	FReply OnDonate();
	void ApplyCameraPreset(EIconForgeCameraPreset P);
	void ApplyLightPreset(EIconForgeLightPreset P);
	void SetSquareSize(int32 Size);

	bool Shoot(const FIconForgeView& View, FString& OutMessage, TArray<UPackage*>* DeferredSave);
	bool SaveIcon(const TArray<FColor>& Px, FIconForgeShot& Shot, TArray<UPackage*>* DeferredSave);
	void AddToHistory(const TArray<FColor>& Px, TSharedRef<FIconForgeShot> Shot);
	FString MakeIconName(const FString& MeshName) const;

	/** Something changed (details panel, chips, presets). bReframe = framing depends on it. */
	void OnSettingsChanged(bool bReframe = false);
	void SaveSessionNow();
	EActiveTimerReturnType AutosaveTimer(double InCurrentTime, float InDeltaTime);
	void SavePreset(const FString& Name);
	void LoadPreset(const FString& File);
	void Notify(const FString& Msg, bool bSuccess);

	// ---- Live FX preview ----
	/** bRerender = scene/camera changed (new capture needed); false = only effects changed (re-use cached raw render). */
	void RequestLivePreview(bool bRerender);
	void UpdateLivePreview();
	void UploadLiveTexture(const TArray<FColor>& Px, int32 W, int32 H);
	const FSlateBrush* GetResultBrush() const;
	FVector2D GetResultSize() const;

	TUniquePtr<FIconForgeStudio> Studio;
	TStrongObjectPtr<UIconForgeSettings> Settings;
	TSharedPtr<SIconForgeViewport> Viewport;
	TSharedPtr<IDetailsView> Details;
	TSharedPtr<SBox> PickerHost;
	TSharedPtr<SBox> AspectBox;
	TSharedPtr<SWrapBox> HistoryBox;
	FGetCurrentSelectionDelegate GetSelectedAssets;
	FSyncToAssetsDelegate SyncToAssets;

	TArray<TSharedPtr<FIconForgeShot>> History;
	TSharedPtr<FIconForgeShot> SelectedShot;
	FVector2D LastViewportArea = FVector2D::ZeroVector;
	bool bSessionDirty = false;
	bool bBatchRunning = false;

	// Live preview state
	TArray<FColor> LiveRaw;                 // last raw capture (no effects), output resolution
	int32 LiveRawW = 0, LiveRawH = 0;
	TStrongObjectPtr<UTexture2D> LiveTexture;
	TSharedPtr<FSlateBrush> LiveBrush;
	TStrongObjectPtr<UTexture2D> LiveRawTexture;
	TSharedPtr<FSlateBrush> LiveRawBrush;
	FIntPoint LiveSize = FIntPoint::ZeroValue;
	bool bLiveNeedsRender = false;
	bool bLiveNeedsFX = false;
	double LiveRequestTime = 0.0;
	double LastLiveMs = 0.0;
	bool bShowLive = false;                 // RESULT shows the live preview instead of the selected shot
	bool bCompareRaw = false;               // A/B: show the image without effects
	FIconForgeOrbit LastSeenOrbit;
};
