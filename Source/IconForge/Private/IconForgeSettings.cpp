#include "IconForgeSettings.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

static FString ToFull(const FString& In)
{
	FString P = FPaths::ConvertRelativePathToFull(In);
	FPaths::NormalizeDirectoryName(P);
	FPaths::RemoveDuplicateSlashes(P);
	return P;
}

FString UIconForgeSettings::ResolvePNGDirectory() const
{
	FString Dir = PNGDirectory.Path;
	Dir.TrimStartAndEndInline();
	if (Dir.IsEmpty()) { Dir = FPaths::ProjectSavedDir() / TEXT("Icons"); }
	else if (FPaths::IsRelative(Dir)) { Dir = FPaths::ProjectDir() / Dir; }
	return ToFull(Dir);
}

FString UIconForgeSettings::ResolveTexturePath() const
{
	FString P = TextureAssetPath.Path;
	P.TrimStartAndEndInline();
	if (P.IsEmpty()) { return TEXT("/Game/Icons"); }
	P.ReplaceInline(TEXT("\\"), TEXT("/"));
	// Accept "Content/Icons" or an absolute disk path inside the Content folder as well as "/Game/Icons"
	const FString ContentDir = ToFull(FPaths::ProjectContentDir());
	if (!FPaths::IsRelative(P) && P.Contains(TEXT(":")))
	{
		const FString Full = ToFull(P);
		P = Full.StartsWith(ContentDir) ? Full.RightChop(ContentDir.Len()) : FString(TEXT("Icons"));
	}
	if (P.StartsWith(TEXT("Content/"))) { P.RightChopInline(8); }
	if (!P.StartsWith(TEXT("/"))) { P = TEXT("/Game/") + P; }
	FPaths::RemoveDuplicateSlashes(P);
	while (P.Len() > 1 && P.EndsWith(TEXT("/"))) { P.LeftChopInline(1); }
	return P;
}

FString UIconForgeSettings::SanitizeName(const FString& In)
{
	// Valid both as a file name and as an asset/package name
	FString Out;
	Out.Reserve(In.Len());
	for (TCHAR C : In)
	{
		Out.AppendChar((FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-')) ? C : TEXT('_'));
	}
	while (Out.Contains(TEXT("__"))) { Out.ReplaceInline(TEXT("__"), TEXT("_")); }
	Out.TrimCharInline(TEXT('_'), nullptr);
	return Out.IsEmpty() ? FString(TEXT("Icon")) : Out;
}

FString UIconForgeSettings::MakeName(const FString& MeshName) const
{
	FString Base = MeshName;
	for (const TCHAR* Prefix : { TEXT("SM_"), TEXT("S_") })
	{
		if (Base.StartsWith(Prefix, ESearchCase::IgnoreCase) && Base.Len() > FCString::Strlen(Prefix))
		{
			Base.RightChopInline(FCString::Strlen(Prefix));
			break;
		}
	}
	FString N = NamePattern.TrimStartAndEnd().IsEmpty() ? FString(TEXT("T_{Name}_Icon")) : NamePattern;
	N.ReplaceInline(TEXT("{BaseName}"), *Base);
	N.ReplaceInline(TEXT("{Name}"), *MeshName);
	N.ReplaceInline(TEXT("{W}"), *FString::FromInt(Width));
	N.ReplaceInline(TEXT("{H}"), *FString::FromInt(Height));
	return SanitizeName(N);
}

int32 UIconForgeSettings::GetEffectiveSuperSampling() const
{
	int32 SS = FMath::Clamp(SuperSampling, 1, 4);
	while (SS > 1 && (Width * SS > 8192 || Height * SS > 8192)) { --SS; }
	return SS;
}

FString UIconForgeSettings::GetSessionFile() { return ToFull(FPaths::ProjectSavedDir() / TEXT("IconForge/Session.json")); }
FString UIconForgeSettings::GetPresetDir()   { return ToFull(FPaths::ProjectSavedDir() / TEXT("IconForge/Presets")); }

bool UIconForgeSettings::SaveToFile(const FString& Path) const
{
	FString Out;
	if (!FJsonObjectConverter::UStructToJsonObjectString(GetClass(), this, Out, 0, CPF_Transient)) { return false; }
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool UIconForgeSettings::LoadFromFile(const FString& Path)
{
	FString In;
	if (!FFileHelper::LoadFileToString(In, *Path)) { return false; }
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(In);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) { return false; }
	const bool bOk = FJsonObjectConverter::JsonObjectToUStruct(Obj.ToSharedRef(), GetClass(), this, 0, CPF_Transient);

	// Files can be hand-edited or come from an older version: enforce the same limits as the UI
	Width = FMath::Clamp(Width, 16, 4096);
	Height = FMath::Clamp(Height, 16, 4096);
	SuperSampling = FMath::Clamp(SuperSampling, 1, 4);
	FieldOfView = FMath::Clamp(FieldOfView, 5.f, 120.f);
	FramingPadding = FMath::Clamp(FramingPadding, 0.5f, 3.f);
	CameraSmoothing = FMath::Clamp(CameraSmoothing, 0.f, 40.f);
	PrecisionMultiplier = FMath::Clamp(PrecisionMultiplier, 0.05f, 1.f);
	return bOk;
}

void UIconForgeSettings::ResetToDefaults()
{
	const UIconForgeSettings* D = GetDefault<UIconForgeSettings>();
	for (TFieldIterator<FProperty> It(GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		It->CopyCompleteValue_InContainer(this, D);
	}
}

void UIconForgeSettings::ApplyLightPreset(EIconForgeLightPreset P)
{
	bLightsFollowCamera = true;
	switch (P)
	{
	case EIconForgeLightPreset::Studio:
		KeyRotation = FRotator(-40, 40, 0);  KeyIntensity = 5.f;  KeyColor = FLinearColor(1.f, 0.96f, 0.9f); bKeyShadows = true;
		FillRotation = FRotator(-10, -60, 0); FillIntensity = 1.8f; FillColor = FLinearColor(0.75f, 0.85f, 1.f);
		RimRotation = FRotator(-25, 170, 0); RimIntensity = 4.f;  RimColor = FLinearColor::White;
		SkyIntensity = 1.f; break;
	case EIconForgeLightPreset::Soft:
		KeyRotation = FRotator(-30, 25, 0);  KeyIntensity = 3.f;  KeyColor = FLinearColor::White; bKeyShadows = false;
		FillRotation = FRotator(-5, -50, 0); FillIntensity = 2.5f; FillColor = FLinearColor::White;
		RimRotation = FRotator(-20, 180, 0); RimIntensity = 1.5f; RimColor = FLinearColor::White;
		SkyIntensity = 2.f; break;
	case EIconForgeLightPreset::Dramatic:
		KeyRotation = FRotator(-35, 75, 0);  KeyIntensity = 8.f;  KeyColor = FLinearColor(1.f, 0.9f, 0.8f); bKeyShadows = true;
		FillRotation = FRotator(-10, -70, 0); FillIntensity = 0.3f; FillColor = FLinearColor(0.6f, 0.7f, 1.f);
		RimRotation = FRotator(-15, 160, 0); RimIntensity = 7.f;  RimColor = FLinearColor(1.f, 0.85f, 0.7f);
		SkyIntensity = 0.3f; break;
	case EIconForgeLightPreset::CoolRim:
		KeyRotation = FRotator(-40, 35, 0);  KeyIntensity = 4.f;  KeyColor = FLinearColor::White; bKeyShadows = true;
		FillRotation = FRotator(-10, -60, 0); FillIntensity = 1.2f; FillColor = FLinearColor::White;
		RimRotation = FRotator(-20, 175, 0); RimIntensity = 8.f;  RimColor = FLinearColor(0.3f, 0.7f, 1.f);
		SkyIntensity = 0.8f; break;
	case EIconForgeLightPreset::Warm:
		KeyRotation = FRotator(-35, 45, 0);  KeyIntensity = 5.f;  KeyColor = FLinearColor(1.f, 0.8f, 0.55f); bKeyShadows = true;
		FillRotation = FRotator(-10, -60, 0); FillIntensity = 1.5f; FillColor = FLinearColor(1.f, 0.7f, 0.5f);
		RimRotation = FRotator(-25, 170, 0); RimIntensity = 3.f;  RimColor = FLinearColor(1.f, 0.9f, 0.7f);
		SkyIntensity = 0.8f; break;
	case EIconForgeLightPreset::Flat:
		KeyRotation = FRotator(-10, 0, 0);   KeyIntensity = 3.f;  KeyColor = FLinearColor::White; bKeyShadows = false;
		FillRotation = FRotator(0, -90, 0);  FillIntensity = 1.f; FillColor = FLinearColor::White;
		RimRotation = FRotator(0, 180, 0);   RimIntensity = 0.f;  RimColor = FLinearColor::White;
		SkyIntensity = 3.f; break;
	}
}
