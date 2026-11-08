#include "AEWPackaging.h"
#include "AEWModdingSettings.h"
#include "AssetRegistryModule.h"
#include "Engine/PrimaryAssetLabel.h"
#include "FileUtilities/ZipArchiveWriter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "DerivedDataCacheInterface.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "EditorStyleSet.h"
#include "FileHelpers.h"
#include "IUATHelperModule.h"
#include "UnrealEdMisc.h"
#include "Editor.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "AEWPackageOutput"

namespace
{
	FString CleanName(const FString& In)
	{
		FString Out;
		for (const TCHAR C : In.TrimStartAndEnd())
		{
			Out.AppendChar(FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') || C == TEXT('.') ? C : TEXT('_'));
		}
		return Out;
	}

	bool ParseChunkFile(const FString& FileName, int32& OutChunk)
	{
		if (!FileName.StartsWith(TEXT("pakchunk")))
		{
			return false;
		}
		int32 Index = 8;
		FString Digits;
		while (Index < FileName.Len() && FChar::IsDigit(FileName[Index]))
		{
			Digits.AppendChar(FileName[Index++]);
		}
		if (Digits.IsEmpty() || Index >= FileName.Len() || FileName[Index] != TEXT('-'))
		{
			return false;
		}
		OutChunk = FCString::Atoi(*Digits);
		return true;
	}
}

namespace AEWPackaging
{
	FString CleanModName(const FString& In)
	{
		return CleanName(In);
	}

	FString UatLogPath()
	{
		FString Engine = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
		Engine.RemoveFromEnd(TEXT("/"));
		Engine.ReplaceInline(TEXT(":"), TEXT(""));
		Engine.ReplaceInline(TEXT("/"), TEXT("+"));
		Engine.ReplaceInline(TEXT("\\"), TEXT("+"));
		Engine.ReplaceInline(TEXT(" "), TEXT("+"));
		return FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA")) / TEXT("Unreal Engine/AutomationTool/Logs") / Engine / TEXT("Log.txt");
	}

	static FString Tail(const FString& Path, int64 Bytes)
	{
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path, FILEREAD_AllowWrite));
		if (!Reader)
		{
			return FString();
		}
		const int64 Size = Reader->TotalSize();
		const int64 Start = FMath::Max<int64>(0, Size - Bytes);
		TArray<uint8> Data;
		Data.SetNumUninitialized(Size - Start);
		Reader->Seek(Start);
		Reader->Serialize(Data.GetData(), Data.Num());
		Data.Add(0);
		return FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Data.GetData())));
	}

	static FString ValueAfter(const FString& Text, const FString& Key)
	{
		const int32 At = Text.Find(Key);
		if (At == INDEX_NONE)
		{
			return FString();
		}
		FString Rest = Text.Mid(At + Key.Len());
		int32 End = INDEX_NONE;
		if (Rest.FindChar(TEXT('\n'), End))
		{
			Rest.LeftInline(End);
		}
		return Rest.TrimStartAndEnd();
	}

	bool UatLogShowsSuccess(const FString& Path)
	{
		return Tail(Path, 400).Contains(TEXT("ExitCode=0 (Success)"));
	}

	FString OutputFolderFor(const FString& PackageFolder)
	{
		FString OutDir = Settings()->PackagedModsFolder.Path.TrimStartAndEnd();
		if (OutDir.IsEmpty())
		{
			OutDir = PackageFolder.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("PackagedMods") : PackageFolder / TEXT("Mods");
		}
		return FPaths::ConvertRelativePathToFull(OutDir);
	}

	static FString NewestPaksFolder(const FString& Root, const FString& Skip)
	{
		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(Found, *Root, TEXT("pakchunk*"), true, false);
		FString PaksDir;
		FDateTime Newest = FDateTime::MinValue();
		const FString SkipDir = FPaths::ConvertRelativePathToFull(Skip);
		for (const FString& File : Found)
		{
			int32 Chunk = 0;
			const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::GetPath(File));
			if (!ParseChunkFile(FPaths::GetCleanFilename(File), Chunk) || (!SkipDir.IsEmpty() && FPaths::IsUnderDirectory(Dir, SkipDir)))
			{
				continue;
			}
			const FDateTime T = IFileManager::Get().GetTimeStamp(*File);
			if (T > Newest)
			{
				Newest = T;
				PaksDir = Dir;
			}
		}
		return PaksDir;
	}

	int32 ProcessLastPackage(TArray<FString>& Log)
	{
		const FString UatLog = UatLogPath();
		if (!UatLogShowsSuccess(UatLog))
		{
			Log.Add(TEXT("No successful packaging run found in ") + UatLog);
			return -1;
		}
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *UatLog, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
		{
			Log.Add(TEXT("Could not read ") + UatLog);
			return -1;
		}
		FString FirstLine = Text.Left(4000);
		FirstLine.ReplaceInline(TEXT("\\"), TEXT("/"));
		const FString Project = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		if (!FirstLine.Contains(TEXT("BuildCookRun")) || !FirstLine.Contains(Project, ESearchCase::IgnoreCase))
		{
			Log.Add(TEXT("The last packaging run was not Package Project for this project."));
			return -1;
		}
		FString StageDir = ValueAfter(Text, TEXT("BaseStageDirectory="));
		const FString ArchiveDir = ValueAfter(Text, TEXT("BaseArchiveDirectory="));
		if (StageDir.IsEmpty())
		{
			StageDir = FPaths::ProjectSavedDir() / TEXT("StagedBuilds");
		}
		const FString OutDir = OutputFolderFor(ArchiveDir);
		FString PaksDir = ArchiveDir.IsEmpty() ? FString() : NewestPaksFolder(ArchiveDir, OutDir);
		const bool bMove = !PaksDir.IsEmpty();
		if (!bMove)
		{
			PaksDir = NewestPaksFolder(StageDir, OutDir);
		}
		if (PaksDir.IsEmpty())
		{
			Log.Add(TEXT("No pakchunk files found under ") + (ArchiveDir.IsEmpty() ? StageDir : ArchiveDir));
			return -1;
		}
		return ProcessPackagedChunks(PaksDir, ArchiveDir, Log, bMove);
	}

	static TSet<int32> ChunksNotCooked()
	{
		TSet<int32> Labelled;
		TSet<int32> Cooked;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Labels;
		Registry.GetAssetsByClass(UPrimaryAssetLabel::StaticClass()->GetFName(), Labels, true);
		for (const FAssetData& A : Labels)
		{
			const UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset());
			if (!Label || Label->Rules.ChunkId <= 0)
			{
				continue;
			}
			Labelled.Add(Label->Rules.ChunkId);
			if (Label->Rules.CookRule != EPrimaryAssetCookRule::NeverCook && Label->Rules.CookRule != EPrimaryAssetCookRule::DevelopmentCook)
			{
				Cooked.Add(Label->Rules.ChunkId);
			}
		}
		return Labelled.Difference(Cooked);
	}

	int32 ProcessPackagedChunks(const FString& PaksFolder, const FString& PackageFolder, TArray<FString>& Log, bool bMove)
	{
		const UAEWModdingSettings* S = Settings();
		IFileManager& FM = IFileManager::Get();
		const bool bRename = S->bRenamePackagedChunks;
		const bool bArchive = S->bArchivePackagedChunks;

		const FString OutDir = OutputFolderFor(PackageFolder);
		FM.MakeDirectory(*OutDir, true);

		TArray<FString> Files;
		FM.FindFiles(Files, *(PaksFolder / TEXT("pakchunk*")), true, false);

		TMap<int32, const FAEWChunkOutput*> Names;
		for (const FAEWChunkOutput& Out : S->ChunkOutputs)
		{
			if (Out.ChunkId > 0 && (!Out.Name.IsEmpty() || !Out.Zip.IsEmpty()))
			{
				Names.Add(Out.ChunkId, &Out);
			}
		}

		const TSet<int32> Skip = ChunksNotCooked();
		TSet<int32> Skipped;

		TMap<FString, TArray<FString>> Zips;
		TArray<FString> Loose;
		TSet<int32> Done;
		int32 Removed = 0;
		for (const FString& File : Files)
		{
			int32 Chunk = 0;
			if (!ParseChunkFile(File, Chunk) || Chunk == 0)
			{
				continue;
			}
			if (Skip.Contains(Chunk))
			{
				Skipped.Add(Chunk);
				continue;
			}
			const FAEWChunkOutput* const* Found = Names.Find(Chunk);
			const FString Name = bRename && Found ? (*Found)->Name : FString();
			const FString ZipName = Found && !(*Found)->Zip.IsEmpty() ? (*Found)->Zip
				: (Found && !(*Found)->Name.IsEmpty() ? (*Found)->Name : FString::Printf(TEXT("pakchunk%d"), Chunk));
			const FString Base = Name.IsEmpty() ? FPaths::GetBaseFilename(File) : (Name.EndsWith(TEXT("_P")) ? Name : Name + TEXT("_P"));
			const FString Source = PaksFolder / File;
			const FString Target = OutDir / Base + TEXT(".") + FPaths::GetExtension(File);
			FM.Delete(*Target, false, true, true);
			const bool bOk = bMove ? FM.Move(*Target, *Source, true, true) : FM.Copy(*Target, *Source, true, true) == COPY_OK;
			if (!bOk)
			{
				Log.Add(FString::Printf(TEXT("warning: could not %s %s to %s"), bMove ? TEXT("move") : TEXT("copy"), *File, *Target));
				continue;
			}
			if (bMove)
			{
				++Removed;
			}
			Zips.FindOrAdd(ZipName).Add(Target);
			Loose.Add(Target);
			Done.Add(Chunk);
		}

		TArray<FString> ZipNames;
		if (bArchive)
		{
			Zips.GetKeys(ZipNames);
			ZipNames.Sort();
			for (const FString& ZipName : ZipNames)
			{
				TArray<FString>& Members = Zips[ZipName];
				Members.Sort();
				const FString ZipPath = OutDir / ZipName + TEXT(".zip");
				FM.Delete(*ZipPath, false, true, true);
				IFileHandle* Handle = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*ZipPath);
				if (!Handle)
				{
					Log.Add(TEXT("warning: could not write ") + ZipPath);
					continue;
				}
				bool bAll = true;
				{
					FZipArchiveWriter Zip(Handle);
					for (const FString& Member : Members)
					{
						TArray<uint8> Data;
						if (FFileHelper::LoadFileToArray(Data, *Member))
						{
							Zip.AddFile(FPaths::GetCleanFilename(Member), Data, FM.GetTimeStamp(*Member));
						}
						else
						{
							bAll = false;
							Log.Add(TEXT("warning: could not read ") + Member);
						}
					}
				}
				TArray<FString> Short;
				for (const FString& Member : Members)
				{
					Short.Add(FPaths::GetCleanFilename(Member));
					if (bAll)
					{
						FM.Delete(*Member, false, true, true);
						Loose.Remove(Member);
					}
				}
				Log.Add(FString::Printf(TEXT("%s.zip: %s"), *ZipName, *FString::Join(Short, TEXT(", "))));
			}
		}
		for (const FString& File : Loose)
		{
			Log.Add(TEXT("File: ") + FPaths::GetCleanFilename(File));
		}
		if (Removed > 0)
		{
			Log.Add(FString::Printf(TEXT("Moved %d file(s) out of %s"), Removed, *PaksFolder));
		}
		if (Skipped.Num() > 0)
		{
			TArray<int32> List = Skipped.Array();
			List.Sort();
			TArray<FString> Ids;
			for (int32 Id : List)
			{
				Ids.Add(FString::FromInt(Id));
			}
			Log.Add(TEXT("Left in place (not cooked): chunk ") + FString::Join(Ids, TEXT(", ")));
		}
		Log.Insert(bArchive
			? FString::Printf(TEXT("Packaged mods: %d chunk(s) into %d archive(s) in %s"), Done.Num(), ZipNames.Num(), *OutDir)
			: FString::Printf(TEXT("Packaged mods: %d chunk(s) in %s"), Done.Num(), *OutDir), 0);
		FFileHelper::SaveStringArrayToFile(Log, *(OutDir / TEXT("PackagedMods.txt")));
		return Done.Num();
	}

	bool PackageProject()
	{
		const FText Title = LOCTEXT("PackageTitle", "Package Project");
		if (GEditor && GEditor->PlayWorld)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PackagePIE", "Stop Play In Editor first."), &Title);
			return false;
		}
		if (!FEditorFileUtils::SaveDirtyPackages(true, true, true, false, false, false))
		{
			return false;
		}

		UProjectPackagingSettings* PS = GetMutableDefault<UProjectPackagingSettings>();
		if (PS->StagingDirectory.Path.IsEmpty())
		{
			PS->StagingDirectory.Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		}
		IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
		if (!Desktop)
		{
			return false;
		}
		const void* Parent = nullptr;
		if (FSlateApplication::IsInitialized())
		{
			Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		}
		FString Folder;
		if (!Desktop->OpenDirectoryDialog(Parent, LOCTEXT("PackageFolder", "Package project...").ToString(), PS->StagingDirectory.Path, Folder))
		{
			return false;
		}
		PS->StagingDirectory.Path = Folder;
		PS->SaveConfig();

		const UProjectPackagingSettings::FConfigurationInfo& Config = UProjectPackagingSettings::ConfigurationInfo[PS->BuildConfiguration];
		FString Options;
		if (PS->FullRebuild)
		{
			Options += TEXT(" -clean");
		}
		if (PS->bCompressed)
		{
			Options += TEXT(" -compressed");
		}
		if (PS->bSkipEditorContent)
		{
			Options += TEXT(" -SkipCookingEditorContent");
		}
		if (FDerivedDataCacheInterface* DDC = GetDerivedDataCache())
		{
			Options += FString::Printf(TEXT(" -ddc=%s"), DDC->GetGraphName());
		}
		if (PS->bUseIoStore)
		{
			Options += TEXT(" -iostore");
			PS->UsePakFile = true;
		}
		if (PS->UsePakFile)
		{
			Options += TEXT(" -pak");
		}
		if (PS->bUseIoStore)
		{
			Options += TEXT(" -iostore");
		}
		if (PS->bMakeBinaryConfig)
		{
			Options += TEXT(" -makebinaryconfig");
		}
		if (PS->IncludePrerequisites)
		{
			Options += TEXT(" -prereqs");
		}
		if (!PS->ApplocalPrerequisitesDirectory.Path.IsEmpty())
		{
			Options += FString::Printf(TEXT(" -applocaldirectory=\"%s\""), *PS->ApplocalPrerequisitesDirectory.Path);
		}
		else if (PS->IncludeAppLocalPrerequisites)
		{
			Options += TEXT(" -applocaldirectory=\"$(EngineDir)/Binaries/ThirdParty/AppLocalDependencies\"");
		}
		if (PS->ForDistribution)
		{
			Options += TEXT(" -distribution");
		}
		if (!PS->IncludeDebugFiles)
		{
			Options += TEXT(" -nodebuginfo");
		}
		if (PS->bGenerateChunks)
		{
			Options += TEXT(" -manifests");
		}
		Options += TEXT(" -targetplatform=Win64");
		const bool bBuild = PS->Build == EProjectPackagingBuild::Always || PS->Build == EProjectPackagingBuild::IfProjectHasCode
			|| (PS->Build == EProjectPackagingBuild::IfEditorWasBuiltLocally && !FApp::GetEngineIsPromotedBuild());
		if (bBuild)
		{
			Options += TEXT(" -build");
		}
		if (PS->IncludeCrashReporter)
		{
			Options += TEXT(" -CrashReporter");
		}
		const int32 Cookers = GetDefault<UEditorExperimentalSettings>()->MultiProcessCooking;
		if (Cookers > 0)
		{
			Options += FString::Printf(TEXT(" -NumCookersToSpawn=%d"), Cookers);
		}
		const FTargetInfo* Target = PS->GetBuildTargetInfo();
		if (!Target)
		{
			Options += FString::Printf(TEXT(" -clientconfig=%s"), LexToString(Config.Configuration));
		}
		else if (Target->Type == EBuildTargetType::Server)
		{
			Options += FString::Printf(TEXT(" -target=%s -serverconfig=%s"), *Target->Name, LexToString(Config.Configuration));
		}
		else
		{
			Options += FString::Printf(TEXT(" -target=%s -clientconfig=%s"), *Target->Name, LexToString(Config.Configuration));
		}

		const FString Project = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		const FString CommandLine = FString::Printf(TEXT("-ScriptsForProject=\"%s\" BuildCookRun -nocompileeditor%s -nop4 -project=\"%s\" -cook -stage -archive -archivedirectory=\"%s\" -package -ue4exe=\"%s\" %s -utf8output"),
			*Project,
			FApp::IsEngineInstalled() ? TEXT(" -installed") : TEXT(""),
			*Project,
			*PS->StagingDirectory.Path,
			*FUnrealEdMisc::Get().GetExecutableForCommandlets(),
			*Options);

		IUATHelperModule::Get().CreateUatTask(CommandLine, LOCTEXT("Windows64", "Windows (64-bit)"), LOCTEXT("PackagingProjectTaskName", "Packaging project"), LOCTEXT("PackagingTaskName", "Packaging"), FEditorStyle::GetBrush(TEXT("MainFrame.PackageProject")));
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
