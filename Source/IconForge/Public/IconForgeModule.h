#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SIconForgeEditor;
struct FAssetData;

class FIconForgeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Opens (or focuses) the Icon Forge tab and puts the given Static Meshes on stage / in the Batch selection. */
	static void OpenWithAssets(const TArray<FAssetData>& Assets);

private:
	void RegisterMenus();
	static TWeakPtr<SIconForgeEditor> EditorWidget;
};
