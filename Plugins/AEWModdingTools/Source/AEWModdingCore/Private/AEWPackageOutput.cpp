#include "AEWPackaging.h"
#include "AEWModdingSettings.h"
#include "FileUtilities/ZipArchiveWriter.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(Found, *StageDir, TEXT("pakchunk*"), true, false);
		FString PaksDir;
		FDateTime Newest = FDateTime::MinValue();
		for (const FString& File : Found)
		{
			const FDateTime T = IFileManager::Get().GetTimeStamp(*File);
			if (T > Newest)
			{
				Newest = T;
				PaksDir = FPaths::GetPath(File);
			}
		}
		if (PaksDir.IsEmpty())
		{
			Log.Add(TEXT("No pakchunk files found under ") + StageDir);
			return -1;
		}
		return ProcessPackagedChunks(PaksDir, ArchiveDir, Log);
	}

	int32 ProcessPackagedChunks(const FString& PaksFolder, const FString& PackageFolder, TArray<FString>& Log)
	{
		const UAEWModdingSettings* S = Settings();
		IFileManager& FM = IFileManager::Get();

		FString OutDir = S->PackagedModsFolder.Path.TrimStartAndEnd();
		if (OutDir.IsEmpty())
		{
			OutDir = (PackageFolder.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("PackagedMods") : PackageFolder / TEXT("Mods"));
		}
		OutDir = FPaths::ConvertRelativePathToFull(OutDir);
		FM.MakeDirectory(*OutDir, true);

		TArray<FString> Files;
		FM.FindFiles(Files, *(PaksFolder / TEXT("pakchunk*")), true, false);

		TMap<int32, const FAEWChunkOutput*> Names;
		for (const FAEWChunkOutput& Out : S->ChunkOutputs)
		{
			if (Out.ChunkId > 0 && !Out.Name.IsEmpty())
			{
				Names.Add(Out.ChunkId, &Out);
			}
		}

		TMap<FString, TArray<FString>> Zips;
		TSet<int32> Unnamed;
		TSet<int32> Done;
		for (const FString& File : Files)
		{
			int32 Chunk = 0;
			if (!ParseChunkFile(File, Chunk) || Chunk == 0)
			{
				continue;
			}
			const FAEWChunkOutput* const* Found = Names.Find(Chunk);
			if (!Found)
			{
				Unnamed.Add(Chunk);
				continue;
			}
			const FAEWChunkOutput& Out = **Found;
			const FString Base = Out.Name.EndsWith(TEXT("_P")) ? Out.Name : Out.Name + TEXT("_P");
			const FString Target = OutDir / Base + TEXT(".") + FPaths::GetExtension(File);
			if (FM.Copy(*Target, *(PaksFolder / File), true, true) != COPY_OK)
			{
				Log.Add(FString::Printf(TEXT("warning: could not copy %s to %s"), *File, *Target));
				continue;
			}
			Zips.FindOrAdd(Out.Zip.IsEmpty() ? Out.Name : Out.Zip).Add(Target);
			if (!Done.Contains(Chunk))
			{
				Done.Add(Chunk);
			}
		}

		TArray<FString> ZipNames;
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
						Log.Add(TEXT("warning: could not read ") + Member);
					}
				}
			}
			TArray<FString> Short;
			for (const FString& Member : Members)
			{
				Short.Add(FPaths::GetCleanFilename(Member));
			}
			Log.Add(FString::Printf(TEXT("%s.zip: %s"), *ZipName, *FString::Join(Short, TEXT(", "))));
		}

		TArray<int32> UnnamedList = Unnamed.Array();
		UnnamedList.Sort();
		if (UnnamedList.Num() > 0)
		{
			TArray<FString> Ids;
			for (int32 Id : UnnamedList)
			{
				Ids.Add(FString::Printf(TEXT("pakchunk%d"), Id));
			}
			Log.Add(TEXT("Left as they are (no name in Cook > Name Chunks): ") + FString::Join(Ids, TEXT(", ")));
		}
		Log.Insert(FString::Printf(TEXT("Packaged mods: %d chunk(s) renamed into %d zip(s) in %s"), Done.Num(), ZipNames.Num(), *OutDir), 0);
		FFileHelper::SaveStringArrayToFile(Log, *(OutDir / TEXT("PackagedMods.txt")));
		return Done.Num();
	}
}

#undef LOCTEXT_NAMESPACE
