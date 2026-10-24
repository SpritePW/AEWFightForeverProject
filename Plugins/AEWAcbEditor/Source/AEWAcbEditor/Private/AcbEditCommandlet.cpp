#include "AcbEditCommandlet.h"
#include "AcbDocument.h"
#include "AcbEditorSession.h"
#include "AcbEditorSettings.h"
#include "AcbImportBridge.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

UAcbEditCommandlet::UAcbEditCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UAcbEditCommandlet::Main(const FString& Params)
{
	UAcbEditorSettings* Settings = GetMutableDefault<UAcbEditorSettings>();

	FString Acb;
	FString Awb;
	FString CueSheetFolder;
	FString Tracks;
	FString Category;
	FString PresetName;
	FString TemplateCue;
	FString Out;
	FParse::Value(*Params, TEXT("Acb="), Acb);
	FParse::Value(*Params, TEXT("Awb="), Awb);
	FParse::Value(*Params, TEXT("CueSheetFolder="), CueSheetFolder);
	FParse::Value(*Params, TEXT("Tracks="), Tracks);
	FParse::Value(*Params, TEXT("Category="), Category);
	FParse::Value(*Params, TEXT("Preset="), PresetName);
	FParse::Value(*Params, TEXT("Template="), TemplateCue);
	if (FParse::Value(*Params, TEXT("Out="), Out))
	{
		Settings->OutputDirectory.Path = Out;
	}
	const bool bLoop = !FParse::Param(*Params, TEXT("NoLoop"));
	const bool bImport = FParse::Param(*Params, TEXT("Import"));

	if (Acb.IsEmpty())
	{
		UE_LOG(LogAEWAcbEditor, Error, TEXT("No ACB: pass -Acb=<file.acb|.uasset>."));
		return 1;
	}
	if (Awb.IsEmpty())
	{
		Awb = FAcbEditorSession::FindSiblingAwb(Acb);
	}

	FAcbEditorSession Session;
	FString Error;
	if (!Session.Open(Acb, Awb, Error))
	{
		UE_LOG(LogAEWAcbEditor, Error, TEXT("%s"), *Error);
		return 1;
	}
	UE_LOG(LogAEWAcbEditor, Display, TEXT("%s"), *Session.Describe());

	TArray<FAcbCueTemplate> Cues;
	if (!PresetName.IsEmpty())
	{
		for (const FAcbCuePreset& Preset : Settings->GetPresetsFor(Session.GetCueSheetName()))
		{
			if (Preset.Name.Equals(PresetName, ESearchCase::IgnoreCase))
			{
				Cues = Preset.Cues;
			}
		}
		if (Cues.Num() == 0)
		{
			UE_LOG(LogAEWAcbEditor, Error, TEXT("No cue preset named '%s' for cue sheet %s (built-in or in the plugin settings)."), *PresetName, *Session.GetCueSheetName());
			return 1;
		}
	}
	else
	{
		FAcbCueTemplate Single;
		Single.Category = Category;
		Cues.Add(Single);
	}
	int32 Template = INDEX_NONE;
	if (!TemplateCue.IsEmpty())
	{
		Template = Session.FindCue(TemplateCue);
		if (Template == INDEX_NONE)
		{
			UE_LOG(LogAEWAcbEditor, Error, TEXT("No cue named '%s' (-Template)."), *TemplateCue);
			return 1;
		}
	}

	int32 Failed = 0;
	if (!Tracks.IsEmpty())
	{
		TArray<FString> Files;
		for (const TCHAR* Extension : { TEXT("*.hca"), TEXT("*.adx"), TEXT("*.wav") })
		{
			TArray<FString> Found;
			IFileManager::Get().FindFiles(Found, *(Tracks / Extension), true, false);
			for (const FString& File : Found)
			{
				Files.Add(Tracks / File);
			}
		}
		Files.Sort();
		for (const FString& File : Files)
		{
			FString Message;
			const FString Base = FAcbEditorSession::FromUtf8(AcbCore::AcbDocument::SanitizeCueName(FAcbEditorSession::ToUtf8(FPaths::GetBaseFilename(File))));
			TArray<FAcbCueTemplate> FileCues = Cues;
			for (FAcbCueTemplate& Cue : FileCues)
			{
				Cue.Pattern = FAcbEditorSession::ResolveCueName(Cue.Pattern, Base);
			}
			if (Session.AddTrack(File, FileCues, Template, bLoop, Message))
			{
				UE_LOG(LogAEWAcbEditor, Display, TEXT("%s"), *Message);
			}
			else
			{
				UE_LOG(LogAEWAcbEditor, Error, TEXT("%s"), *Message);
				++Failed;
			}
		}
	}

	const FAcbSaveResult Result = Session.Save(*Settings);
	if (!Result.bOk)
	{
		UE_LOG(LogAEWAcbEditor, Error, TEXT("Save failed: %s"), *Result.Error);
		return 1;
	}
	const FString Written = Result.AwbPath.IsEmpty() ? Result.AcbPath : Result.AcbPath + TEXT(" + ") + Result.AwbPath;
	if (!Result.Cleanup.IsEmpty())
	{
		UE_LOG(LogAEWAcbEditor, Display, TEXT("%s"), *Result.Cleanup);
	}
	UE_LOG(LogAEWAcbEditor, Display, TEXT("Saved %s"), *Written);

	bool bAwbCopied = false;
	if (!Result.AwbPath.IsEmpty() && Settings->bCopyAwbToProject)
	{
		TArray<FString> CopyLog;
		bAwbCopied = AcbImportBridge::CopyAwbToProject(Result.AwbPath, Settings->AwbDirectory, CopyLog);
		for (const FString& Line : CopyLog)
		{
			UE_LOG(LogAEWAcbEditor, Display, TEXT("%s"), *Line);
		}
	}

	if (bImport)
	{
		AcbImportBridge::FRequest Request;
		Request.AcbFile = Result.AcbPath;
		if (CueSheetFolder.IsEmpty())
		{
			CueSheetFolder = AcbImportBridge::FindCueSheetFolder(Session.GetCueSheetName());
		}
		if (CueSheetFolder.IsEmpty())
		{
			UE_LOG(LogAEWAcbEditor, Error, TEXT("No cue sheet asset named %s in the project: pass -CueSheetFolder=/Game/..."), *Session.GetCueSheetName());
			return 1;
		}
		Request.PackagePath = CueSheetFolder;
		Request.bHasStreamingAwb = !Result.AwbPath.IsEmpty();
		Request.AwbDirectory = Settings->AwbDirectory;
		Request.bStageAwbForPackaging = Settings->bAddAwbFolderToPackaging;
		Request.bSaveAssets = true;
		Request.bAwbAlreadyCopied = bAwbCopied;
		Request.bAssignSheetChunkToCues = Settings->bAssignSheetChunkToCues;
		for (const TSharedPtr<FAcbCueRow>& Row : Session.BuildRows())
		{
			Request.CueNames.Add(Row->Name);
		}
		TArray<FString> Log;
		const bool bImported = AcbImportBridge::Import(Request, Log, Error);
		for (const FString& Line : Log)
		{
			UE_LOG(LogAEWAcbEditor, Display, TEXT("%s"), *Line);
		}
		if (!bImported)
		{
			UE_LOG(LogAEWAcbEditor, Error, TEXT("Import failed: %s"), *Error);
			return 1;
		}
	}
	return Failed > 0 ? 2 : 0;
}
