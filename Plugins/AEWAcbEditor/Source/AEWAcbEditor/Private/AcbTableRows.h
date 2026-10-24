#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class UPackage;
class UDataTable;

namespace AcbTableRows
{
	struct FFieldInfo
	{
		FName Field;
		FString Label;
		FString TemplateValue;
	};

	struct FPlannedRow
	{
		FString TableName;
		FSoftObjectPath Table;
		FSoftObjectPath NewTable;
		int32 FirstID = 0;
		int32 LastID = 0;
		FName TemplateRow;
		bool bEnabled = true;
		int32 CueFields = 0;
		int32 CoveredCueFields = 0;
		TArray<FString> TemplateCues;
		TMap<FString, FString> CueMap;
		FString From;
		FString To;
		FString RowName;
		TMap<FName, FString> Values;
		TArray<FFieldInfo> Fields;
		FName UnlockField;
		FName UnlockTemplateRow;
		bool bAddUnlock = true;
		FSoftObjectPath UnlockNewTable;
		int32 FirstUnlockID = 0;
		int32 LastUnlockID = 0;
	};

	TArray<FPlannedRow> Find(const TArray<FString>& TemplateCues, TArray<FString>& OutLog);

	void SetCues(FPlannedRow& Row, const TMap<FString, FString>& CueMap);

	void Merge(TArray<FPlannedRow>& Pending, const FPlannedRow& Added);

	void RenameCue(TArray<FPlannedRow>& Pending, const FString& OldName, const FString& NewName);

	int32 Apply(const FString& CueFolder, const TArray<FPlannedRow>& Rows, TArray<UPackage*>& ToSave, TArray<FString>& OutLog);

	void RefreshComposites(const TArray<UPackage*>& Changed);

	void GetSourceTables(const UDataTable* Table, TArray<const UDataTable*>& OutTables);

	FString MatchCase(const FString& Original, const FString& Replacement);

	FString Substitute(const FString& Text, const FString& From, const FString& To);
}
