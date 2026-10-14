#include "SIconForgeEditor.h"
#include "SIconForgeViewport.h"
#include "IconForgeEffects.h"
#include "HAL/PlatformTime.h"
#include "Style/IconForgeStyle.h"
#include "Style/IconForgeWidgets.h"
#include "ContentBrowserModule.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopedSlowTask.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformApplicationMisc.h"
#include "FileHelpers.h"
#include "Internationalization/Regex.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "IconForge"

using namespace IconForgeUI;

namespace
{
	const TCHAR* DonateUrl = TEXT("https://dalink.to/coreveldev");

	FRotator PresetRotation(EIconForgeCameraPreset P)
	{
		switch (P)
		{
		case EIconForgeCameraPreset::Front:        return FRotator(0, 180, 0);
		case EIconForgeCameraPreset::Back:         return FRotator(0, 0, 0);
		case EIconForgeCameraPreset::Left:         return FRotator(0, -90, 0);
		case EIconForgeCameraPreset::Right:        return FRotator(0, 90, 0);
		case EIconForgeCameraPreset::Top:          return FRotator(-89.f, 180, 0);
		case EIconForgeCameraPreset::ThreeQuarter: return FRotator(-20, -135, 0);
		default:                                   return FRotator(-35.264f, -135, 0);   // true isometric
		}
	}

	/** Typing in a search box / numeric field must never trigger hotkeys (e.g. "1" or Space). */
	bool IsTextInputFocused()
	{
		if (!FSlateApplication::IsInitialized()) { return false; }
		TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
		if (!Focused.IsValid()) { return false; }
		const FString Type = Focused->GetTypeAsString();
		return Type.Contains(TEXT("EditableText")) || Type.Contains(TEXT("SearchBox")) || Type.Contains(TEXT("TextBox"));
	}

	TSharedRef<SWidget> Divider()
	{
		return SNew(SBox).HeightOverride(1.f)[ SNew(SImage).Image(FIconForgeStyle::Brush("IconForge.Divider")) ];
	}

	TSharedRef<SWidget> VDivider()
	{
		return SNew(SBox).WidthOverride(1.f).HeightOverride(18.f).Padding(0.f)[ SNew(SImage).Image(FIconForgeStyle::Brush("IconForge.Divider")) ];
	}

	FString ShotPathText(const FIconForgeShot& S)
	{
		TArray<FString> Lines;
		if (!S.PngFile.IsEmpty()) { Lines.Add(S.PngFile); }
		if (!S.AssetObjectPath.IsEmpty()) { Lines.Add(S.AssetObjectPath); }
		if (!S.Error.IsEmpty()) { Lines.Add(S.Error); }
		return FString::Join(Lines, TEXT("\n"));
	}
}

// ============================================================ Construct

void SIconForgeEditor::Construct(const FArguments&)
{
	Settings.Reset(NewObject<UIconForgeSettings>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional));
	Settings->LoadFromFile(UIconForgeSettings::GetSessionFile());
	Studio = MakeUnique<FIconForgeStudio>();

	FPropertyEditorModule& PE = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DA;
	DA.bHideSelectionTip = true;
	DA.bAllowSearch = true;
	DA.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Details = PE.CreateDetailView(DA);
	Details->SetObject(Settings.Get());
	Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& E)
	{
		// Member name: editing MeshRotation.Yaw reports "Yaw" as property but "MeshRotation" as member
		FName N = E.GetMemberPropertyName();
		if (N.IsNone()) { N = E.GetPropertyName(); }
		const bool bReframe = N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, FieldOfView) ||
			N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, FramingPadding) ||
			N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, MeshRotation) ||
			N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, Width) ||
			N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, Height);
		const bool bFXOnly = N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, ColorFX) ||
			N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, RetroFX) ||
			N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, StylizeFX) ||
			N == GET_MEMBER_NAME_CHECKED(UIconForgeSettings, CRTFX);
		if (bFXOnly)
		{
			// Hand-tuned effects: no preset chip is highlighted any more. Effects never need a new capture.
			Settings->ActiveFXPreset = EIconForgeFXPreset::Custom;
			bSessionDirty = true;
			RequestLivePreview(false);
			return;
		}
		OnSettingsChanged(bReframe);
	});

	SAssignNew(Viewport, SIconForgeViewport)
		.Studio(Studio.Get())
		.Settings(Settings.Get())
		.OnHotkey(FIconForgeHotkey::CreateSP(this, &SIconForgeEditor::HandleHotkey));

	// Restore last orbit angle
	if (TSharedPtr<FIconForgeViewportClient> C = Viewport->GetClient())
	{
		FIconForgeOrbit O = C->GetOrbit();
		O.Pitch = (float)Settings->LastCameraRotation.Pitch;
		O.Yaw = (float)Settings->LastCameraRotation.Yaw;
		C->SetOrbit(O, true);
		LastSeenOrbit = C->GetOrbit();
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FIconForgeStyle::Brush("IconForge.Background"))
		.Padding(14.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
			[
				BuildHeader()
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SSplitter).Orientation(Orient_Horizontal).PhysicalSplitterHandleSize(8.f)
				+ SSplitter::Slot().Value(0.2f)[ BuildMeshPanel() ]
				+ SSplitter::Slot().Value(0.53f)[ BuildViewportPanel() ]
				+ SSplitter::Slot().Value(0.27f)
				[
					SNew(SSplitter).Orientation(Orient_Vertical).PhysicalSplitterHandleSize(8.f)
					+ SSplitter::Slot().Value(0.56f)[ BuildSettingsPanel() ]
					+ SSplitter::Slot().Value(0.44f)[ BuildResultPanel() ]
				]
			]
		]
	];

	RegisterActiveTimer(1.f, FWidgetActiveTimerDelegate::CreateSP(this, &SIconForgeEditor::AutosaveTimer));
}

SIconForgeEditor::~SIconForgeEditor()
{
	SaveSessionNow();
	Viewport.Reset();
}

void SIconForgeEditor::Tick(const FGeometry& G, const double T, const float Dt)
{
	SCompoundWidget::Tick(G, T, Dt);
	UpdateLivePreview();
	if (!AspectBox || !Settings) { return; }
	// Biggest box with the icon aspect ratio inside the stage (parent border, padding 6)
	TSharedPtr<SWidget> Parent = AspectBox->GetParentWidget();
	if (!Parent) { return; }
	const FVector2D Avail = Parent->GetTickSpaceGeometry().GetLocalSize() - FVector2D(12.f, 12.f);
	if (Avail.X <= 10.f || Avail.Y <= 10.f) { return; }
	const float Aspect = Settings->GetAspect();
	FVector2D Size(Avail.X, Avail.X / Aspect);
	if (Size.Y > Avail.Y) { Size = FVector2D(Avail.Y * Aspect, Avail.Y); }
	if (!Size.Equals(LastViewportArea, 0.5f))
	{
		LastViewportArea = Size;
		AspectBox->SetWidthOverride(FOptionalSize((float)FMath::FloorToDouble(Size.X)));
		AspectBox->SetHeightOverride(FOptionalSize((float)FMath::FloorToDouble(Size.Y)));
	}
}

// ============================================================ Header

TSharedRef<SWidget> SIconForgeEditor::BuildHeader()
{
	return SNew(SHorizontalBox)
		// Logo
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
		[
			SNew(SBox).WidthOverride(34.f).HeightOverride(34.f)
			[
				SNew(SBorder).BorderImage(FIconForgeStyle::Brush("IconForge.Logo")).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(FIconForgeStyle::Font("Bold", 13)).ColorAndOpacity(FIconForgeStyle::Background()).Text(LOCTEXT("Logo", "IF"))
				]
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Display").Text(LOCTEXT("Title", "Icon Forge"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Muted").Text(LOCTEXT("Subtitle", "Studio-quality item icons from Static Meshes"))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
		// Status
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
		[
			SNew(SBorder).BorderImage(FIconForgeStyle::Brush("IconForge.Raised")).Padding(FMargin(10.f, 6.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
				[
					Dot(TAttribute<FSlateColor>::CreateSP(this, &SIconForgeEditor::GetStatusColor), 8.f)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Mono").Text(this, &SIconForgeEditor::GetStatusText)
				]
			]
		]
		// Donate (DonationAlerts)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(SButton)
			.ButtonStyle(&St(), "IconForge.Button")
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("DonateTip", "Support Icon Forge on DonationAlerts (opens dalink.to/coreveldev in your browser)"))
			.OnClicked(this, &SIconForgeEditor::OnDonate)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SImage).Image(FIconForgeStyle::Brush("IconForge.Donate"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Button").Text(LOCTEXT("Donate", "Donate"))
				]
			]
		]
		// Presets
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(SComboButton)
			.ButtonStyle(&St(), "IconForge.Button")
			.IsFocusable(false)
			.HasDownArrow(true)
			.ForegroundColor(FIconForgeStyle::Text())
			.ToolTipText(LOCTEXT("PresetsTip", "Save / load all settings as a named preset"))
			.OnGetMenuContent(this, &SIconForgeEditor::BuildPresetMenu)
			.ButtonContent()
			[
				SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Button").Text(LOCTEXT("Presets", "Presets"))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
		[
			Button(LOCTEXT("Reset", "Reset"), FOnClicked::CreateSP(this, &SIconForgeEditor::OnResetSettings),
				LOCTEXT("ResetTip", "Reset all settings to factory defaults"), "IconForge.Button", "Icons.Refresh")
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
		[
			SNew(SBox).IsEnabled_Lambda([this]() { return GetSelectionCount() > 0; })
			[
				Button(TAttribute<FText>::CreateSP(this, &SIconForgeEditor::GetBatchLabel), FOnClicked::CreateSP(this, &SIconForgeEditor::OnBatchShot),
					LOCTEXT("BatchTip", "Render every mesh selected in the MESHES list (Ctrl/Shift + click) with the current angle, lights and settings"),
					"IconForge.Button.Accent")
			]
		]
		// Shot!
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(&St(), "IconForge.Button.Primary")
			.IsFocusable(false)
			.IsEnabled_Lambda([this]() { return Studio && Studio->GetMesh() != nullptr; })
			.ToolTipText(LOCTEXT("ShotTip", "Render the icon exactly as framed in the stage (Space / Enter)"))
			.OnClicked(this, &SIconForgeEditor::OnShot)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Shot").Text(LOCTEXT("Shot", "Shot!"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock).Font(FIconForgeStyle::Font("Bold", 8)).ColorAndOpacity(FIconForgeStyle::Hex(0x0B0D11, 0.6f)).Text(LOCTEXT("ShotKey", "SPACE"))
				]
			]
		];
}

FText SIconForgeEditor::GetStatusText() const
{
	if (!Settings) { return FText::GetEmpty(); }
	if (!Settings->bSavePNG && !Settings->bCreateTextureAsset) { return LOCTEXT("NoOutput", "NO OUTPUT ENABLED"); }
	const UStaticMesh* M = Studio ? Studio->GetMesh() : nullptr;
	FString Out;
	if (Settings->bSavePNG) { Out = TEXT("PNG"); }
	if (Settings->bCreateTextureAsset) { Out += Out.IsEmpty() ? TEXT("TEXTURE") : TEXT(" + TEXTURE"); }
	return FText::FromString(FString::Printf(TEXT("%s  \u00B7  %d\u00D7%d  \u00B7  %s"),
		M ? *M->GetName() : TEXT("NO MESH"), Settings->Width, Settings->Height, *Out));
}

FSlateColor SIconForgeEditor::GetStatusColor() const
{
	if (Settings && !Settings->bSavePNG && !Settings->bCreateTextureAsset) { return FIconForgeStyle::Bad(); }
	return (Studio && Studio->GetMesh()) ? FIconForgeStyle::Live() : FIconForgeStyle::TextDim();
}

int32 SIconForgeEditor::GetSelectionCount() const
{
	return GetSelectedAssets.IsBound() ? GetSelectedAssets.Execute().Num() : 0;
}

FText SIconForgeEditor::GetBatchLabel() const
{
	const int32 N = GetSelectionCount();
	return N > 0 ? FText::Format(LOCTEXT("BatchN", "Batch  {0}"), FText::AsNumber(N)) : LOCTEXT("Batch", "Batch");
}

// ============================================================ Left: meshes

TSharedRef<SWidget> SIconForgeEditor::BuildMeshPanel()
{
	auto Toggle = [this](const FText& Label, const FText& Tip, bool UIconForgeSettings::* Member)
	{
		return Chip(Label,
			[this, Member]() { return Settings.IsValid() && (Settings.Get()->*Member); },
			[this, Member](bool bOn) { Settings.Get()->*Member = bOn; OnSettingsChanged(); RebuildAssetPicker(); },
			Tip);
	};

	TSharedRef<SWidget> Extra = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
		[
			Toggle(LOCTEXT("Proj", "Project"), LOCTEXT("ProjTip", "Show only /Game content (hide Engine & plugin meshes)"), &UIconForgeSettings::bProjectContentOnly)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			Toggle(LOCTEXT("Lod", "No LODs"), LOCTEXT("LodTip", "Hide meshes named *_LOD1, *_LOD2..."), &UIconForgeSettings::bHideLODMeshes)
		];

	return Panel(LOCTEXT("Meshes", "MESHES"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 0.f, 14.f, 8.f)
		[
			SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Muted").AutoWrapText(true)
			.Text(LOCTEXT("MeshesHint", "Click = put on stage.  Double-click = instant shot.  Ctrl/Shift + click = select several for Batch."))
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(6.f, 0.f, 6.f, 6.f)
		[
			SAssignNew(PickerHost, SBox)[ BuildAssetPicker() ]
		],
		Extra);
}

TSharedRef<SWidget> SIconForgeEditor::BuildAssetPicker()
{
	FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FAssetPickerConfig Cfg;
	Cfg.Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	Cfg.Filter.bRecursiveClasses = true;
	if (Settings->bProjectContentOnly)
	{
		// Filter in the asset registry query itself: much faster than rejecting thousands of engine meshes one by one
		Cfg.Filter.PackagePaths.Add(FName(TEXT("/Game")));
		Cfg.Filter.bRecursivePaths = true;
	}
	Cfg.InitialAssetViewType = EAssetViewType::List;   // compact rows, name readable, still with small thumbnail
	Cfg.SelectionMode = ESelectionMode::Multi;
	Cfg.bAllowNullSelection = false;
	Cfg.bFocusSearchBoxWhenOpened = true;
	Cfg.bShowPathInColumnView = true;
	Cfg.bAllowDragging = false;
	Cfg.SaveSettingsName = TEXT("IconForgeAssetPicker");
	Cfg.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SIconForgeEditor::SelectMesh);
	Cfg.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateLambda([this](const FAssetData& A) { SelectMesh(A); OnShot(); });
	Cfg.GetCurrentSelectionDelegates.Add(&GetSelectedAssets);
	Cfg.SyncToAssetsDelegates.Add(&SyncToAssets);

	if (Settings->bHideLODMeshes)
	{
		Cfg.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda([](const FAssetData& A)
		{
			static const FRegexPattern LodPattern(TEXT("_LOD[1-9][0-9]*$"), ERegexPatternFlags::CaseInsensitive);
			FRegexMatcher M(LodPattern, A.AssetName.ToString());
			return M.FindNext();
		});
	}
	return CB.Get().CreateAssetPicker(Cfg);
}

void SIconForgeEditor::RebuildAssetPicker()
{
	if (!PickerHost) { return; }
	// Destroy the old picker BEFORE creating the new one, so it cannot unbind the new picker's delegates
	PickerHost->SetContent(SNullWidget::NullWidget);
	GetSelectedAssets.Unbind();
	SyncToAssets.Unbind();
	PickerHost->SetContent(BuildAssetPicker());
}

// ============================================================ Centre: stage

TSharedRef<SWidget> SIconForgeEditor::BuildViewportPanel()
{
	TSharedRef<SWidget> Guides = Chip(LOCTEXT("Guides", "Guides"),
		[this]() { return Settings.IsValid() && Settings->bShowGuides; },
		[this](bool bOn) { Settings->bShowGuides = bOn; OnSettingsChanged(); },
		LOCTEXT("GuidesTip", "Rule-of-thirds grid and centre cross (G). Never rendered into the icon."));

	return Panel(LOCTEXT("Stage", "STAGE"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 0.f, 14.f, 10.f)
		[
			BuildCameraBar()
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8.f, 0.f, 8.f, 8.f)
		[
			SNew(SBorder).BorderImage(FIconForgeStyle::Brush("IconForge.Stage")).Padding(6.f)
			.HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				// Size is computed every frame in Tick(): biggest box with the icon aspect ratio
				SAssignNew(AspectBox, SBox).WidthOverride(512.f).HeightOverride(512.f)
				[
					Viewport.ToSharedRef()
				]
			]
		],
		Guides);
}

TSharedRef<SWidget> SIconForgeEditor::BuildCameraBar()
{
	auto Cam = [this](const FText& L, EIconForgeCameraPreset P, int32 Key)
	{
		return Button(FText::Format(LOCTEXT("CamLabel", "{0}  {1}"), FText::AsNumber(Key), L),
			FOnClicked::CreateLambda([this, P]() { ApplyCameraPreset(P); return FReply::Handled(); }),
			FText::Format(LOCTEXT("CamTip", "Camera: {0} (key {1})"), L, FText::AsNumber(Key)), "IconForge.Button.Small");
	};
	auto Light = [this](const FText& L, EIconForgeLightPreset P)
	{
		return Button(L, FOnClicked::CreateLambda([this, P]() { ApplyLightPreset(P); return FReply::Handled(); }),
			FText::Format(LOCTEXT("LightTip", "Lighting preset: {0}. Fine-tune in SETTINGS > Lighting."), L), "IconForge.Button.Small");
	};

	TSharedRef<SWrapBox> CamBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	CamBox->AddSlot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[ SNew(SBox).WidthOverride(48.f)[ Caption(LOCTEXT("CamCap", "CAMERA")) ] ];
	CamBox->AddSlot()[ Cam(LOCTEXT("Front", "Front"), EIconForgeCameraPreset::Front, 1) ];
	CamBox->AddSlot()[ Cam(LOCTEXT("Back", "Back"), EIconForgeCameraPreset::Back, 2) ];
	CamBox->AddSlot()[ Cam(LOCTEXT("Left", "Left"), EIconForgeCameraPreset::Left, 3) ];
	CamBox->AddSlot()[ Cam(LOCTEXT("Right", "Right"), EIconForgeCameraPreset::Right, 4) ];
	CamBox->AddSlot()[ Cam(LOCTEXT("Top", "Top"), EIconForgeCameraPreset::Top, 5) ];
	CamBox->AddSlot()[ Cam(LOCTEXT("Q", "3/4"), EIconForgeCameraPreset::ThreeQuarter, 6) ];
	CamBox->AddSlot()[ Cam(LOCTEXT("Iso", "Iso"), EIconForgeCameraPreset::Isometric, 7) ];
	CamBox->AddSlot().VAlign(VAlign_Center).Padding(4.f, 0.f)[ VDivider() ];
	CamBox->AddSlot()[ Button(LOCTEXT("Frame", "Frame  F"), FOnClicked::CreateSP(this, &SIconForgeEditor::OnFrame),
		LOCTEXT("FrameTip", "Fit the object into the icon, keep the angle (F or double-click the stage)"), "IconForge.Button.Small", "Icons.Fullscreen") ];
	CamBox->AddSlot()[ Button(LOCTEXT("ResetCam", "Reset  R"), FOnClicked::CreateLambda([this]() { ApplyCameraPreset(EIconForgeCameraPreset::ThreeQuarter); return FReply::Handled(); }),
		LOCTEXT("ResetCamTip", "Back to the 3/4 view and frame (R)"), "IconForge.Button.Small") ];

	TSharedRef<SWrapBox> LightBox = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	LightBox->AddSlot().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[ SNew(SBox).WidthOverride(48.f)[ Caption(LOCTEXT("LightCap", "LIGHT")) ] ];
	LightBox->AddSlot()[ Light(LOCTEXT("LStudio", "Studio"), EIconForgeLightPreset::Studio) ];
	LightBox->AddSlot()[ Light(LOCTEXT("LSoft", "Soft"), EIconForgeLightPreset::Soft) ];
	LightBox->AddSlot()[ Light(LOCTEXT("LDrama", "Dramatic"), EIconForgeLightPreset::Dramatic) ];
	LightBox->AddSlot()[ Light(LOCTEXT("LCool", "Cool rim"), EIconForgeLightPreset::CoolRim) ];
	LightBox->AddSlot()[ Light(LOCTEXT("LWarm", "Warm"), EIconForgeLightPreset::Warm) ];
	LightBox->AddSlot()[ Light(LOCTEXT("LFlat", "Flat (UI)"), EIconForgeLightPreset::Flat) ];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ CamBox ]
		+ SVerticalBox::Slot().AutoHeight()[ LightBox ];
}

// ============================================================ Right: settings

TSharedRef<SWidget> SIconForgeEditor::BuildQuickSettings()
{
	TSharedRef<SWrapBox> Sizes = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	for (const int32 Size : { 64, 128, 256, 512, 1024, 2048 })
	{
		Sizes->AddSlot()
		[
			Chip(FText::AsNumber(Size, &FNumberFormattingOptions::DefaultNoGrouping()),
				[this, Size]() { return Settings->Width == Size && Settings->Height == Size; },
				[this, Size](bool) { SetSquareSize(Size); },
				FText::Format(LOCTEXT("SizeTip", "{0} \u00D7 {0} px. Any custom size: SETTINGS > Output > Width / Height."), FText::AsNumber(Size, &FNumberFormattingOptions::DefaultNoGrouping())))
		];
	}

	TSharedRef<SWrapBox> Quality = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	for (const int32 SS : { 1, 2, 3, 4 })
	{
		Quality->AddSlot()
		[
			Chip(FText::FromString(FString::Printf(TEXT("%d\u00D7"), SS)),
				[this, SS]() { return Settings->SuperSampling == SS; },
				[this, SS](bool) { Settings->SuperSampling = SS; OnSettingsChanged(); },
				LOCTEXT("SSTip", "Supersampling: render N times bigger and downscale for smooth edges. 1\u00D7 = fastest."))
		];
	}

	TSharedRef<SWrapBox> Background = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	Background->AddSlot()
	[
		Chip(LOCTEXT("Transparent", "Transparent"),
			[this]() { return Settings->bTransparentBackground; },
			[this](bool) { Settings->bTransparentBackground = true; OnSettingsChanged(); },
			LOCTEXT("TransparentTip", "PNG / texture with alpha channel"))
	];
	Background->AddSlot()
	[
		Chip(LOCTEXT("Solid", "Solid colour"),
			[this]() { return !Settings->bTransparentBackground; },
			[this](bool) { Settings->bTransparentBackground = false; OnSettingsChanged(); },
			LOCTEXT("SolidTip", "Opaque background, colour in SETTINGS > Output > Background Color"))
	];

	TSharedRef<SWrapBox> Output = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	Output->AddSlot()
	[
		Chip(LOCTEXT("Png", "PNG file"),
			[this]() { return Settings->bSavePNG; },
			[this](bool bOn) { Settings->bSavePNG = bOn; OnSettingsChanged(); },
			LOCTEXT("PngTip", "Write a .png (default folder: <Project>/Saved/Icons)"))
	];
	Output->AddSlot()
	[
		Chip(LOCTEXT("Asset", "Texture asset"),
			[this]() { return Settings->bCreateTextureAsset; },
			[this](bool bOn) { Settings->bCreateTextureAsset = bOn; OnSettingsChanged(); },
			LOCTEXT("AssetTip", "Create / update a UI Texture2D asset (default: /Game/Icons)"))
	];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[ Row(LOCTEXT("SizeCap", "SIZE"), Sizes) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[ Row(LOCTEXT("QualityCap", "QUALITY"), Quality) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[ Row(LOCTEXT("BgCap", "BACKGROUND"), Background) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[ Row(LOCTEXT("OutCap", "OUTPUT"), Output) ]
		+ SVerticalBox::Slot().AutoHeight()[ Row(LOCTEXT("FxCap", "LOOK / FX"), BuildFXPresets()) ];
}

TSharedRef<SWidget> SIconForgeEditor::BuildFXPresets()
{
	struct FItem { EIconForgeFXPreset P; FText Label; FText Tip; };
	const TArray<FItem> Items = {
		{ EIconForgeFXPreset::None,       LOCTEXT("FxNone", "Clean"),      LOCTEXT("FxNoneTip", "No effects: the render as is") },
		{ EIconForgeFXPreset::GameBoy,    LOCTEXT("FxGB", "Game Boy"),     LOCTEXT("FxGBTip", "32 px sprite, 4 shades of green, ordered dither, dark outline") },
		{ EIconForgeFXPreset::NES,        LOCTEXT("FxNES", "NES"),         LOCTEXT("FxNESTip", "32 px sprite, NES hardware palette, black outline") },
		{ EIconForgeFXPreset::PICO8,      LOCTEXT("FxP8", "PICO-8"),       LOCTEXT("FxP8Tip", "32 px sprite, PICO-8 16-colour palette, light dither") },
		{ EIconForgeFXPreset::SNES,       LOCTEXT("FxSNES", "16-bit"),     LOCTEXT("FxSNESTip", "64 px sprite, 15 hand-picked colours (auto palette), outline + inner edge shading") },
		{ EIconForgeFXPreset::PS1,        LOCTEXT("FxPS1", "PS1"),         LOCTEXT("FxPS1Tip", "128 px, 15-bit colour with the famous 4x4 PlayStation dither") },
		{ EIconForgeFXPreset::CGA,        LOCTEXT("FxCGA", "CGA"),         LOCTEXT("FxCGATip", "Cyan / magenta / white, 1981 PC style") },
		{ EIconForgeFXPreset::OneBitMac,  LOCTEXT("Fx1Bit", "1-bit Mac"),  LOCTEXT("Fx1BitTip", "Black & white with Atkinson dithering (classic Macintosh)") },
		{ EIconForgeFXPreset::VirtualBoy, LOCTEXT("FxVB", "Virtual Boy"),  LOCTEXT("FxVBTip", "Red on black with scanlines") },
		{ EIconForgeFXPreset::Arcade,     LOCTEXT("FxArc", "CRT Arcade"),  LOCTEXT("FxArcTip", "Curved tube, scanlines, phosphor mask, chromatic aberration, neon glow") },
		{ EIconForgeFXPreset::Toon,       LOCTEXT("FxToon", "Toon"),       LOCTEXT("FxToonTip", "Cel-shaded colours with a thick ink outline") },
		{ EIconForgeFXPreset::CleanUI,    LOCTEXT("FxUI", "Clean UI"),     LOCTEXT("FxUITip", "Crisp outline + soft drop shadow for modern inventories") },
		{ EIconForgeFXPreset::Legendary,  LOCTEXT("FxLeg", "Legendary"),   LOCTEXT("FxLegTip", "Golden glow, radial slot background and rounded rarity frame") },
	};
	TSharedRef<SWrapBox> Box = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f));
	for (const FItem& I : Items)
	{
		const EIconForgeFXPreset P = I.P;
		Box->AddSlot()
		[
			Chip(I.Label,
				[this, P]() { return Settings->ActiveFXPreset == P; },
				[this, P](bool) { ApplyFXPreset(P); },
				FText::Format(LOCTEXT("FxTip", "{0}\nResets all effects first. Fine-tune in SETTINGS > Color / Retro / Stylize / CRT."), I.Tip))
		];
	}
	Box->AddSlot()
	[
		SNew(SBox).Visibility_Lambda([this]() { return Settings->ActiveFXPreset == EIconForgeFXPreset::Custom ? EVisibility::Visible : EVisibility::Collapsed; })
		[ Pill(LOCTEXT("FxCustom", "CUSTOM"), FIconForgeStyle::Live()) ]
	];
	return Box;
}

void SIconForgeEditor::ApplyFXPreset(EIconForgeFXPreset P)
{
	Settings->ApplyFXPreset(P);
	Details->ForceRefresh();
	bSessionDirty = true;
	bShowLive = true;
	if (!Settings->bLiveFXPreview) { Settings->bLiveFXPreview = true; }
	RequestLivePreview(false);
}

TSharedRef<SWidget> SIconForgeEditor::BuildSettingsPanel()
{
	return Panel(LOCTEXT("SettingsCap", "SETTINGS"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 0.f, 14.f, 12.f)
		[
			BuildQuickSettings()
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Divider()
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(4.f, 6.f, 4.f, 4.f)
		[
			Details.ToSharedRef()
		]);
}

// ============================================================ Right: result

TSharedRef<SWidget> SIconForgeEditor::BuildResultPanel()
{
	TSharedRef<SWidget> Extra = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
		[
			Chip(LOCTEXT("LiveChip", "Live FX"),
				[this]() { return Settings->bLiveFXPreview && bShowLive; },
				[this](bool bOn)
				{
					Settings->bLiveFXPreview = bOn; bShowLive = bOn; bSessionDirty = true;
					if (bOn) { RequestLivePreview(true); }
				},
				LOCTEXT("LiveTip", "Live preview of the final icon WITH all effects (retro, outline, CRT...). Updates when you stop moving the camera or change a setting."))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 4.f, 0.f)
		[
			Chip(LOCTEXT("ABChip", "A/B"),
				[this]() { return bCompareRaw; },
				[this](bool bOn) { bCompareRaw = bOn; },
				LOCTEXT("ABTip", "Compare: show the live preview without effects"))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).Visibility_Lambda([this]() { return History.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				Button(LOCTEXT("Clear", "Clear"), FOnClicked::CreateSP(this, &SIconForgeEditor::OnClearHistory),
					LOCTEXT("ClearTip", "Clear the history strip (files and assets are kept)"), "IconForge.Button.Small")
			]
		];

	return Panel(LOCTEXT("ResultCap", "RESULT"),
		SNew(SVerticalBox)
		// Preview
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(14.f, 0.f, 14.f, 10.f)
		[
			SNew(SBorder).BorderImage(FIconForgeStyle::Brush("IconForge.Stage")).Padding(10.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
					.Visibility_Lambda([this]() { return GetResultBrush() ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SBox)
						// Tiny native sprites (32x32) are shown at least 256 px big
						.WidthOverride_Lambda([this]() -> FOptionalSize { const FVector2D Sz = GetResultSize(); return Sz.X > 0 ? FOptionalSize((float)Sz.X) : FOptionalSize(); })
						.HeightOverride_Lambda([this]() -> FOptionalSize { const FVector2D Sz = GetResultSize(); return Sz.Y > 0 ? FOptionalSize((float)Sz.Y) : FOptionalSize(); })
						[
							SNew(SOverlay)
							+ SOverlay::Slot()[ SNew(SImage).Image(FAppStyle::GetBrush("Checkerboard")) ]
							+ SOverlay::Slot()[ SNew(SImage).Image_Lambda([this]() { const FSlateBrush* B = GetResultBrush(); return B ? B : FAppStyle::GetNoBrush(); }) ]
						]
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
				[
					SNew(SBox).Visibility_Lambda([this]() { return bShowLive && LiveBrush ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						Pill(TAttribute<FText>::CreateLambda([this]()
						{
							const FText Mode = bCompareRaw ? LOCTEXT("LiveRaw", "LIVE \u00B7 NO FX") : LOCTEXT("LiveFx", "LIVE FX");
							return FText::Format(LOCTEXT("LivePill", "{0}  \u00B7  {1}\u00D7{2}  \u00B7  {3} ms"), Mode,
								FText::AsNumber(LiveSize.X, &FNumberFormattingOptions::DefaultNoGrouping()),
								FText::AsNumber(LiveSize.Y, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(FMath::RoundToInt(LastLiveMs)));
						}), FIconForgeStyle::Live())
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(10.f)
				[
					SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Muted").AutoWrapText(true).Justification(ETextJustify::Center)
					.Text(LOCTEXT("NoShot", "Your icons appear here.\nPress Shot! or Space."))
					.Visibility_Lambda([this]() { return GetResultBrush() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				]
			]
		]
		// Name + status
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 0.f, 14.f, 2.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return SelectedShot && !bShowLive ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).TextStyle(&St(), "IconForge.Text.H2")
				.Text_Lambda([this]() { return SelectedShot ? FText::FromString(SelectedShot->Name) : FText::GetEmpty(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([this]() { return SelectedShot && SelectedShot->Error.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
				[ Pill(LOCTEXT("Saved", "SAVED"), FIconForgeStyle::Live()) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).Visibility_Lambda([this]() { return SelectedShot && !SelectedShot->Error.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
				[ Pill(LOCTEXT("Problem", "PROBLEM"), FIconForgeStyle::Bad()) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 0.f, 14.f, 8.f)
		[
			SNew(STextBlock).TextStyle(&St(), "IconForge.Text.Mono").AutoWrapText(true)
			.Visibility_Lambda([this]() { return SelectedShot && !bShowLive ? EVisibility::Visible : EVisibility::Collapsed; })
			.Text_Lambda([this]()
			{
				if (!SelectedShot) { return FText::GetEmpty(); }
				return FText::FromString(FString::Printf(TEXT("%d\u00D7%d  \u00B7  %s  \u00B7  %s\n%s"), SelectedShot->Width, SelectedShot->Height,
					*SelectedShot->MeshName, *SelectedShot->Time.ToString(TEXT("%H:%M:%S")), *ShotPathText(*SelectedShot)));
			})
		]
		// Actions
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 0.f, 14.f, 10.f)
		[
			SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f))
			+ SWrapBox::Slot()[ Button(LOCTEXT("Folder", "Open Folder"), FOnClicked::CreateSP(this, &SIconForgeEditor::OnOpenFolder),
				LOCTEXT("FolderTip", "Show the PNG in the file explorer"), "IconForge.Button.Small", "Icons.FolderOpen") ]
			+ SWrapBox::Slot()[ Button(LOCTEXT("Browse", "Find in Content"), FOnClicked::CreateSP(this, &SIconForgeEditor::OnShowInContentBrowser),
				LOCTEXT("BrowseTip", "Select the texture asset in the Content Browser"), "IconForge.Button.Small", "Icons.Search") ]
			+ SWrapBox::Slot()[ Button(LOCTEXT("Copy", "Copy Path"), FOnClicked::CreateSP(this, &SIconForgeEditor::OnCopyPath),
				LOCTEXT("CopyTip", "Copy the asset reference (or PNG path) to the clipboard"), "IconForge.Button.Small", "GenericCommands.Copy") ]
		]
		// History
		+ SVerticalBox::Slot().AutoHeight().Padding(14.f, 0.f, 14.f, 6.f)
		[
			Caption(TAttribute<FText>::CreateLambda([this]() { return FText::Format(LOCTEXT("HistoryCap", "HISTORY  \u00B7  {0}"), FText::AsNumber(History.Num())); }))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 0.f, 10.f, 10.f)
		[
			SNew(SBox).MaxDesiredHeight(136.f)
			[
				SNew(SScrollBox).Orientation(Orient_Vertical)
				+ SScrollBox::Slot()[ SAssignNew(HistoryBox, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.f, 4.f)) ]
			]
		],
		Extra);
}

void SIconForgeEditor::RebuildHistoryStrip()
{
	if (!HistoryBox) { return; }
	HistoryBox->ClearChildren();
	for (const TSharedPtr<FIconForgeShot>& S : History)
	{
		const FText Tip = FText::FromString(S->Name + TEXT("\n") + ShotPathText(*S));
		HistoryBox->AddSlot()
		[
			SNew(SBox).WidthOverride(58.f).HeightOverride(58.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SButton).ButtonStyle(&St(), "IconForge.Button.Ghost").IsFocusable(false).ContentPadding(FMargin(3.f)).ToolTipText(Tip)
					.OnClicked_Lambda([this, S]() { SelectedShot = S; bShowLive = false; return FReply::Handled(); })
					[
						SNew(SOverlay)
						+ SOverlay::Slot()[ SNew(SImage).Image(FAppStyle::GetBrush("Checkerboard")) ]
						+ SOverlay::Slot()[ SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[ SNew(SImage).Image(S->Brush.Get()) ] ]
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SBorder).BorderImage(FIconForgeStyle::Brush("IconForge.Outline"))
					.Visibility_Lambda([this, S]() { return SelectedShot == S ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(5.f)
				[
					SNew(SBox).Visibility(S->Error.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
					[ Dot(FSlateColor(FIconForgeStyle::Bad()), 8.f) ]
				]
			]
		];
	}
}

TSharedRef<SWidget> SIconForgeEditor::BuildPresetMenu()
{
	FMenuBuilder MB(true, nullptr);
	MB.BeginSection("Save", LOCTEXT("SavePreset", "Save current settings as"));
	MB.AddWidget(SNew(SBox).WidthOverride(220.f).Padding(FMargin(8.f, 2.f))
	[
		SNew(SEditableTextBox).HintText(LOCTEXT("PresetHint", "Preset name + Enter"))
		.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type C)
		{
			if (C == ETextCommit::OnEnter && !T.IsEmptyOrWhitespace()) { SavePreset(T.ToString()); FSlateApplication::Get().DismissAllMenus(); }
		})
	], FText::GetEmpty());
	MB.EndSection();

	MB.BeginSection("Load", LOCTEXT("LoadPreset", "Load preset"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(UIconForgeSettings::GetPresetDir() / TEXT("*.json")), true, false);
	Files.Sort();
	if (Files.Num() == 0)
	{
		MB.AddWidget(SNew(STextBlock).Text(LOCTEXT("NoPresets", "  (no presets yet)")).ColorAndOpacity(FSlateColor::UseSubduedForeground()), FText::GetEmpty());
	}
	for (const FString& F : Files)
	{
		const FString Full = UIconForgeSettings::GetPresetDir() / F;
		MB.AddMenuEntry(FText::FromString(FPaths::GetBaseFilename(F)), FText::FromString(Full), FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Full]() { LoadPreset(Full); })));
	}
	MB.EndSection();
	MB.AddMenuEntry(LOCTEXT("OpenPresetDir", "Open presets folder"), FText::GetEmpty(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			const FString D = UIconForgeSettings::GetPresetDir();
			IFileManager::Get().MakeDirectory(*D, true);
			FPlatformProcess::ExploreFolder(*D);
		})));
	return MB.MakeWidget();
}

// ============================================================ Settings / session

void SIconForgeEditor::OnSettingsChanged(bool bReframe)
{
	bSessionDirty = true;     // written to disk by the autosave timer (not on every slider tick)
	if (Viewport && Viewport->GetClient())
	{
		TSharedPtr<FIconForgeViewportClient> C = Viewport->GetClient();
		C->ViewFOV = Settings->FieldOfView;
		C->ResetCachedFov();
		Studio->ApplySettings(*Settings, C->GetCameraRotation());
		C->Invalidate();
	}
	if (bReframe) { OnFrame(); }
	RequestLivePreview(true);
}

void SIconForgeEditor::SaveSessionNow()
{
	if (!Settings) { return; }
	if (Viewport && Viewport->GetClient())
	{
		Settings->LastCameraRotation = Viewport->GetClient()->GetDesiredRotation();
		Settings->LastCameraRotation.Yaw = FRotator::NormalizeAxis(Settings->LastCameraRotation.Yaw);
		Settings->LastCameraRotation.Roll = 0.f;
	}
	Settings->SaveToFile(UIconForgeSettings::GetSessionFile());
	bSessionDirty = false;
}

EActiveTimerReturnType SIconForgeEditor::AutosaveTimer(double, float)
{
	if (bSessionDirty && !bBatchRunning) { SaveSessionNow(); }
	return EActiveTimerReturnType::Continue;
}

void SIconForgeEditor::SavePreset(const FString& InName)
{
	const FString Clean = FPaths::MakeValidFileName(InName.TrimStartAndEnd());
	if (Clean.IsEmpty()) { Notify(TEXT("Invalid preset name"), false); return; }
	const FString File = UIconForgeSettings::GetPresetDir() / (Clean + TEXT(".json"));
	const bool bOk = Settings->SaveToFile(File);
	Notify(bOk ? FString::Printf(TEXT("Preset saved: %s"), *Clean) : FString::Printf(TEXT("Failed to save preset: %s"), *File), bOk);
}

void SIconForgeEditor::LoadPreset(const FString& File)
{
	// Presets carry look & output, not the session state (list filters, last camera angle)
	const bool bProj = Settings->bProjectContentOnly, bLod = Settings->bHideLODMeshes;
	const FRotator LastCam = Settings->LastCameraRotation;
	const bool bOk = Settings->LoadFromFile(File);
	Settings->bProjectContentOnly = bProj; Settings->bHideLODMeshes = bLod; Settings->LastCameraRotation = LastCam;
	Details->ForceRefresh();
	OnSettingsChanged(true);
	Notify(bOk ? FString::Printf(TEXT("Preset loaded: %s"), *FPaths::GetBaseFilename(File)) : TEXT("Failed to load preset"), bOk);
}

FReply SIconForgeEditor::OnResetSettings()
{
	const bool bProj = Settings->bProjectContentOnly, bLod = Settings->bHideLODMeshes;
	const FRotator LastCam = Settings->LastCameraRotation;
	Settings->ResetToDefaults();
	Settings->bProjectContentOnly = bProj; Settings->bHideLODMeshes = bLod; Settings->LastCameraRotation = LastCam;
	Details->ForceRefresh();
	OnSettingsChanged(true);
	Notify(TEXT("Settings reset to defaults"), true);
	return FReply::Handled();
}

void SIconForgeEditor::SetSquareSize(int32 Size)
{
	Settings->Width = Size;
	Settings->Height = Size;
	OnSettingsChanged(true);
}

// ============================================================ Mesh / camera

void SIconForgeEditor::SelectMesh(const FAssetData& Asset)
{
	if (bBatchRunning) { return; }
	UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
	if (!Mesh) { return; }
	Studio->SetMesh(Mesh);
	if (Settings->bAutoFrameOnSelect && Viewport->GetClient())
	{
		Studio->ApplySettings(*Settings, Viewport->GetClient()->GetCameraRotation());
		Viewport->GetClient()->FrameMesh(nullptr, true);
	}
	RequestLivePreview(true);
}

void SIconForgeEditor::OpenAssets(const TArray<FAssetData>& Assets)
{
	TArray<FAssetData> Meshes;
	bool bOutsideGame = false;
	for (const FAssetData& A : Assets)
	{
		if (A.IsInstanceOf(UStaticMesh::StaticClass()))
		{
			Meshes.Add(A);
			bOutsideGame |= !A.PackageName.ToString().StartsWith(TEXT("/Game/"));
		}
	}
	if (Meshes.Num() == 0) { return; }

	if (bOutsideGame && Settings->bProjectContentOnly)
	{
		// Otherwise the selected engine / plugin meshes would be invisible in the list
		Settings->bProjectContentOnly = false;
		OnSettingsChanged();
		RebuildAssetPicker();
	}
	SelectMesh(Meshes[0]);
	if (SyncToAssets.IsBound()) { SyncToAssets.Execute(Meshes); }   // selected in the list => ready for Batch
}

void SIconForgeEditor::ApplyCameraPreset(EIconForgeCameraPreset P)
{
	if (TSharedPtr<FIconForgeViewportClient> C = Viewport->GetClient())
	{
		const FRotator R = PresetRotation(P);
		Studio->ApplySettings(*Settings, R);
		C->FrameMesh(&R);
		bSessionDirty = true;
		RequestLivePreview(true);
	}
}

void SIconForgeEditor::ApplyLightPreset(EIconForgeLightPreset P)
{
	Settings->ApplyLightPreset(P);
	Details->ForceRefresh();
	OnSettingsChanged();
}

FReply SIconForgeEditor::OnFrame()
{
	if (Viewport && Viewport->GetClient())
	{
		Studio->ApplySettings(*Settings, Viewport->GetClient()->GetCameraRotation());
		Viewport->GetClient()->FrameMesh();
	}
	return FReply::Handled();
}

bool SIconForgeEditor::HandleHotkey(const FKeyEvent& E)
{
	if (bBatchRunning || E.IsControlDown() || E.IsAltDown() || E.IsCommandDown()) { return false; }
	const FKey K = E.GetKey();
	if (K == EKeys::Enter || K == EKeys::SpaceBar)
	{
		if (!E.IsRepeat()) { OnShot(); }     // holding Space must not fire a shot every frame
		return true;
	}
	if (K == EKeys::F) { OnFrame(); return true; }
	if (K == EKeys::R) { ApplyCameraPreset(EIconForgeCameraPreset::ThreeQuarter); return true; }
	if (K == EKeys::G) { Settings->bShowGuides = !Settings->bShowGuides; OnSettingsChanged(); return true; }
	static const FKey Num[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven };
	static const FKey Pad[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven };
	for (int32 i = 0; i < 7; ++i)
	{
		if (K == Num[i] || K == Pad[i]) { ApplyCameraPreset((EIconForgeCameraPreset)i); return true; }
	}
	return false;
}

FReply SIconForgeEditor::OnKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	if (!IsTextInputFocused() && HandleHotkey(E)) { return FReply::Handled(); }
	return SCompoundWidget::OnKeyDown(G, E);
}

// ============================================================ Shooting

FString SIconForgeEditor::MakeIconName(const FString& MeshName) const
{
	const FString Base = Settings->MakeName(MeshName);
	if (Settings->bOverwriteExisting) { return Base; }

	const FString PngDir = Settings->ResolvePNGDirectory();
	const FString AssetDir = Settings->ResolveTexturePath();
	auto Taken = [&](const FString& N)
	{
		if (Settings->bSavePNG && FPaths::FileExists(PngDir / (N + TEXT(".png")))) { return true; }
		if (Settings->bCreateTextureAsset)
		{
			const FString Pkg = AssetDir / N;
			if (FindPackage(nullptr, *Pkg) || FPackageName::DoesPackageExist(Pkg)) { return true; }
		}
		return false;
	};
	FString Name = Base;
	for (int32 i = 2; Taken(Name) && i < 10000; ++i) { Name = FString::Printf(TEXT("%s_%d"), *Base, i); }
	return Name;
}

bool SIconForgeEditor::Shoot(const FIconForgeView& View, FString& OutMsg, TArray<UPackage*>* DeferredSave)
{
	UStaticMesh* Mesh = Studio->GetMesh();
	if (!Mesh) { OutMsg = TEXT("Select a Static Mesh first"); return false; }
	if (!Settings->bSavePNG && !Settings->bCreateTextureAsset) { OutMsg = TEXT("Nothing to save: enable PNG file and/or Texture asset (SETTINGS > OUTPUT)"); return false; }

	TArray<FColor> Px;
	const float TanV = View.TanHalfVFov > 0.f ? View.TanHalfVFov : Viewport->GetClient()->GetTanHalfVFov();
	if (!Studio->Render(*Settings, View.Location, View.Rotation, TanV, Px))
	{
		OutMsg = FString::Printf(TEXT("Render failed for %s"), *Mesh->GetName());
		return false;
	}

	// Same code path as the live preview: background, retro, outlines, CRT...
	int32 OutW = Settings->Width, OutH = Settings->Height;
	IconForgeFX::Process(*Settings, Px, OutW, OutH);

	TSharedRef<FIconForgeShot> Shot = MakeShared<FIconForgeShot>();
	Shot->MeshName = Mesh->GetName();
	Shot->Name = MakeIconName(Shot->MeshName);
	Shot->Width = OutW;
	Shot->Height = OutH;
	Shot->Time = FDateTime::Now();
	const bool bOk = SaveIcon(Px, *Shot, DeferredSave);
	AddToHistory(Px, Shot);
	OutMsg = bOk ? Shot->Name : Shot->Error;
	return bOk;
}

FReply SIconForgeEditor::OnShot()
{
	TSharedPtr<FIconForgeViewportClient> C = Viewport ? Viewport->GetClient() : nullptr;
	if (!C || bBatchRunning) { return FReply::Handled(); }
	FString Msg;
	const bool bOk = Shoot(C->SnapAndGetView(), Msg, nullptr);   // finish smoothing, shoot exactly what is targeted
	Studio->ReleaseTargets();
	bShowLive = false;          // show the fresh shot in RESULT
	Notify(bOk ? FString::Printf(TEXT("Icon saved: %s"), *Msg) : Msg, bOk);
	return FReply::Handled();
}

FReply SIconForgeEditor::OnBatchShot()
{
	TArray<FAssetData> Assets = GetSelectedAssets.IsBound() ? GetSelectedAssets.Execute() : TArray<FAssetData>();
	if (Assets.Num() == 0) { Notify(TEXT("Select meshes in the list (Ctrl/Shift + click)"), false); return FReply::Handled(); }
	TSharedPtr<FIconForgeViewportClient> C = Viewport->GetClient();
	if (!C || bBatchRunning) { return FReply::Handled(); }

	bBatchRunning = true;
	ON_SCOPE_EXIT { bBatchRunning = false; };

	const FIconForgeOrbit StartOrbit = C->GetOrbit();         // exact user framing, restored afterwards
	const FIconForgeView Start = C->SnapAndGetView();
	TStrongObjectPtr<UStaticMesh> Prev(Studio->GetMesh());
	TArray<UPackage*> ToSave;
	int32 Done = 0, Tried = 0;
	FString FirstError;
	{
		FScopedSlowTask Task(Assets.Num(), LOCTEXT("BatchProgress", "Rendering icons..."));
		Task.MakeDialog(true);
		for (const FAssetData& A : Assets)
		{
			if (Task.ShouldCancel()) { break; }
			Task.EnterProgressFrame(1.f, FText::Format(LOCTEXT("BatchItem", "Rendering {0}"), FText::FromName(A.AssetName)));
			UStaticMesh* M = Cast<UStaticMesh>(A.GetAsset());
			if (!M) { continue; }
			++Tried;
			Studio->SetMesh(M);
			Studio->ApplySettings(*Settings, Start.Rotation);
			const FIconForgeView View = Settings->bAutoFrameInBatch ? C->FrameMesh(&Start.Rotation, true) : Start;
			FString Msg;
			if (Shoot(View, Msg, &ToSave)) { ++Done; }
			else if (FirstError.IsEmpty()) { FirstError = Msg; }
		}
	}
	Studio->ReleaseTargets();

	// One save for all packages instead of one per icon
	if (ToSave.Num() > 0 && Settings->bSaveTextureAssetToDisk)
	{
		if (!UEditorLoadingAndSavingUtils::SavePackages(ToSave, false) && FirstError.IsEmpty())
		{
			FirstError = TEXT("Some texture packages could not be saved");
		}
	}

	Studio->SetMesh(Prev.Get());
	Studio->ApplySettings(*Settings, Start.Rotation);
	C->SetOrbit(StartOrbit, true);

	FString Msg = FString::Printf(TEXT("Batch done: %d / %d icons"), Done, Tried);
	if (!FirstError.IsEmpty()) { Msg += TEXT("\n") + FirstError; }
	Notify(Msg, Done > 0 && Done == Tried);
	return FReply::Handled();
}

bool SIconForgeEditor::SaveIcon(const TArray<FColor>& Px, FIconForgeShot& Shot, TArray<UPackage*>* DeferredSave)
{
	const int32 W = Shot.Width, H = Shot.Height;   // may differ from Settings (native pixel-art output)
	const FString Name = Shot.Name;
	TArray<FString> Errors;
	bool bAny = false;

	if (Px.Num() != W * H) { Shot.Error = TEXT("Internal error: pixel buffer size mismatch"); return false; }

	if (Settings->bSavePNG)
	{
		const FString Dir = Settings->ResolvePNGDirectory();
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString File = Dir / (Name + TEXT(".png"));
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(W, H, TArrayView64<const FColor>(Px.GetData(), Px.Num()), Png);
		if (Png.Num() > 0 && FFileHelper::SaveArrayToFile(Png, *File)) { Shot.PngFile = File; bAny = true; }
		else { Errors.Add(FString::Printf(TEXT("Could not write PNG: %s"), *File)); }
	}

	if (Settings->bCreateTextureAsset)
	{
		const FString PkgName = Settings->ResolveTexturePath() / Name;
		if (!FPackageName::IsValidLongPackageName(PkgName))
		{
			Errors.Add(FString::Printf(TEXT("Invalid texture asset path: %s (use e.g. /Game/Icons)"), *PkgName));
		}
		else
		{
			UPackage* Pkg = CreatePackage(*PkgName);
			Pkg->FullyLoad();
			UObject* Existing = FindObject<UObject>(Pkg, *Name);
			if (Existing && !Existing->IsA<UTexture2D>())
			{
				// NewObject over an object of another class would assert
				Errors.Add(FString::Printf(TEXT("%s already exists and is a %s, not a texture"), *PkgName, *Existing->GetClass()->GetName()));
			}
			else
			{
				UTexture2D* Tex = Cast<UTexture2D>(Existing);
				const bool bNew = Tex == nullptr;
				if (bNew) { Tex = NewObject<UTexture2D>(Pkg, *Name, RF_Public | RF_Standalone | RF_Transactional); }
				Tex->PreEditChange(nullptr);
				Tex->Source.Init(W, H, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Px.GetData()));
				Tex->SRGB = true;
				Tex->CompressionSettings = TC_EditorIcon;
				Tex->LODGroup = TEXTUREGROUP_UI;
				Tex->MipGenSettings = TMGS_NoMipmaps;
				// Gradient backgrounds are opaque, rounded corners are not: keep alpha whenever the image has any
				Tex->CompressionNoAlpha = !Px.ContainsByPredicate([](const FColor& C) { return C.A < 255; });
				// Pixel art must stay crisp when the UI scales it
				Tex->Filter = Settings->WantsPixelFiltering() ? TF_Nearest : TF_Default;
				Tex->PostEditChange();
				if (bNew) { FAssetRegistryModule::AssetCreated(Tex); }
				Pkg->MarkPackageDirty();
				if (Settings->bSaveTextureAssetToDisk)
				{
					if (DeferredSave) { DeferredSave->AddUnique(Pkg); }
					else if (!UEditorLoadingAndSavingUtils::SavePackages({ Pkg }, false))
					{
						Errors.Add(FString::Printf(TEXT("Texture created but could not be saved: %s"), *PkgName));
					}
				}
				Shot.AssetObjectPath = PkgName + TEXT(".") + Name;
				bAny = true;
			}
		}
	}

	Shot.Error = FString::Join(Errors, TEXT("\n"));
	return bAny && Errors.Num() == 0;
}

void SIconForgeEditor::AddToHistory(const TArray<FColor>& Px, TSharedRef<FIconForgeShot> Shot)
{
	const int32 W = Shot->Width, H = Shot->Height;
	UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	if (!T) { return; }
	T->SRGB = true;
	T->Filter = Settings->WantsPixelFiltering() ? TF_Nearest : TF_Default;
	void* Data = T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
	T->GetPlatformData()->Mips[0].BulkData.Unlock();
	T->UpdateResource();

	Shot->Texture.Reset(T);
	Shot->Brush = MakeShared<FSlateBrush>();
	Shot->Brush->SetResourceObject(T);
	Shot->Brush->ImageSize = FVector2D(W, H);

	History.Insert(Shot, 0);
	if (History.Num() > 32) { History.SetNum(32); }
	SelectedShot = Shot;
	RebuildHistoryStrip();
}

// ============================================================ Result actions

FReply SIconForgeEditor::OnOpenFolder()
{
	if (SelectedShot && !SelectedShot->PngFile.IsEmpty() && FPaths::FileExists(SelectedShot->PngFile))
	{
		FPlatformProcess::ExploreFolder(*SelectedShot->PngFile); // opens the explorer with the file selected
		return FReply::Handled();
	}
	const FString Dir = Settings->ResolvePNGDirectory();
	IFileManager::Get().MakeDirectory(*Dir, true);
	FPlatformProcess::ExploreFolder(*Dir);
	return FReply::Handled();
}

FReply SIconForgeEditor::OnShowInContentBrowser()
{
	if (!SelectedShot || SelectedShot->AssetObjectPath.IsEmpty()) { Notify(TEXT("No texture asset for this shot"), false); return FReply::Handled(); }
	if (UObject* Obj = LoadObject<UTexture2D>(nullptr, *SelectedShot->AssetObjectPath))
	{
		FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> Sel;
		Sel.Add(FAssetData(Obj));
		CB.Get().SyncBrowserToAssets(Sel);
	}
	else
	{
		Notify(TEXT("The texture asset no longer exists"), false);
	}
	return FReply::Handled();
}

FReply SIconForgeEditor::OnCopyPath()
{
	if (!SelectedShot) { Notify(TEXT("Nothing to copy yet"), false); return FReply::Handled(); }
	FString Text;
	if (!SelectedShot->AssetObjectPath.IsEmpty()) { Text = FString::Printf(TEXT("/Script/Engine.Texture2D'%s'"), *SelectedShot->AssetObjectPath); }
	else { Text = SelectedShot->PngFile; }
	if (Text.IsEmpty()) { Notify(TEXT("This shot was not saved"), false); return FReply::Handled(); }
	FPlatformApplicationMisc::ClipboardCopy(*Text);
	Notify(TEXT("Copied to clipboard"), true);
	return FReply::Handled();
}

FReply SIconForgeEditor::OnClearHistory()
{
	History.Reset();
	SelectedShot.Reset();
	RebuildHistoryStrip();
	return FReply::Handled();
}

FReply SIconForgeEditor::OnDonate()
{
	FString Error;
	FPlatformProcess::LaunchURL(DonateUrl, nullptr, &Error);
	if (!Error.IsEmpty()) { Notify(FString::Printf(TEXT("Could not open the browser: %s"), DonateUrl), false); }
	return FReply::Handled();
}

void SIconForgeEditor::Notify(const FString& Msg, bool bSuccess)
{
	FNotificationInfo Info(FText::FromString(Msg));
	Info.ExpireDuration = bSuccess ? 3.f : 6.f;
	Info.bUseSuccessFailIcons = true;
	TSharedPtr<SNotificationItem> N = FSlateNotificationManager::Get().AddNotification(Info);
	if (N) { N->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail); }
}

// ============================================================ Live FX preview

void SIconForgeEditor::RequestLivePreview(bool bRerender)
{
	if (!Settings || !Settings->bLiveFXPreview) { return; }
	if (bRerender || LiveRaw.Num() == 0) { bLiveNeedsRender = true; }
	bLiveNeedsFX = true;
	LiveRequestTime = FPlatformTime::Seconds();
	bShowLive = true;
}

void SIconForgeEditor::UpdateLivePreview()
{
	if (!Settings || !Studio || !Viewport || bBatchRunning) { return; }
	TSharedPtr<FIconForgeViewportClient> C = Viewport->GetClient();
	if (!C) { return; }

	// Camera moved? Wait until the user stops (debounce), then capture once
	const FIconForgeOrbit O = C->GetOrbit();
	if (!O.Target.Equals(LastSeenOrbit.Target, 0.01) || !FMath::IsNearlyEqual(O.Yaw, LastSeenOrbit.Yaw, 0.01) ||
		!FMath::IsNearlyEqual(O.Pitch, LastSeenOrbit.Pitch, 0.01) || !FMath::IsNearlyEqual(O.Distance, LastSeenOrbit.Distance, 0.01))
	{
		LastSeenOrbit = O;
		if (Settings->bLiveFXPreview) { bLiveNeedsRender = true; bLiveNeedsFX = true; LiveRequestTime = FPlatformTime::Seconds(); }
	}

	if (!Settings->bLiveFXPreview || (!bLiveNeedsRender && !bLiveNeedsFX) || !Studio->GetMesh()) { return; }
	const double Now = FPlatformTime::Seconds();
	const double Delay = bLiveNeedsRender ? 0.3 : 0.06;     // effects alone are re-applied almost instantly
	if (Now - LiveRequestTime < Delay) { return; }
	if (FSlateApplication::Get().GetPressedMouseButtons().Num() > 0 && bLiveNeedsRender) { return; }   // still dragging

	const double T0 = FPlatformTime::Seconds();
	if (bLiveNeedsRender || LiveRawW != Settings->Width || LiveRawH != Settings->Height)
	{
		const FIconForgeView V = C->SnapAndGetView();   // camera is at rest here (debounced)
		TArray<FColor> Raw;
		const float TanV = V.TanHalfVFov > 0.f ? V.TanHalfVFov : C->GetTanHalfVFov();
		if (!Studio->Render(*Settings, V.Location, V.Rotation, TanV, Raw, true))
		{
			bLiveNeedsRender = bLiveNeedsFX = false;
			return;
		}
		LiveRaw = MoveTemp(Raw);
		LiveRawW = Settings->Width; LiveRawH = Settings->Height;
		// A/B texture: raw image with the plain output background
		TArray<FColor> RawShown = LiveRaw;
		int32 Rw = LiveRawW, Rh = LiveRawH;
		{
			TStrongObjectPtr<UIconForgeSettings> Plain(DuplicateObject<UIconForgeSettings>(Settings.Get(), GetTransientPackage()));
			Plain->ResetEffects();
			IconForgeFX::Process(*Plain, RawShown, Rw, Rh);
		}
		UTexture2D* RT = UTexture2D::CreateTransient(Rw, Rh, PF_B8G8R8A8);
		if (RT)
		{
			RT->SRGB = true;
			void* D = RT->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(D, RawShown.GetData(), RawShown.Num() * sizeof(FColor));
			RT->GetPlatformData()->Mips[0].BulkData.Unlock();
			RT->UpdateResource();
			LiveRawTexture.Reset(RT);
			if (!LiveRawBrush) { LiveRawBrush = MakeShared<FSlateBrush>(); }
			LiveRawBrush->SetResourceObject(RT);
			LiveRawBrush->ImageSize = FVector2D(Rw, Rh);
		}
	}
	bLiveNeedsRender = false;

	TArray<FColor> Px = LiveRaw;
	int32 W = LiveRawW, H = LiveRawH;
	IconForgeFX::Process(*Settings, Px, W, H);
	UploadLiveTexture(Px, W, H);
	bLiveNeedsFX = false;
	LastLiveMs = (FPlatformTime::Seconds() - T0) * 1000.0;
}

void SIconForgeEditor::UploadLiveTexture(const TArray<FColor>& Px, int32 W, int32 H)
{
	if (Px.Num() != W * H || W <= 0 || H <= 0) { return; }
	UTexture2D* T = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	if (!T) { return; }
	T->SRGB = true;
	T->Filter = Settings->WantsPixelFiltering() ? TF_Nearest : TF_Default;
	void* Data = T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
	T->GetPlatformData()->Mips[0].BulkData.Unlock();
	T->UpdateResource();
	LiveTexture.Reset(T);
	if (!LiveBrush) { LiveBrush = MakeShared<FSlateBrush>(); }
	LiveBrush->SetResourceObject(T);
	LiveBrush->ImageSize = FVector2D(W, H);
	LiveSize = FIntPoint(W, H);
}

const FSlateBrush* SIconForgeEditor::GetResultBrush() const
{
	if (bShowLive && LiveBrush)
	{
		return (bCompareRaw && LiveRawBrush) ? LiveRawBrush.Get() : LiveBrush.Get();
	}
	return SelectedShot && SelectedShot->Brush ? SelectedShot->Brush.Get() : nullptr;
}

FVector2D SIconForgeEditor::GetResultSize() const
{
	FVector2D Sz = FVector2D::ZeroVector;
	if (const FSlateBrush* B = GetResultBrush()) { Sz = B->ImageSize; }
	if (Sz.X > 0 && Sz.Y > 0 && FMath::Max(Sz.X, Sz.Y) < 256.0)
	{
		Sz *= FMath::CeilToDouble(256.0 / FMath::Max(Sz.X, Sz.Y));   // integer zoom keeps pixel art crisp
	}
	return Sz;
}

#undef LOCTEXT_NAMESPACE
