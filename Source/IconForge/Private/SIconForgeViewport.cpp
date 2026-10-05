#include "SIconForgeViewport.h"
#include "IconForgeStudio.h"
#include "IconForgeSettings.h"
#include "Style/IconForgeStyle.h"
#include "Style/IconForgeWidgets.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "SceneView.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "IconForgeViewport"

// ======================================================= Client

FIconForgeViewportClient::FIconForgeViewportClient(FIconForgeStudio& InStudio, UIconForgeSettings* InSettings, const TSharedRef<SEditorViewport>& InWidget)
	: FEditorViewportClient(nullptr, &InStudio.GetScene(), InWidget)
	, Studio(InStudio), Settings(InSettings)
{
	SetRealtime(true);
	DrawHelper.bDrawGrid = false;
	DrawHelper.bDrawPivot = false;
	bDrawAxes = false;                       // no axis gizmo in the corner: the viewport IS the icon
	EngineShowFlags.SetGrid(false);
	EngineShowFlags.SetFog(false);
	EngineShowFlags.SetAtmosphere(false);
	EngineShowFlags.SetSelectionOutline(false);
	EngineShowFlags.SetEyeAdaptation(false);
	EngineShowFlags.SetMotionBlur(false);
	SetViewModes(VMI_Lit, VMI_Lit);
	// We drive the camera ourselves (free camera mode, our own orbit math)
	if (bUsingOrbitCamera) { ToggleOrbitCamera(false); }
	ViewFOV = InSettings ? InSettings->FieldOfView : 30.f;
	ApplyCamera();
}

float FIconForgeViewportClient::GetTanHalfVFov() const
{
	if (RenderedTanV > 0.f) { return RenderedTanV; }
	const UIconForgeSettings* S = Settings.Get();
	const float Fov = S ? S->FieldOfView : ViewFOV;
	const float Aspect = S ? S->GetAspect() : 1.f;
	return FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)) / Aspect;
}

float FIconForgeViewportClient::MinDistance() const
{
	return FMath::Max(Studio.GetBounds().SphereRadius * 0.05f, 0.5f);
}

FIconForgeView FIconForgeViewportClient::MakeView(const FIconForgeOrbit& O) const
{
	FIconForgeView V;
	V.Rotation = FRotator(O.Pitch, O.Yaw, 0.f);
	V.Location = O.Target - V.Rotation.Vector() * O.Distance;
	V.TanHalfVFov = GetTanHalfVFov();
	V.bValid = true;
	return V;
}

void FIconForgeViewportClient::ApplyCamera()
{
	const FIconForgeView V = MakeView(Current);
	SetViewLocation(V.Location);
	SetViewRotation(V.Rotation);
	SetLookAtLocation(Current.Target);
	Invalidate();
}

void FIconForgeViewportClient::SetOrbit(const FIconForgeOrbit& O, bool bInstant)
{
	Desired = O;
	Desired.Pitch = FMath::Clamp(Desired.Pitch, -89.f, 89.f);
	Desired.Distance = FMath::Max(Desired.Distance, MinDistance());
	if (bInstant) { Current = Desired; }
	ApplyCamera();
}

void FIconForgeViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Smooth camera: exponential approach to Desired (frame-rate independent)
	const UIconForgeSettings* S = Settings.Get();
	const float Smooth = S ? S->CameraSmoothing : 0.f;
	if (Smooth <= 0.f)
	{
		Current = Desired;
	}
	else
	{
		const float A = 1.f - FMath::Exp(-Smooth * DeltaSeconds);
		Current.Target   = FMath::Lerp(Current.Target, Desired.Target, A);
		Current.Pitch    = FMath::Lerp(Current.Pitch, Desired.Pitch, A);
		Current.Yaw     += FMath::FindDeltaAngleDegrees(Current.Yaw, Desired.Yaw) * A;
		Current.Distance = FMath::Exp(FMath::Lerp(FMath::Loge(FMath::Max(Current.Distance, 0.01f)), FMath::Loge(FMath::Max(Desired.Distance, 0.01f)), A)); // log space = even zoom
	}
	// Keep yaw bounded (endless orbiting used to grow it forever)
	if (FMath::Abs(Desired.Yaw) > 3600.f)
	{
		const float Wrapped = (float)FRotator::NormalizeAxis(Desired.Yaw);
		Current.Yaw += Wrapped - Desired.Yaw;
		Desired.Yaw = Wrapped;
	}
	ApplyCamera();

	if (UIconForgeSettings* MS = Settings.Get())
	{
		ViewFOV = MS->FieldOfView;
		Studio.ApplySettings(*MS, GetCameraRotation());
	}
	if (!GIntraFrameDebuggingGameThread && PreviewScene && PreviewScene->GetWorld())
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void FIconForgeViewportClient::Orbit(const FVector2D& D, bool bPrecise)
{
	const UIconForgeSettings* S = Settings.Get();
	const float K = 0.25f * (S ? S->OrbitSensitivity : 1.f) * (bPrecise && S ? S->PrecisionMultiplier : 1.f);
	const float InvY = (S && S->bInvertOrbitY) ? -1.f : 1.f;
	Desired.Yaw += (float)D.X * K;
	Desired.Pitch = FMath::Clamp(Desired.Pitch - (float)D.Y * K * InvY, -89.f, 89.f);
}

void FIconForgeViewportClient::Pan(const FVector2D& D, float HeightPx, bool bPrecise)
{
	const UIconForgeSettings* S = Settings.Get();
	// World units per pixel at target depth -> 1.0 sensitivity = object sticks to cursor
	const float WorldPerPx = 2.f * Desired.Distance * GetTanHalfVFov() / FMath::Max(HeightPx, 1.f);
	const float K = WorldPerPx * (S ? S->PanSensitivity : 0.5f) * (bPrecise && S ? S->PrecisionMultiplier : 1.f);
	const FRotationMatrix M(FRotator(Desired.Pitch, Desired.Yaw, 0.f));
	Desired.Target += (-M.GetScaledAxis(EAxis::Y) * D.X + M.GetScaledAxis(EAxis::Z) * D.Y) * K;
}

void FIconForgeViewportClient::Zoom(float Steps, bool bPrecise)
{
	const UIconForgeSettings* S = Settings.Get();
	const float K = 0.12f * (S ? S->ZoomSensitivity : 1.f) * (bPrecise && S ? S->PrecisionMultiplier : 1.f);
	const float MaxD = FMath::Max(Studio.GetBounds().SphereRadius * 60.f, 1000.f);
	Desired.Distance = FMath::Clamp(Desired.Distance * FMath::Pow(1.f + K, -Steps), MinDistance(), MaxD);
}

FIconForgeView FIconForgeViewportClient::FrameMesh(const FRotator* ForcedRotation, bool bInstant)
{
	const FBoxSphereBounds B = Studio.GetBounds();
	const UIconForgeSettings* S = Settings.Get();
	const float Pad = S ? S->FramingPadding : 1.05f;
	const float Aspect = S ? S->GetAspect() : 1.f;
	const float TanV = GetTanHalfVFov();
	const float TanMin = FMath::Min(TanV, TanV * Aspect);
	const float Radius = FMath::Max(B.SphereRadius, 1.f) * Pad;

	Desired.Target = B.Origin;
	Desired.Distance = Radius / FMath::Max(FMath::Sin(FMath::Atan(TanMin)), KINDA_SMALL_NUMBER);
	if (ForcedRotation)
	{
		Desired.Pitch = FMath::Clamp((float)FRotator::NormalizeAxis(ForcedRotation->Pitch), -89.f, 89.f);
		Desired.Yaw = (float)ForcedRotation->Yaw;
	}
	if (bInstant) { Current = Desired; ApplyCamera(); }
	return MakeView(Desired);
}

FIconForgeView FIconForgeViewportClient::SnapAndGetView()
{
	Current = Desired;
	ApplyCamera();
	return MakeView(Current);
}

FLinearColor FIconForgeViewportClient::GetBackgroundColor() const
{
	const UIconForgeSettings* S = Settings.Get();
	return (S && !S->bTransparentBackground) ? S->BackgroundColor : FIconForgeStyle::Hex(0x07080B);
}

void FIconForgeViewportClient::OverridePostProcessSettings(FSceneView& View)
{
	if (const UIconForgeSettings* S = Settings.Get())
	{
		FPostProcessSettings PP;
		Studio.FillPostProcess(*S, PP);
		View.OverridePostProcessSettings(PP, 1.f);
	}
}

FSceneView* FIconForgeViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoViewIndex)
{
	FSceneView* V = FEditorViewportClient::CalcSceneView(ViewFamily, StereoViewIndex);
	if (V)
	{
		const FMatrix P = V->ViewMatrices.GetViewToClip();
		if (P.M[1][1] > KINDA_SMALL_NUMBER) { RenderedTanV = 1.f / P.M[1][1]; }
	}
	return V;
}

void FIconForgeViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);
	const UIconForgeSettings* S = Settings.Get();
	if (!S || !S->bShowGuides || !Studio.GetMesh()) { return; }

	const float Dpi = FMath::Max(Canvas.GetDPIScale(), 0.01f);
	const FIntRect R = Canvas.GetViewRect();
	const float W = (float)R.Width() / Dpi, H = (float)R.Height() / Dpi;
	FLinearColor Thirds = FLinearColor::White; Thirds.A = 0.10f;
	FLinearColor Center = FIconForgeStyle::Accent(); Center.A = 0.8f;
	auto Line = [&](FVector2D A, FVector2D B, const FLinearColor& C) { FCanvasLineItem L(A, B); L.SetColor(C); Canvas.DrawItem(L); };
	for (int32 i = 1; i < 3; ++i)
	{
		Line(FVector2D(W * i / 3.f, 0), FVector2D(W * i / 3.f, H), Thirds);
		Line(FVector2D(0, H * i / 3.f), FVector2D(W, H * i / 3.f), Thirds);
	}
	const float C = 10.f;
	Line(FVector2D(W * 0.5f - C, H * 0.5f), FVector2D(W * 0.5f + C, H * 0.5f), Center);
	Line(FVector2D(W * 0.5f, H * 0.5f - C), FVector2D(W * 0.5f, H * 0.5f + C), Center);
}

// ======================================================= Widget

void SIconForgeViewport::Construct(const FArguments& InArgs)
{
	Studio = InArgs._Studio;
	Settings = InArgs._Settings;
	OnHotkey = InArgs._OnHotkey;
	SEditorViewport::Construct(SEditorViewport::FArguments());
	// All mouse input is handled by this widget (our own camera), the inner SViewport only renders.
	if (ViewportWidget) { ViewportWidget->SetVisibility(EVisibility::HitTestInvisible); }
}

TSharedRef<FEditorViewportClient> SIconForgeViewport::MakeEditorViewportClient()
{
	Client = MakeShared<FIconForgeViewportClient>(*Studio, Settings, SharedThis(this));
	return Client.ToSharedRef();
}

void SIconForgeViewport::OnFocusViewportToSelection()
{
	if (Client) { Client->FrameMesh(); }
}

bool SIconForgeViewport::HasMesh() const
{
	return Studio && Studio->GetMesh() != nullptr;
}

FReply SIconForgeViewport::OnKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	// Our hotkeys first: SEditorViewport binds Space (cycle gizmo), W/E/R (transform modes)... which we don't use.
	if (OnHotkey.IsBound() && OnHotkey.Execute(E)) { return FReply::Handled(); }
	return SEditorViewport::OnKeyDown(G, E);
}

FReply SIconForgeViewport::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
	const FKey B = E.GetEffectingButton();
	if (B == EKeys::LeftMouseButton || B == EKeys::RightMouseButton || B == EKeys::MiddleMouseButton)
	{
		bDragging = true;
		return FReply::Handled()
			.PreventThrottling()                               // keep the editor rendering while dragging
			.CaptureMouse(SharedThis(this))
			.UseHighPrecisionMouseMovement(SharedThis(this))   // hides cursor, raw deltas, no screen-edge stop
			.SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}
	return FReply::Unhandled();
}

FReply SIconForgeViewport::OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E)
{
	if (!bDragging) { return FReply::Unhandled(); }
	// Release only when no other camera button is still held (works whether or not the released
	// button is still reported in the pressed set)
	const FKey Released = E.GetEffectingButton();
	auto StillDown = [&E, &Released](const FKey& K) { return K != Released && E.IsMouseButtonDown(K); };
	if (!StillDown(EKeys::LeftMouseButton) && !StillDown(EKeys::RightMouseButton) && !StillDown(EKeys::MiddleMouseButton))
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

FReply SIconForgeViewport::OnMouseButtonDoubleClick(const FGeometry& G, const FPointerEvent& E)
{
	if (E.GetEffectingButton() == EKeys::LeftMouseButton && Client)
	{
		Client->FrameMesh();
		return FReply::Handled();
	}
	return OnMouseButtonDown(G, E);
}

void SIconForgeViewport::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// Alt+Tab / window lost focus while dragging: never stay stuck in "dragging" state
	bDragging = false;
	SEditorViewport::OnMouseCaptureLost(CaptureLostEvent);
}

FReply SIconForgeViewport::OnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
	if (!bDragging || !HasMouseCapture() || !Client) { return FReply::Unhandled(); }

	const FVector2D D = E.GetCursorDelta();
	if (D.IsNearlyZero()) { return FReply::Handled().PreventThrottling(); }
	const bool bPrecise = E.IsControlDown();
	const bool L = E.IsMouseButtonDown(EKeys::LeftMouseButton);
	const bool M = E.IsMouseButtonDown(EKeys::MiddleMouseButton);
	const bool R = E.IsMouseButtonDown(EKeys::RightMouseButton);
	const float HeightPx = (float)(G.GetLocalSize().Y * G.GetAccumulatedLayoutTransform().GetScale());

	if (M || (L && E.IsShiftDown()) || (L && R)) { Client->Pan(D, HeightPx, bPrecise); }
	else if (R) { Client->Zoom(-(float)(D.Y + D.X) * 0.05f, bPrecise); }
	else if (L) { Client->Orbit(D, bPrecise); }
	Client->Invalidate();
	return FReply::Handled().PreventThrottling();
}

FReply SIconForgeViewport::OnMouseWheel(const FGeometry& G, const FPointerEvent& E)
{
	if (Client) { Client->Zoom(E.GetWheelDelta(), E.IsControlDown()); Client->Invalidate(); }
	return FReply::Handled().PreventThrottling();
}

FCursorReply SIconForgeViewport::OnCursorQuery(const FGeometry& G, const FPointerEvent& E) const
{
	return FCursorReply::Cursor(HasMesh() ? EMouseCursor::GrabHand : EMouseCursor::Default);
}

FText SIconForgeViewport::GetInfoText() const
{
	UStaticMesh* M = Studio ? Studio->GetMesh() : nullptr;
	if (!M) { return FText::GetEmpty(); }
	const FVector Size = Studio->GetBounds().BoxExtent * 2.f;
	return FText::FromString(FString::Printf(TEXT("%s  \u00B7  %s tris  \u00B7  %.0f \u00D7 %.0f \u00D7 %.0f cm"),
		*M->GetName(), *FText::AsNumber(M->GetNumTriangles(0)).ToString(), Size.X, Size.Y, Size.Z));
}

FText SIconForgeViewport::GetSizeText() const
{
	if (!Settings) { return FText::GetEmpty(); }
	const int32 SS = Settings->GetEffectiveSuperSampling();
	return FText::FromString(FString::Printf(TEXT("%d \u00D7 %d  \u00B7  SS %d\u00D7  \u00B7  FOV %.0f\u00B0"),
		Settings->Width, Settings->Height, SS, Settings->FieldOfView));
}

void SIconForgeViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	using namespace IconForgeUI;
	SEditorViewport::PopulateViewportOverlays(Overlay);

	// Top-left: what is on stage
	Overlay->AddSlot().VAlign(VAlign_Top).HAlign(HAlign_Left).Padding(10.f)
	[
		SNew(SBorder)
		.BorderImage(FIconForgeStyle::Brush("IconForge.Raised"))
		.Padding(FMargin(12.f, 8.f))
		.Visibility_Lambda([this]() { return HasMesh() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				Caption(LOCTEXT("StageTitle", "WHAT YOU SEE IS THE ICON"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
			[
				SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Muted").Text(this, &SIconForgeViewport::GetInfoText)
			]
		]
	];

	// Top-right: output size
	Overlay->AddSlot().VAlign(VAlign_Top).HAlign(HAlign_Right).Padding(10.f)
	[
		SNew(SBorder)
		.BorderImage(FIconForgeStyle::Brush("IconForge.Raised"))
		.Padding(FMargin(10.f, 5.f))
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Mono").Text(this, &SIconForgeViewport::GetSizeText)
		]
	];

	// Bottom: shortcuts
	Overlay->AddSlot().VAlign(VAlign_Bottom).HAlign(HAlign_Center).Padding(10.f)
	[
		SNew(SBorder)
		.BorderImage(FIconForgeStyle::Brush("IconForge.Raised"))
		.Padding(FMargin(12.f, 5.f))
		.Visibility_Lambda([this]() { return HasMesh() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Caption").AutoWrapText(true).Justification(ETextJustify::Center)
			.Text(LOCTEXT("Shortcuts", "LMB orbit   MMB / Shift+LMB pan   RMB / Wheel zoom   Ctrl precise   F / double-click frame   R reset   1-7 angles   G guides   SPACE shot"))
		]
	];

	// Centre: empty state
	Overlay->AddSlot().VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(20.f)
	[
		SNew(SBorder)
		.BorderImage(FIconForgeStyle::Brush("IconForge.Raised"))
		.Padding(FMargin(22.f, 18.f))
		.Visibility_Lambda([this]() { return HasMesh() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		[
			SNew(SBox).MaxDesiredWidth(340.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(36.f).HeightOverride(36.f)
					[
						SNew(SBorder).BorderImage(FIconForgeStyle::Brush("IconForge.Logo")).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(FIconForgeStyle::Font("Bold", 13)).ColorAndOpacity(FIconForgeStyle::Background()).Text(LOCTEXT("EmptyLogo", "IF"))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f, 0.f, 4.f)
				[
					SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Title").Text(LOCTEXT("EmptyTitle", "Pick a Static Mesh"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Muted").AutoWrapText(true).Justification(ETextJustify::Center)
					.Text(LOCTEXT("EmptyHint", "Click a mesh in the MESHES list, or right-click any Static Mesh in the Content Browser and choose \"Icon Forge: Make Icon\"."))
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
