/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2015-2017 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Asset Type Action of Atom Cue Sheet
 * File     : AssetTypeActions_SoundAtomCueSheet.cpp
 *
 ****************************************************************************/

#include "AssetTypeActions_SoundAtomCueSheet.h"

#include "CriWareEditorPrivatePCH.h"
#include "CriWareEditorStyle.h"
#include "AtomSoundData.h"
#include "SoundAtomCueSheet.h"
#include "SoundAtomCueSheetFactory.h"

#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_SoundAtomCueSheet::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto SoundAtomCueSheet = GetTypedWeakObjectPtrs<USoundAtomCueSheet>(InObjects);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("SoundAtomCueSheet_RebuildAndReimport", "Rebuild And Reimport"),
		LOCTEXT("SoundAtomCueSheet_RebuildAndReimportTooltip", "Rebuild the Sound Atom Cue Sheet and then Reimport it."),
		FSlateIcon(FCriWareEditorStyle::GetAppStyleSetName(), "ClassIcon.SoundAtomCueSheet"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_SoundAtomCueSheet::ExecuteRebuildAndReimport, SoundAtomCueSheet),
			FCanExecuteAction()
		)
	);

	TSharedRef<SWidget> TextBoxWidget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(80)
			.Text(this, &FAssetTypeActions_SoundAtomCueSheet::GetWorkUnitName)
			.OnTextCommitted(this, &FAssetTypeActions_SoundAtomCueSheet::OnTextCommited)
		];

	MenuBuilder.AddWidget(TextBoxWidget, FText::FromString(""));
}

void FAssetTypeActions_SoundAtomCueSheet::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets) {
		const USoundAtomCueSheet* CueSheet = CastChecked<USoundAtomCueSheet>(Asset);
		if (CueSheet->AssetImportData) {
			CueSheet->AssetImportData->ExtractFilenames(OutSourceFilePaths);
		}
	}
}

void FAssetTypeActions_SoundAtomCueSheet::ExecuteRebuildAndReimport(TArray<TWeakObjectPtr<USoundAtomCueSheet>> SoundAtomCueSheets)
{
	if (!IsValid(GEngine))
	{
		return;
	}

	// Make sure the Python Editor Script Plugin is enabled
	if (!FModuleManager::Get().IsModuleLoaded("PythonScriptPlugin"))
	{
		UE_LOG(LogCriWareEditor, Error, TEXT("The Python Editor Script plugin is disabled."));
		return;
	}

	// Set command line arguments
	const auto NonAssetFolderPath = "";
	//NonAssetFolderPath = " -NonAssetFolder " + FPaths::ProjectContentDir() + "CRIWARE";
	const auto WorkUnit = (GetWorkUnitName().IsEmpty()) ? FString() : " -WorkUnitName " + GetWorkUnitName().ToString();
	
	// Path from the content directory to the Python file
	const auto PythonFilePath = "Runtime/CriWare/CriWare/Content/Python/criware_cuesheet_rebuild_and_reimport.py";

	//Execution of Python commands
	auto ExecutePythonCommand = [&](const FString& PluginsDir)->void
	{
		const auto PythonCommand = "py " + PluginsDir + PythonFilePath + NonAssetFolderPath + WorkUnit;
		auto Result = GEngine->Exec(nullptr, *PythonCommand);
		if (!Result)
		{
			UE_LOG(LogCriWareEditor, Error, TEXT("Failed to execute Python command."));
		}
	};

	// Check if you are using the CRIWARE plugin as the project plugin
	if (FPaths::FileExists(FPaths::ProjectPluginsDir() + PythonFilePath))
	{
		ExecutePythonCommand(FPaths::ProjectPluginsDir());
	}
	// Check if you are using the CRIWARE plugin as the engine plugin
	else if (FPaths::FileExists(FPaths::EnginePluginsDir() + PythonFilePath))
	{
		ExecutePythonCommand(FPaths::EnginePluginsDir());
	}
	else
	{
		UE_LOG(LogCriWareEditor, Error, TEXT("File does not exist.(%s)"), *PythonFilePath);
	}
}

bool FAssetTypeActions_SoundAtomCueSheet::CanExecuteReimport(const TArray<TWeakObjectPtr<USoundAtomCueSheet>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt) {
		auto Object = (*ObjIt).Get();
		if (Object) {
			return true;
		}
	}

	return false;
}

void FAssetTypeActions_SoundAtomCueSheet::OnTextCommited(const FText& InText, ETextCommit::Type CommitInfo)
{
	WorkUnitName = InText.ToString();
}

#undef LOCTEXT_NAMESPACE

