#pragma once

#include "CoreMinimal.h"
#include "AcbTableRows.h"

namespace AcbImportBridge
{
	struct FRequest
	{
		FString AcbFile;
		FString PackagePath;
		bool bHasStreamingAwb = false;
		FString AwbDirectory;
		bool bAwbAlreadyCopied = false;
		bool bAssignSheetChunkToCues = true;
		bool bStageAwbForPackaging = true;
		bool bSaveAssets = true;
		TSet<FString> CueNames;
		bool bUpdateDataTables = true;
		bool bDeleteRemovedCueAssets = true;
		bool bDeleteRowsOfDeletedCues = true;
		TMap<FString, FString> RenamedCues;
		TArray<FString> DeletedCues;
		TArray<AcbTableRows::FPlannedRow> TableRows;
	};

	bool Import(const FRequest& Request, TArray<FString>& OutLog, FString& OutError);

	void UpdateDataTables(const FString& PackagePath, const TMap<FString, FString>& RenamedCues, const TArray<FString>& DeletedCues,
		bool bDeleteEmptyRows, TArray<UPackage*>& ToSave, TArray<FString>& OutLog);

	bool CopyAwbToProject(const FString& AwbFile, const FString& AwbDirectory, TArray<FString>& OutLog);

	bool SyncAwbPakRule(const FString& SheetPackageName, TArray<FString>& OutLog);
	int32 SyncAllAwbPakRules(TArray<FString>& OutLog);

	FString FindCueSheetFolder(const FString& CueSheetName);

	bool EnsureStagedForPackaging(const FString& ContentRelativeDir, TArray<FString>& OutLog);

	int32 FindPackageChunk(const FString& PackageName, TArray<FString>& OutLog);

	bool EnsureAwbPakRule(const FString& ContentRelativeDir, const FString& SheetName, const FString& PakName, TArray<FString>& OutLog);
}
