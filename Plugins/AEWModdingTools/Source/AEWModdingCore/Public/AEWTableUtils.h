#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

class UDataTable;
class UEnum;
class UScriptStruct;

namespace AEWTables
{
	AEWMODDINGCORE_API UDataTable* Load(const FSoftObjectPath& Path);

	AEWMODDINGCORE_API FProperty* FindField(const UStruct* Struct, const FString& Name);

	AEWMODDINGCORE_API bool SetByText(const UStruct* Struct, void* Container, const FString& Name, const FString& Text, FString& OutError);
	AEWMODDINGCORE_API bool SetByText(const UStruct* Struct, void* Container, const FString& Name, const FString& Text);
	AEWMODDINGCORE_API FString GetAsText(const UStruct* Struct, const void* Container, const FString& Name);

	AEWMODDINGCORE_API UEnum* GetEnum(FProperty* Property, FNumericProperty*& OutUnderlying);
	AEWMODDINGCORE_API bool GetInt(const UStruct* Struct, const void* Container, const FString& Name, int64& Out);
	AEWMODDINGCORE_API bool SetInt(const UStruct* Struct, void* Container, const FString& Name, int64 Value);

	AEWMODDINGCORE_API TArray<UDataTable*> GetParents(const UDataTable* Table);
	AEWMODDINGCORE_API void GetSourceTables(const UDataTable* Table, TArray<const UDataTable*>& Out);
	AEWMODDINGCORE_API void RefreshComposite(UDataTable* Table);

	AEWMODDINGCORE_API void MakeUnlocked(const UScriptStruct* Struct, void* Row);
	AEWMODDINGCORE_API FName UnlockRowName(int64 UnlockId);
	AEWMODDINGCORE_API TSet<int64> CollectInts(const TArray<const UDataTable*>& Tables, const FString& Field);
	AEWMODDINGCORE_API int64 FirstFree(const TSet<int64>& Used, int64 First, int64 Last);

	AEWMODDINGCORE_API FString Escape(const FString& In);
	AEWMODDINGCORE_API FString InvText(const FString& In);

	struct AEWMODDINGCORE_API FRowBuffer
	{
		const UScriptStruct* Struct;
		uint8* Memory;

		explicit FRowBuffer(const UScriptStruct* InStruct, const void* Source = nullptr);
		~FRowBuffer();
		FRowBuffer(const FRowBuffer&) = delete;
		FRowBuffer& operator=(const FRowBuffer&) = delete;

		const struct FTableRowBase& AsRow() const;
	};

	AEWMODDINGCORE_API void ReplaceRow(UDataTable* Table, FName RowName, const FRowBuffer& Row);
}

namespace AEWMenus
{
	AEWMODDINGCORE_API void AddToolEntry(FName EntryName, const FText& Label, const FText& ToolTip, const struct FUIAction& Action);

	AEWMODDINGCORE_API void AddGroupedToolEntry(FName Group, const FText& GroupLabel, FName EntryName, const FText& Label, const FText& ToolTip, const struct FUIAction& Action);

	AEWMODDINGCORE_API void AddSubGroupedToolEntry(FName Group, const FText& GroupLabel, FName SubGroup, const FText& SubGroupLabel, FName EntryName, const FText& Label, const FText& ToolTip, const struct FUIAction& Action);
}
