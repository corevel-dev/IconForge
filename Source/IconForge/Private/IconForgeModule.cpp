#include "IconForgeModule.h"
#include "SIconForgeEditor.h"
#include "Style/IconForgeStyle.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "Styling/AppStyle.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetData.h"

#define LOCTEXT_NAMESPACE "IconForge"

namespace
{
	const FName IconForgeTab(TEXT("IconForge"));
}

TWeakPtr<SIconForgeEditor> FIconForgeModule::EditorWidget;

void FIconForgeModule::StartupModule()
{
	FIconForgeStyle::Initialize();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(IconForgeTab, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
	{
		TSharedRef<SIconForgeEditor> W = SNew(SIconForgeEditor);
		EditorWidget = W;
		return SNew(SDockTab).TabRole(ETabRole::NomadTab).Label(LOCTEXT("Tab", "Icon Forge"))[ W ];
	}))
	.SetDisplayName(LOCTEXT("Tab", "Icon Forge"))
	.SetTooltipText(LOCTEXT("TabTip", "Render item icons from Static Meshes"))
	.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"))
	.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FIconForgeModule::RegisterMenus));
}

void FIconForgeModule::RegisterMenus()
{
	FToolMenuOwnerScoped Owner(this);

	// Tools > Icon Forge
	if (UToolMenu* Tools = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& S = Tools->FindOrAddSection("IconForge", LOCTEXT("Sec", "Icon Forge"));
		S.AddMenuEntry("OpenIconForge", LOCTEXT("Open", "Icon Forge"), LOCTEXT("OpenTip", "Open the icon studio"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
			FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(IconForgeTab); })));
	}

	// Right click on Static Mesh(es) in the Content Browser
	if (UToolMenu* Ctx = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.StaticMesh"))
	{
		FToolMenuSection& CS = Ctx->FindOrAddSection("GetAssetActions");
		CS.AddDynamicEntry("IconForgeCtx", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& In)
		{
			const UContentBrowserAssetContextMenuContext* C = In.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!C) { return; }
			TArray<FAssetData> Assets;
			for (const FAssetData& A : C->SelectedAssets)
			{
				if (A.IsInstanceOf(UStaticMesh::StaticClass())) { Assets.Add(A); }
			}
			if (Assets.Num() == 0) { return; }
			const FText Label = Assets.Num() == 1
				? LOCTEXT("Make", "Icon Forge: Make Icon")
				: FText::Format(LOCTEXT("MakeN", "Icon Forge: Make Icons ({0})"), FText::AsNumber(Assets.Num()));
			In.AddMenuEntry("IconForgeMake", Label,
				LOCTEXT("MakeTip", "Open in Icon Forge. With several meshes selected they are all selected for Batch."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
				FUIAction(FExecuteAction::CreateLambda([Assets] { FIconForgeModule::OpenWithAssets(Assets); })));
		}));
	}
}

void FIconForgeModule::OpenWithAssets(const TArray<FAssetData>& Assets)
{
	FGlobalTabmanager::Get()->TryInvokeTab(IconForgeTab);
	if (TSharedPtr<SIconForgeEditor> W = EditorWidget.Pin())
	{
		W->OpenAssets(Assets);
	}
}

void FIconForgeModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(IconForgeTab);
	}
	FIconForgeStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FIconForgeModule, IconForge)
