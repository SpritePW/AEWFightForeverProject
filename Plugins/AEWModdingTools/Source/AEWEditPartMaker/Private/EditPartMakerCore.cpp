#include "EditPartMakerCore.h"
#include "EditPartMakerSettings.h"
#include "AEWTableUtils.h"
#include "AEWPackaging.h"
#include "AEWModdingSettings.h"

#include "AssetRegistryModule.h"
#include "DataTableEditorUtils.h"
#include "Engine/CompositeDataTable.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "FileHelpers.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include <initializer_list>

DEFINE_LOG_CATEGORY(LogAEWEditPartMaker);

#define LOCTEXT_NAMESPACE "AEWEditPartMaker"

namespace
{
	using AEWTables::FindField;
	using AEWTables::SetByText;
	using AEWTables::GetAsText;
	using AEWTables::GetInt;

	FString ShortEnum(const FString& Value)
	{
		int32 Index = INDEX_NONE;
		return Value.FindLastChar(TEXT(':'), Index) ? Value.Mid(Index + 1) : Value;
	}

	FString Quote(const FString& In)
	{
		return AEWTables::Escape(In);
	}

	UDataTable* LoadTable(const FSoftObjectPath& Path)
	{
		return AEWTables::Load(Path);
	}

	TArray<UDataTable*> GetCompositeParents(UDataTable* Table)
	{
		return AEWTables::GetParents(Table);
	}

	template <typename FuncType>
	void ForEachRow(std::initializer_list<const UDataTable*> TableList, FuncType Fn)
	{
		for (const UDataTable* Table : TableList)
		{
			if (!Table || !Table->GetRowStruct())
			{
				continue;
			}
			for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
			{
				Fn(Table, Pair.Key, Pair.Value, Table->GetRowStruct());
			}
		}
	}

	struct FFieldValue
	{
		const TCHAR* Key;
		FString Value;
	};

	using AEWTables::FRowBuffer;
	using AEWTables::ReplaceRow;

	FString MeshFolder(const USkeletalMesh* Mesh)
	{
		return Mesh ? FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName()) : FString();
	}

	FName UnlockRowName(int32 UnlockID)
	{
		return AEWTables::UnlockRowName(UnlockID);
	}

	UClass* FindWAClass()
	{
		UClass* Class = LoadObject<UClass>(nullptr, TEXT("/Script/Creation.EditWrestlerPartsAsset"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Class)
		{
			Class = FindObject<UClass>(ANY_PACKAGE, TEXT("EditWrestlerPartsAsset"));
		}
		return Class;
	}
}

FEditPartMakerTables EditPartMaker::LoadTables(TArray<FString>& OutErrors)
{
	const UEditPartMakerSettings* Settings = GetDefault<UEditPartMakerSettings>();
	FEditPartMakerTables Tables;
	Tables.Parts = LoadTable(Settings->PartsTable);
	Tables.PartsList = LoadTable(Settings->PartsListTable);
	Tables.Unlock = LoadTable(Settings->UnlockTable);
	Tables.BaseUnlock = LoadTable(Settings->BaseUnlockTable);
	Tables.Category = LoadTable(Settings->CategoryTable);

	auto Check = [&OutErrors](const UDataTable* Table, const FSoftObjectPath& Path, const TCHAR* What)
	{
		if (!Table)
		{
			OutErrors.Add(FString::Printf(TEXT("%s not found: %s"), What, *Path.ToString()));
		}
		else if (!Table->GetRowStruct())
		{
			OutErrors.Add(FString::Printf(TEXT("%s has no row struct. Is the game stub module loaded?"), *Table->GetName()));
		}
	};
	Check(Tables.Parts, Settings->PartsTable, TEXT("Parts table"));
	Check(Tables.PartsList, Settings->PartsListTable, TEXT("Parts list table"));
	Check(Tables.Unlock, Settings->UnlockTable, TEXT("Unlock table"));
	Check(Tables.BaseUnlock, Settings->BaseUnlockTable, TEXT("Base unlock table"));
	Check(Tables.Category, Settings->CategoryTable, TEXT("Category table"));
	return Tables;
}

TArray<FString> EditPartMaker::GetEnumOptions(const UDataTable* PartsTable, const TCHAR* FieldName)
{
	TArray<FString> Out;
	if (!PartsTable || !PartsTable->GetRowStruct())
	{
		return Out;
	}
	FProperty* Prop = FindField(PartsTable->GetRowStruct(), FieldName);
	const UEnum* Enum = nullptr;
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		Enum = EnumProp->GetEnum();
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		Enum = ByteProp->Enum;
	}
	if (!Enum)
	{
		return Out;
	}
	for (int32 i = 0; i < Enum->NumEnums(); ++i)
	{
		const FString Name = Enum->GetNameStringByIndex(i);
		if (Name.EndsWith(TEXT("_MAX")) || Enum->HasMetaData(TEXT("Hidden"), i))
		{
			continue;
		}
		Out.Add(Name);
	}
	return Out;
}

TArray<FString> EditPartMaker::GetCategoryOptions(const UDataTable* CategoryTable)
{
	TArray<FString> Out;
	if (CategoryTable)
	{
		for (const FName& Name : CategoryTable->GetRowNames())
		{
			Out.Add(Name.ToString());
		}
		Out.Sort();
	}
	return Out;
}

void EditPartMaker::SuggestIds(const FEditPartMakerTables& Tables, const FString& IgnoreRow, int32& OutPartsID, int32& OutUnlockID)
{
	const UEditPartMakerSettings* Settings = GetDefault<UEditPartMakerSettings>();
	TSet<int64> UsedParts;
	TSet<int64> UsedUnlocks;
	const FName Ignore(*IgnoreRow);

	ForEachRow({ Tables.Parts, Tables.PartsList }, [&](const UDataTable*, FName RowName, const uint8* Row, const UScriptStruct* Struct)
	{
		if (!IgnoreRow.IsEmpty() && RowName == Ignore)
		{
			return;
		}
		int64 Value = 0;
		if (GetInt(Struct, Row, TEXT("PartsID"), Value)) UsedParts.Add(Value);
		if (GetInt(Struct, Row, TEXT("UnlockItemId"), Value)) UsedUnlocks.Add(Value);
	});
	ForEachRow({ Tables.Unlock, Tables.BaseUnlock }, [&](const UDataTable*, FName, const uint8* Row, const UScriptStruct* Struct)
	{
		int64 Value = 0;
		if (GetInt(Struct, Row, TEXT("UniqueID"), Value)) UsedUnlocks.Add(Value);
	});

	int64 PartsID = Settings->FirstPartsID;
	while (UsedParts.Contains(PartsID)) ++PartsID;
	int64 UnlockID = Settings->FirstUnlockID;
	while (UsedUnlocks.Contains(UnlockID) && UnlockID < Settings->LastUnlockID) ++UnlockID;

	OutPartsID = static_cast<int32>(PartsID);
	OutUnlockID = static_cast<int32>(UnlockID);
}

void EditPartMaker::FillDefaultsFromMesh(USkeletalMesh* Mesh, FPartRequest& Request)
{
	if (!Mesh)
	{
		return;
	}
	FString Base = Mesh->GetName();
	Base.RemoveFromStart(TEXT("SK_"));
	Base.RemoveFromStart(TEXT("SKM_"));

	Request.Mesh = Mesh;
	Request.RowName = Base;
	Request.Label = Base.Replace(TEXT("_"), TEXT(" ")).ToUpper();
	Request.WAName = TEXT("WA_") + Base;
}

void EditPartMaker::Validate(const FEditPartMakerTables& Tables, const FPartRequest& R, TArray<FString>& Errors, TArray<FString>& Warnings)
{
	const UEditPartMakerSettings* Settings = GetDefault<UEditPartMakerSettings>();
	if (!Tables.IsUsable())
	{
		Errors.Add(TEXT("Parts/unlock tables not loaded."));
		return;
	}

	USkeletalMesh* Mesh = R.Mesh.Get();
	if (!Mesh) Errors.Add(TEXT("No skeletal mesh selected."));
	if (R.RowName.IsEmpty() || R.RowName.Contains(TEXT(" ")) || R.RowName.Contains(TEXT(",")) || R.RowName.Contains(TEXT("\"")))
		Errors.Add(TEXT("Row name must be set, without spaces, commas or quotes."));
	if (R.WAName.IsEmpty())
		Errors.Add(TEXT("Parts name not set."));
	if (Mesh && !MeshFolder(Mesh).StartsWith(TEXT("/Game/")))
		Errors.Add(TEXT("The mesh must be under /Game/."));
	if (R.Slot.IsEmpty() || R.Gender.IsEmpty() || R.FaceType.IsEmpty() || R.Attire.IsEmpty())
		Errors.Add(TEXT("Set slot, gender, face type and attire condition."));
	if (R.Label.IsEmpty())
		Warnings.Add(TEXT("Label is empty - the menu will show a blank name."));

	const FName Row(*R.RowName);

	const bool bInParts = Tables.Parts->GetRowMap().Contains(Row);
	const bool bInList = Tables.PartsList && Tables.PartsList->GetRowMap().Contains(Row);
	if (bInParts && !R.bOverwrite)
		Errors.Add(FString::Printf(TEXT("Row '%s' already exists in %s. Tick 'Update existing' to replace it."), *R.RowName, *Tables.Parts->GetName()));
	if (bInList && !bInParts)
		Errors.Add(FString::Printf(TEXT("Row '%s' is a vanilla row in %s - choose another name."), *R.RowName, *Tables.PartsList->GetName()));

	if (R.PartsID <= 0) Errors.Add(TEXT("PartsID must be > 0."));
	TArray<FString> PartsIdUsers;
	TArray<FString> UnlockUsers;
	const FString WantedSlot = R.Slot;
	ForEachRow({ Tables.Parts, Tables.PartsList }, [&](const UDataTable*, FName RowName, const uint8* RowPtr, const UScriptStruct* Struct)
	{
		if (RowName == Row)
		{
			return;
		}
		int64 Value = 0;
		if (GetInt(Struct, RowPtr, TEXT("PartsID"), Value) && Value == R.PartsID) PartsIdUsers.AddUnique(RowName.ToString());
		if (GetInt(Struct, RowPtr, TEXT("UnlockItemId"), Value) && Value == R.UnlockID) UnlockUsers.AddUnique(RowName.ToString());

		if (R.FaceType != TEXT("Default")
			&& ShortEnum(GetAsText(Struct, RowPtr, TEXT("EditSlot"))) == WantedSlot
			&& ShortEnum(GetAsText(Struct, RowPtr, TEXT("FaceTypeCondition"))) == R.FaceType)
		{
			const FString OtherGender = ShortEnum(GetAsText(Struct, RowPtr, TEXT("GenderCondition")));
			if (OtherGender == R.Gender || OtherGender == TEXT("Any") || R.Gender == TEXT("Any"))
			{
				Warnings.Add(FString::Printf(TEXT("'%s' is also a %s part with face type %s for %s; the menu may merge them."),
					*RowName.ToString(), *WantedSlot, *R.FaceType, *OtherGender));
			}
		}
	});
	if (PartsIdUsers.Num() > 0)
		Errors.Add(FString::Printf(TEXT("PartsID %d is already used by %s."), R.PartsID, *FString::Join(PartsIdUsers, TEXT(", "))));
	if (UnlockUsers.Num() > 0)
		Errors.Add(FString::Printf(TEXT("Unlock ID %d is already used by %s."), R.UnlockID, *FString::Join(UnlockUsers, TEXT(", "))));

	if (R.UnlockID < Settings->FirstUnlockID || R.UnlockID > Settings->LastUnlockID)
		Warnings.Add(FString::Printf(TEXT("Unlock ID %d is outside the custom range %d-%d."), R.UnlockID, Settings->FirstUnlockID, Settings->LastUnlockID));
	const FName UnlockRow = UnlockRowName(R.UnlockID);
	bool bVanillaUnlock = false;
	ForEachRow({ Tables.BaseUnlock }, [&](const UDataTable*, FName RowName, const uint8* RowPtr, const UScriptStruct* Struct)
	{
		int64 Value = 0;
		if (GetInt(Struct, RowPtr, TEXT("UniqueID"), Value) && Value == R.UnlockID && !Tables.Unlock->GetRowMap().Contains(RowName))
		{
			bVanillaUnlock = true;
		}
	});
	if (bVanillaUnlock)
		Errors.Add(FString::Printf(TEXT("Unlock ID %d is a vanilla unlock row."), R.UnlockID));
	else if (Tables.Unlock->GetRowMap().Contains(UnlockRow) && !(R.bOverwrite && bInParts))
		Warnings.Add(FString::Printf(TEXT("%s already exists in %s, unused; it will be replaced."), *UnlockRow.ToString(), *Tables.Unlock->GetName()));

	if (Tables.Category)
	{
		if (!R.WACategory.IsEmpty() && R.WACategory != TEXT("None") && !Tables.Category->GetRowMap().Contains(FName(*R.WACategory)))
			Warnings.Add(FString::Printf(TEXT("Parts category '%s' isn't a row of %s; no effect."), *R.WACategory, *Tables.Category->GetName()));
		if (!R.CompatibleCategory.IsEmpty() && R.CompatibleCategory != TEXT("---") && !Tables.Category->GetRowMap().Contains(FName(*R.CompatibleCategory)))
			Warnings.Add(FString::Printf(TEXT("Compatible category '%s' is not a row of %s."), *R.CompatibleCategory, *Tables.Category->GetName()));
	}
	if (R.Slot == TEXT("Suits") && (R.WACategory.IsEmpty() || R.WACategory == TEXT("None")))
		Warnings.Add(TEXT("Suit with category None: the body won't be masked."));

	if (Tables.PartsList && !GetCompositeParents(Tables.PartsList).Contains(Tables.Parts))
		Errors.Add(FString::Printf(TEXT("%s is not a parent of %s; the game won't see the row."), *Tables.Parts->GetName(), *Tables.PartsList->GetName()));
	if (Tables.BaseUnlock && !GetCompositeParents(Tables.BaseUnlock).Contains(Tables.Unlock))
		Warnings.Add(FString::Printf(TEXT("%s is not a parent of %s - the unlock row would be ignored."), *Tables.Unlock->GetName(), *Tables.BaseUnlock->GetName()));

	if (Mesh)
	{
		const USkeleton* Skeleton = Mesh->GetSkeleton();
		if (!Skeleton)
		{
			Errors.Add(TEXT("Mesh has no skeleton."));
		}
		else if (!Skeleton->GetPathName().StartsWith(TEXT("/Game/GameData/Resources/Models/WrestlerData/")))
		{
			Warnings.Add(FString::Printf(TEXT("Mesh uses its own skeleton (%s); bone names must match the game's."), *Skeleton->GetName()));
		}
	}

	const FString WAPackage = MeshFolder(Mesh) / R.WAName;
	if (Mesh && !R.WAName.IsEmpty() && FPackageName::DoesPackageExist(WAPackage))
		Warnings.Add(FString::Printf(TEXT("%s exists; its SkeletalMesh and Category will be updated."), *WAPackage));
}

bool EditPartMaker::MakePart(const FEditPartMakerTables& Tables, const FPartRequest& R, TArray<FString>& Log)
{
	TArray<FString> Errors;
	TArray<FString> Warnings;
	Validate(Tables, R, Errors, Warnings);
	if (Errors.Num() > 0)
	{
		for (const FString& E : Errors) Log.Add(TEXT("ERROR: ") + E);
		return false;
	}
	for (const FString& W : Warnings) Log.Add(TEXT("warning: ") + W);

	const UEditPartMakerSettings* Settings = GetDefault<UEditPartMakerSettings>();
	USkeletalMesh* Mesh = R.Mesh.Get();
	UClass* WAClass = FindWAClass();
	if (!WAClass)
	{
		Log.Add(TEXT("ERROR: EditWrestlerPartsAsset not found. Is the Creation stub module loaded?"));
		return false;
	}

	const UScriptStruct* PartsStruct = Tables.Parts->GetRowStruct();
	const UScriptStruct* UnlockStruct = Tables.Unlock->GetRowStruct();
	const FString WAPackageName = MeshFolder(Mesh) / R.WAName;
	const FString WAObjectPath = WAPackageName + TEXT(".") + R.WAName;
	const FString Thumb = R.ThumbnailPath.IsEmpty() ? FString(TEXT("None")) : R.ThumbnailPath;

	FString Err;
	TArray<FString> RowErrors;

	FRowBuffer PartsRow(PartsStruct);
	if (R.bOverwrite)
	{
		if (const uint8* Existing = Tables.Parts->FindRowUnchecked(FName(*R.RowName)))
		{
			PartsStruct->CopyScriptStruct(PartsRow.Memory, Existing);
		}
	}
	const FFieldValue PartsValues[] = {
		{ TEXT("UnlockItemId"), FString::FromInt(R.UnlockID) },
		{ TEXT("PartsID"), FString::FromInt(R.PartsID) },
		{ TEXT("Label"), FString::Printf(TEXT("INVTEXT(\"%s\")"), *Quote(R.Label)) },
		{ TEXT("EditSlot"), R.Slot },
		{ TEXT("Asset"), WAObjectPath },
		{ TEXT("AttireTypeCondition"), R.Attire },
		{ TEXT("GenderCondition"), R.Gender },
		{ TEXT("FaceTypeCondition"), R.FaceType },
		{ TEXT("CommpatiablePartsCategory"), FString::Printf(TEXT("(Name=\"%s\")"), *Quote(R.CompatibleCategory.IsEmpty() ? FString(TEXT("---")) : R.CompatibleCategory)) },
		{ TEXT("ThumbnailTexture"), Thumb },
	};
	for (const FFieldValue& Value : PartsValues)
	{
		if (!SetByText(PartsStruct, PartsRow.Memory, Value.Key, Value.Value, Err)) RowErrors.Add(Err);
	}

	FRowBuffer UnlockRow(UnlockStruct);
	const uint8* Template = nullptr;
	if (Tables.BaseUnlock && Tables.BaseUnlock->GetRowStruct() == UnlockStruct)
	{
		Template = Tables.BaseUnlock->FindRowUnchecked(Settings->UnlockTemplateRow);
	}
	if (Template)
	{
		UnlockStruct->CopyScriptStruct(UnlockRow.Memory, Template);
	}
	else
	{
		Log.Add(FString::Printf(TEXT("warning: unlock template row %s not found; using defaults."), *Settings->UnlockTemplateRow.ToString()));
		const FFieldValue Defaults[] = {
			{ TEXT("RarityStr"), TEXT("1. Common") }, { TEXT("Rarity"), TEXT("Common") }, { TEXT("OptionalSortNo"), TEXT("-1") },
			{ TEXT("DefaultStockStatus"), TEXT("True") }, { TEXT("InStockTiming"), TEXT("-1") }, { TEXT("CurrencyType"), TEXT("Cash") },
			{ TEXT("BrandLogoAssetPath"), TEXT("None") },
		};
		for (const FFieldValue& Value : Defaults)
		{
			if (!SetByText(UnlockStruct, UnlockRow.Memory, Value.Key, Value.Value, Err)) Log.Add(TEXT("warning: ") + Err);
		}
	}
	const FFieldValue UnlockValues[] = {
		{ TEXT("UniqueID"), FString::FromInt(R.UnlockID) },
		{ TEXT("UnlockCategory"), TEXT("WrestlerParts") },
		{ TEXT("DefaultUnlocked"), TEXT("True") },
		{ TEXT("ThumbnailsAssetPath"), Thumb },
		{ TEXT("DisplayText"), FString::Printf(TEXT("INVTEXT(\"%s\")"), *Quote(R.Label)) },
		{ TEXT("DetailCategoryText"), FString::Printf(TEXT("INVTEXT(\"%s\")"), *Quote(R.Slot.ToUpper())) },
	};
	for (const FFieldValue& Value : UnlockValues)
	{
		if (!SetByText(UnlockStruct, UnlockRow.Memory, Value.Key, Value.Value, Err)) RowErrors.Add(Err);
	}

	if (RowErrors.Num() > 0)
	{
		for (const FString& E : RowErrors) Log.Add(TEXT("ERROR: ") + E);
		Log.Add(TEXT("Nothing was changed."));
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("MakePart", "Make AEW Part"));

	UObject* WA = nullptr;
	if (FPackageName::DoesPackageExist(WAPackageName))
	{
		WA = LoadObject<UObject>(nullptr, *WAObjectPath);
		if (WA && !WA->IsA(WAClass))
		{
			Log.Add(FString::Printf(TEXT("ERROR: %s is a %s, not an EditWrestlerPartsAsset."), *WAObjectPath, *WA->GetClass()->GetName()));
			return false;
		}
	}
	if (!WA)
	{
		UPackage* Package = CreatePackage(*WAPackageName);
		Package->FullyLoad();
		WA = NewObject<UObject>(Package, WAClass, FName(*R.WAName), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(WA);
		Log.Add(TEXT("created ") + WAObjectPath);
	}
	WA->Modify();
	if (!SetByText(WAClass, WA, TEXT("SkeletalMesh"), Mesh->GetPathName(), Err)) Log.Add(TEXT("ERROR: ") + Err);
	const FString WACategory = R.WACategory.IsEmpty() ? FString(TEXT("None")) : R.WACategory;
	if (!SetByText(WAClass, WA, TEXT("Category"), FString::Printf(TEXT("(Name=\"%s\")"), *Quote(WACategory)), Err)) Log.Add(TEXT("warning: ") + Err);
	WA->PostEditChange();
	WA->MarkPackageDirty();
	Log.Add(FString::Printf(TEXT("WA %s: SkeletalMesh=%s, Category=%s"), *R.WAName, *Mesh->GetName(), *WACategory));

	ReplaceRow(Tables.Parts, FName(*R.RowName), PartsRow);
	Log.Add(FString::Printf(TEXT("%s: row %s (PartsID %d, unlock %d, %s, %s, %s)"),
		*Tables.Parts->GetName(), *R.RowName, R.PartsID, R.UnlockID, *R.Slot, *R.Gender, *R.FaceType));

	ReplaceRow(Tables.Unlock, UnlockRowName(R.UnlockID), UnlockRow);
	Log.Add(FString::Printf(TEXT("%s: row %s (default unlocked)"), *Tables.Unlock->GetName(), *UnlockRowName(R.UnlockID).ToString()));

	TArray<UPackage*> Packages = { WA->GetOutermost(), Tables.Parts->GetOutermost(), Tables.Unlock->GetOutermost() };
	int32 PartChunk = 0;
	{
		const FString LabelPackage = MeshFolder(Mesh) / (TEXT("PAL_") + R.WAName);
		PartChunk = AEWPackaging::ChunkForLabel(LabelPackage);
		AEWPackaging::UpdateLabel(LabelPackage, PartChunk, { WA, Mesh }, true, true, Packages, &Log);
		AEWPackaging::LabelSharedTables({ Tables.Parts, Tables.PartsList, Tables.Unlock, Tables.BaseUnlock }, Packages, &Log);
	}

	if (Settings->bSaveChangesAutomatically)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(Packages,  false,  false);
		Log.Add(FString::Printf(TEXT("saved %d package(s): WA, tables%s"), Packages.Num(), TEXT(", labels")));
	}

	Log.Add(FString::Printf(TEXT("%s goes in chunk %d; tables in the table chunks."), *MeshFolder(Mesh), PartChunk));
	return true;
}

#undef LOCTEXT_NAMESPACE
