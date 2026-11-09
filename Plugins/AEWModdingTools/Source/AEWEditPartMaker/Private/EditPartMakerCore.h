#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UDataTable;
class USkeletalMesh;

DECLARE_LOG_CATEGORY_EXTERN(LogAEWEditPartMaker, Log, All);

struct FPartRequest
{
	TWeakObjectPtr<USkeletalMesh> Mesh;
	FString RowName;
	FString Label;
	FString Slot;
	FString Gender;
	FString FaceType;
	FString Attire;
	FString WACategory;
	FString CompatibleCategory = TEXT("---");
	FString ThumbnailPath;
	FString WAName;
	int32 PartsID = 0;
	int32 UnlockID = 0;
	bool bOverwrite = false;
};

struct FEditPartMakerTables
{
	UDataTable* Parts = nullptr;
	UDataTable* PartsList = nullptr;
	UDataTable* Unlock = nullptr;
	UDataTable* BaseUnlock = nullptr;
	UDataTable* Category = nullptr;

	bool IsUsable() const { return Parts && Unlock; }
};

namespace EditPartMaker
{
	FEditPartMakerTables LoadTables(TArray<FString>& OutErrors);

	TArray<FString> GetEnumOptions(const UDataTable* PartsTable, const TCHAR* FieldName);
	TArray<FString> GetCategoryOptions(const UDataTable* CategoryTable);

	void SuggestIds(const FEditPartMakerTables& Tables, const FString& IgnoreRow, int32& OutPartsID, int32& OutUnlockID);

	void FillDefaultsFromMesh(USkeletalMesh* Mesh, FPartRequest& Request);

	void Validate(const FEditPartMakerTables& Tables, const FPartRequest& Request, TArray<FString>& OutErrors, TArray<FString>& OutWarnings);

	bool MakePart(const FEditPartMakerTables& Tables, const FPartRequest& Request, TArray<FString>& OutLog);
}
