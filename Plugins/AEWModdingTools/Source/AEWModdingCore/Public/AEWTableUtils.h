#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "Framework/Commands/UIAction.h"
#include "ToolMenuDelegates.h"

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
	struct FSection
	{
		FName Name;
		FText Label;
		int32 Order = 0;
	};

	AEWMODDINGCORE_API const FSection& ToolsSection();

	AEWMODDINGCORE_API const FSection& ArenaSection();

	AEWMODDINGCORE_API const FSection& PackagingSection();

	AEWMODDINGCORE_API FName MenuName(const FString& Menu);

	AEWMODDINGCORE_API void AddEntry(const FString& Menu, const FSection& Section, int32 Order, FName EntryName, const FText& Label, const FText& ToolTip, const FUIAction& Action);

	AEWMODDINGCORE_API void AddSubMenu(const FString& Menu, const FSection& Section, int32 Order, FName EntryName, const FText& Label, const FText& ToolTip);

	AEWMODDINGCORE_API void AddDynamicSubMenu(const FString& Menu, const FSection& Section, int32 Order, FName EntryName, const FText& Label, const FText& ToolTip, const FNewToolMenuDelegate& Fill);
}
