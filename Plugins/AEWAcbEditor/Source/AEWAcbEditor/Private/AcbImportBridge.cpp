#include "AcbImportBridge.h"
#include "AcbEditorSession.h"
#include "AcbEditorSettings.h"
#include "AcbCueDirtyFilter.h"

#include "AssetImportTask.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DataTableEditorUtils.h"
#include "Engine/CompositeDataTable.h"
#include "Engine/DataTable.h"
#include "Engine/AssetManager.h"
#include "Engine/EngineTypes.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SoundAtomCue.h"
#include "SoundAtomCueSheet.h"
#include "Settings/ProjectPackagingSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace AcbImportBridgeLocal
{
	FProperty* FindProp(UObject* Object, const TCHAR* Name)
	{
		return FindFProperty<FProperty>(Object->GetClass(), Name);
	}

	bool SetBool(UObject* Object, const TCHAR* Name, bool bValue)
	{
		if (FBoolProperty* Property = CastField<FBoolProperty>(FindProp(Object, Name)))
		{
			Property->SetPropertyValue_InContainer(Object, bValue);
			return true;
		}
		return false;
	}

	bool SetString(UObject* Object, const TCHAR* Name, const FString& Value)
	{
		FProperty* Property = FindProp(Object, Name);
		if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
		{
			StrProperty->SetPropertyValue_InContainer(Object, Value);
			return true;
		}
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == FDirectoryPath::StaticStruct())
			{
				StructProperty->ContainerPtrToValuePtr<FDirectoryPath>(Object)->Path = Value;
				return true;
			}
		}
		return false;
	}

	bool SetEnum(UObject* Object, const TCHAR* Name, const TCHAR* ValueName)
	{
		FProperty* Property = FindProp(Object, Name);
		UEnum* Enum = nullptr;
		FNumericProperty* Underlying = nullptr;
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Enum = EnumProperty->GetEnum();
			Underlying = EnumProperty->GetUnderlyingProperty();
		}
		else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			Enum = ByteProperty->Enum;
			Underlying = ByteProperty;
		}
		if (!Enum || !Underlying)
		{
			return false;
		}
		int64 Value = Enum->GetValueByNameString(ValueName);
		if (Value == INDEX_NONE)
		{
			Value = Enum->GetValueByNameString(Enum->GetName() + TEXT("::") + ValueName);
		}
		if (Value == INDEX_NONE)
		{
			return false;
		}
		Underlying->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Value);
		return true;
	}

	UObject* MakeImportOptions(bool bStreaming, bool bCopyAwb, const FString& AwbDirectory, TArray<FString>& OutLog)
	{
		UClass* OptionsClass = FindObject<UClass>(ANY_PACKAGE, TEXT("SoundAtomCueSheetImportOptions"));
		if (!OptionsClass)
		{
			OutLog.Add(TEXT("warning: CriWare import options class not found; the importer's saved defaults are used."));
			return nullptr;
		}
		UObject* Options = NewObject<UObject>(GetTransientPackage(), OptionsClass);
		TArray<FString> Missing;
		if (!SetBool(Options, TEXT("bUseCueSheetNameAsPrefix"), false)) Missing.Add(TEXT("bUseCueSheetNameAsPrefix"));
		if (!SetString(Options, TEXT("AssetFilenamePrefix"), FString())) Missing.Add(TEXT("AssetFilenamePrefix"));
		if (!SetString(Options, TEXT("AssetFilenameSuffix"), FString())) Missing.Add(TEXT("AssetFilenameSuffix"));
		if (!SetEnum(Options, TEXT("NewCuePreset"), TEXT("Create"))) Missing.Add(TEXT("NewCuePreset"));
		if (!SetEnum(Options, TEXT("ExistingCuePreset"), TEXT("Keep"))) Missing.Add(TEXT("ExistingCuePreset"));
		if (bStreaming)
		{
			if (!SetBool(Options, TEXT("bForceCopyAwbFiles"), bCopyAwb)) Missing.Add(TEXT("bForceCopyAwbFiles"));
			if (!SetBool(Options, TEXT("bOverrideAwbDirectory"), !AwbDirectory.IsEmpty())) Missing.Add(TEXT("bOverrideAwbDirectory"));
			if (!SetString(Options, TEXT("AwbDirectory"), AwbDirectory)) Missing.Add(TEXT("AwbDirectory"));
		}
		if (Missing.Num() > 0)
		{
			OutLog.Add(TEXT("warning: CriWare import options without these settings (defaults used): ") + FString::Join(Missing, TEXT(", ")));
		}
		return Options;
	}
}

namespace AcbCueDeleteLocal
{
	bool ValueUses(FProperty* Property, const void* Value, const FSoftObjectPath& Path, const UObject* Object);

	bool StructUses(const UStruct* Struct, const void* Data, const FSoftObjectPath& Path, const UObject* Object)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				if (ValueUses(*It, It->ContainerPtrToValuePtr<void>(Data, Index), Path, Object))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool ValueUses(FProperty* Property, const void* Value, const FSoftObjectPath& Path, const UObject* Object)
	{
		if (CastField<FSoftObjectProperty>(Property))
		{
			return static_cast<const FSoftObjectPtr*>(Value)->ToSoftObjectPath() == Path;
		}
		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			return Object && ObjectProperty->GetObjectPropertyValue(Value) == Object;
		}
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			return StructUses(StructProperty->Struct, Value, Path, Object);
		}
		if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(Array, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				if (ValueUses(Array->Inner, Helper.GetRawPtr(Index), Path, Object))
				{
					return true;
				}
			}
		}
		return false;
	}

	FString FindUser(const FAssetData& Cue, const TSet<FName>& Ignore)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		const FSoftObjectPath Path = Cue.ToSoftObjectPath();
		const UObject* Object = Cue.IsAssetLoaded() ? Cue.GetAsset() : nullptr;
		TArray<FName> Referencers;
		Registry.GetReferencers(Cue.PackageName, Referencers);
		for (const FName& Referencer : Referencers)
		{
			if (Ignore.Contains(Referencer))
			{
				continue;
			}
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Referencer, Assets);
			bool bReal = Assets.Num() == 0;
			for (const FAssetData& Asset : Assets)
			{
				const UDataTable* Table = Cast<UDataTable>(Asset.GetAsset());
				if (!Table)
				{
					if (!Asset.GetClass() || !Asset.GetClass()->IsChildOf(USoundAtomCueSheet::StaticClass()))
					{
						bReal = true;
					}
					continue;
				}
				if (Table->IsA<UCompositeDataTable>() || !Table->GetRowStruct())
				{
					continue;
				}
				for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
				{
					if (StructUses(Table->GetRowStruct(), Row.Value, Path, Object))
					{
						bReal = true;
						break;
					}
				}
			}
			if (bReal)
			{
				return Referencer.ToString();
			}
		}
		return FString();
	}
}

bool AcbImportBridge::Import(const FRequest& Request, TArray<FString>& OutLog, FString& OutError)
{
	using namespace AcbImportBridgeLocal;

	const int64 Size = IFileManager::Get().FileSize(*Request.AcbFile);
	if (Size < 0)
	{
		OutError = TEXT("File not found: ") + Request.AcbFile;
		return false;
	}
	if (uint64(Size) > FAcbEditorSession::CriImportLimit)
	{
		OutError = FString::Printf(TEXT("%s is %.2f GiB. The CriWare importer reads ACBs with a 32-bit size, so it must stay under 2 GiB. ")
			TEXT("Keep the audio in a streaming .awb (open the cue sheet with its .awb), or remove tracks."), *FPaths::GetCleanFilename(Request.AcbFile), double(Size) / double(1ull << 30));
		return false;
	}
	if (Request.bHasStreamingAwb && !FPaths::FileExists(FPaths::ChangeExtension(Request.AcbFile, TEXT("awb"))))
	{
		OutError = TEXT("The streaming .awb must sit next to the .acb with the same name.");
		return false;
	}
	if (!Request.PackagePath.StartsWith(TEXT("/Game")))
	{
		OutError = TEXT("Cue sheet folder must be under /Game: ") + Request.PackagePath;
		return false;
	}

	const FString SheetName = FPaths::GetBaseFilename(Request.AcbFile);
	TStrongObjectPtr<UAssetImportTask> Task(NewObject<UAssetImportTask>());
	TStrongObjectPtr<UObject> Options(MakeImportOptions(Request.bHasStreamingAwb, !Request.bAwbAlreadyCopied, Request.AwbDirectory, OutLog));
	Task->Filename = Request.AcbFile;
	Task->DestinationPath = Request.PackagePath;
	Task->DestinationName = SheetName;
	Task->bReplaceExisting = true;
	Task->bAutomated = true;
	Task->bSave = false;
	Task->Options = Options.Get();

	FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<int32> SheetChunks;
	{
		const FAssetData Existing = Registry.Get().GetAssetByObjectPath(FName(*(Request.PackagePath / SheetName + TEXT(".") + SheetName)));
		if (Existing.IsValid())
		{
			SheetChunks.Append(Existing.ChunkIDs);
		}
	}

	OutLog.Add(FString::Printf(TEXT("Importing %s into %s (%.1f MB)..."), *FPaths::GetCleanFilename(Request.AcbFile), *Request.PackagePath, double(Size) / (1024.0 * 1024.0)));
	TArray<UAssetImportTask*> Tasks;
	Tasks.Add(Task.Get());
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks(Tasks);
	if (Task->ImportedObjectPaths.Num() == 0)
	{
		OutError = TEXT("The CriWare importer did not import anything -- see the Output Log (LogCriWareEditor).");
		return false;
	}

	const FString SheetObjectPath = Request.PackagePath / SheetName + TEXT(".") + SheetName;
	USoundAtomCueSheet* Sheet = LoadObject<USoundAtomCueSheet>(nullptr, *SheetObjectPath);
	if (!Sheet)
	{
		OutError = TEXT("Import finished but the cue sheet was not found at ") + SheetObjectPath;
		return false;
	}

	UPackage* SheetPackage = Sheet->GetOutermost();
	if (SheetPackage->GetChunkIDs().Num() > 0)
	{
		SheetChunks = SheetPackage->GetChunkIDs();
	}
	else if (SheetChunks.Num() > 0)
	{
		SheetPackage->SetChunkIDs(SheetChunks);
		SheetPackage->MarkPackageDirty();
		OutLog.Add(TEXT("Kept the cue sheet's chunk assignment through the reimport."));
	}
	int32 CuesAssigned = 0;

	FAcbCueDirtyFilter::Get().Flush();

	TArray<UPackage*> ToSave;
	ToSave.Add(SheetPackage);
	TSet<FString> RemovedCues;
	if (Request.bDeleteRemovedCueAssets)
	{
		for (const FString& Name : Request.DeletedCues)
		{
			RemovedCues.Add(Name);
		}
		for (const TPair<FString, FString>& Rename : Request.RenamedCues)
		{
			RemovedCues.Add(Rename.Key);
		}
		for (const FString& Name : Request.CueNames)
		{
			RemovedCues.Remove(Name);
		}
	}
	TArray<FAssetData> CueAssetsToDelete;
	TArray<FString> Orphans;
	int32 CueAssets = 0;
	TArray<FAssetData> Assets;
	Registry.Get().GetAssetsByPath(FName(*Request.PackagePath), Assets, false);
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClass != USoundAtomCue::StaticClass()->GetFName())
		{
			continue;
		}
		USoundAtomCue* Cue = Cast<USoundAtomCue>(Asset.GetAsset());
		if (!Cue || Cue->CueSheet != Sheet)
		{
			continue;
		}
		++CueAssets;
		if (RemovedCues.Contains(Cue->GetName()))
		{
			CueAssetsToDelete.Add(Asset);
			continue;
		}
		if (Request.CueNames.Num() > 0 && !Request.CueNames.Contains(Cue->CueName))
		{
			Orphans.Add(Cue->GetName());
		}
		UPackage* CuePackage = Cue->GetOutermost();
		if (Request.bAssignSheetChunkToCues && SheetChunks.Num() > 0)
		{
			TArray<int32> Chunks = CuePackage->GetChunkIDs();
			const int32 Before = Chunks.Num();
			for (int32 Chunk : SheetChunks)
			{
				Chunks.AddUnique(Chunk);
			}
			if (Chunks.Num() != Before)
			{
				CuePackage->SetChunkIDs(Chunks);
				CuePackage->MarkPackageDirty();
				++CuesAssigned;
			}
		}
		if (CuePackage->IsDirty())
		{
			ToSave.Add(CuePackage);
		}
	}
	if (CuesAssigned > 0)
	{
		TArray<FString> ChunkNames;
		for (int32 Chunk : SheetChunks)
		{
			ChunkNames.Add(FString::FromInt(Chunk));
		}
		OutLog.Add(FString::Printf(TEXT("Assigned chunk %s (the cue sheet's) to %d cue asset(s)."), *FString::Join(ChunkNames, TEXT(", ")), CuesAssigned));
	}
	else if (SheetChunks.Num() == 0)
	{
		OutLog.Add(TEXT("The cue sheet has no chunk assigned, so its cues and .awb stay in pakchunk0. Assign it with Asset Actions > Assign to Chunk, then import again."));
	}
	OutLog.Add(FString::Printf(TEXT("Imported %s: %d SoundAtomCue assets point at it, %d changed."), *Sheet->GetPathName(), CueAssets, ToSave.Num() - 1));
	if (Orphans.Num() > 0)
	{
		OutLog.Add(FString::Printf(TEXT("warning: %d cue asset(s) have no cue in the ACB any more (delete them if unused): %s"),
			Orphans.Num(), *FString::Join(Orphans, TEXT(", "))));
	}

	if (Request.bHasStreamingAwb)
	{
		const FString AwbInContent = FPaths::ProjectContentDir() / Request.AwbDirectory / SheetName + TEXT(".awb");
		if (Request.AwbDirectory.IsEmpty())
		{
			OutLog.Add(TEXT("warning: AWB folder is empty, so the cue sheet uses CriWare's ContentDir and the .awb was copied to Content/. Set 'AWB folder' (default GameData/Resources/CriWareData)."));
		}
		else if (!FPaths::FileExists(AwbInContent))
		{
			OutLog.Add(TEXT("The importer did not copy the .awb; copying it now."));
			CopyAwbToProject(FPaths::ChangeExtension(Request.AcbFile, TEXT("awb")), Request.AwbDirectory, OutLog);
		}
		else
		{
			OutLog.Add(FString::Printf(TEXT("AWB in project: %s (%.1f MB); the cue sheet loads Content/%s/%s.awb."),
				*AwbInContent, double(IFileManager::Get().FileSize(*AwbInContent)) / (1024.0 * 1024.0), *Request.AwbDirectory, *SheetName));
		}
		if (Sheet->bOverrideAwbDirectory != !Request.AwbDirectory.IsEmpty())
		{
			OutLog.Add(TEXT("warning: the cue sheet's 'Override AWB directory' flag was not set by the importer; set it on the cue sheet asset."));
		}
		if (Request.bStageAwbForPackaging && !Request.AwbDirectory.IsEmpty())
		{
			EnsureStagedForPackaging(Request.AwbDirectory, OutLog);
			FString PakName;
			{
				const int32 Chunk = FindPackageChunk(Sheet->GetOutermost()->GetName(), OutLog);
				if (Chunk > 0)
				{
					PakName = FString::Printf(TEXT("pakchunk%d"), Chunk);
					OutLog.Add(FString::Printf(TEXT("Cue sheet %s is in chunk %d."), *SheetName, Chunk));
				}
				else if (Chunk == 0)
				{
					OutLog.Add(FString::Printf(TEXT("warning: cue sheet %s has no chunk, so %s.awb stays in pakchunk0. Assign the cue sheet a chunk (Asset Actions > Assign to Chunk) and save it: the .awb rule follows."), *SheetName, *SheetName));
				}
			}
			EnsureAwbPakRule(Request.AwbDirectory, SheetName, PakName, OutLog);
		}
	}

	if (Request.bUpdateDataTables)
	{
		TArray<FString> StillDeleted;
		for (const FString& Name : Request.DeletedCues)
		{
			if (Request.CueNames.Contains(Name))
			{
				OutLog.Add(FString::Printf(TEXT("%s was deleted and added again, so DataTable rows keep using it."), *Name));
			}
			else
			{
				StillDeleted.Add(Name);
			}
		}
		UpdateDataTables(Request.PackagePath, Request.RenamedCues, StillDeleted, Request.bDeleteRowsOfDeletedCues, ToSave, OutLog);
		AcbTableRows::RefreshComposites(ToSave);
		if (Request.TableRows.Num() > 0)
		{
			AcbTableRows::Apply(Request.PackagePath, Request.TableRows, ToSave, OutLog);
			AcbTableRows::RefreshComposites(ToSave);
		}
	}

	if (Request.bSaveAssets)
	{
		if (UEditorLoadingAndSavingUtils::SavePackages(ToSave, true))
		{
			OutLog.Add(FString::Printf(TEXT("Saved %d package(s)."), ToSave.Num()));
		}
		else
		{
			OutLog.Add(TEXT("warning: some packages could not be saved -- save them from the Content Browser."));
		}
	}

	if (CueAssetsToDelete.Num() > 0)
	{
		int32 Deleted = 0;
		TArray<FString> Kept;
		TSet<FName> Ignore;
		for (const FAssetData& Asset : CueAssetsToDelete)
		{
			Ignore.Add(Asset.PackageName);
		}
		TArray<UObject*> Objects;
		for (const FAssetData& Asset : CueAssetsToDelete)
		{
			const FString User = AcbCueDeleteLocal::FindUser(Asset, Ignore);
			UObject* Object = User.IsEmpty() ? Asset.GetAsset() : nullptr;
			if (Object)
			{
				Objects.Add(Object);
			}
			else
			{
				Kept.Add(FString::Printf(TEXT("%s (used by %s)"), *Asset.AssetName.ToString(), User.IsEmpty() ? TEXT("?") : *User));
			}
		}
		if (Objects.Num() > 0)
		{
			Deleted = ObjectTools::ForceDeleteObjects(Objects, false);
		}
		OutLog.Add(FString::Printf(TEXT("Deleted %d cue asset(s) of deleted or renamed cues."), Deleted));
		if (Kept.Num() > 0)
		{
			OutLog.Add(FString::Printf(TEXT("warning: %d cue asset(s) are still used and were kept: %s."),
				Kept.Num(), *FString::Join(Kept, TEXT(", "))));
		}
	}
	return true;
}

namespace AcbDataTableLocal
{
	struct FCueEdit
	{
		UObject* OldObject = nullptr;
		UObject* NewObject = nullptr;
		FSoftObjectPath OldPath;
		FSoftObjectPath NewPath;
		FString OldName;
		FString NewName;
	};

	int32 RewriteStruct(const UStruct* Struct, void* Data, const FCueEdit& Edit);

	int32 RewriteValue(FProperty* Property, void* Value, const FCueEdit& Edit)
	{
		if (FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			FSoftObjectPtr& Ptr = *static_cast<FSoftObjectPtr*>(Value);
			if (!Edit.OldPath.IsNull() && Ptr.ToSoftObjectPath() == Edit.OldPath)
			{
				Ptr = Edit.NewPath.IsNull() ? FSoftObjectPtr() : FSoftObjectPtr(Edit.NewPath);
				return 1;
			}
			return 0;
		}
		if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
		{
			if (Edit.OldObject && Object->GetObjectPropertyValue(Value) == Edit.OldObject)
			{
				Object->SetObjectPropertyValue(Value, Edit.NewObject);
				return 1;
			}
			return 0;
		}
		if (FStrProperty* Str = CastField<FStrProperty>(Property))
		{
			FString& Text = *static_cast<FString*>(Value);
			if (!Edit.OldName.IsEmpty() && Text.Equals(Edit.OldName, ESearchCase::IgnoreCase))
			{
				Text = Edit.NewName.IsEmpty() ? FString() : AcbTableRows::MatchCase(Text, Edit.NewName);
				return 1;
			}
			return 0;
		}
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			return RewriteStruct(StructProperty->Struct, Value, Edit);
		}
		if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(Array, Value);
			int32 Count = 0;
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				Count += RewriteValue(Array->Inner, Helper.GetRawPtr(Index), Edit);
			}
			return Count;
		}
		return 0;
	}

	int32 RewriteStruct(const UStruct* Struct, void* Data, const FCueEdit& Edit)
	{
		int32 Count = 0;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				Count += RewriteValue(*It, It->ContainerPtrToValuePtr<void>(Data, Index), Edit);
			}
		}
		return Count;
	}

	bool IsCuePath(const FSoftObjectPath& Path, const UClass* PropertyClass, IAssetRegistry& Registry)
	{
		if (Path.IsNull())
		{
			return false;
		}
		if (PropertyClass && PropertyClass->IsChildOf(USoundAtomCue::StaticClass()))
		{
			return true;
		}
		const FAssetData Asset = Registry.GetAssetByObjectPath(FName(*Path.ToString()));
		return Asset.IsValid() && Asset.AssetClass == USoundAtomCue::StaticClass()->GetFName();
	}

	bool HasCue(FProperty* Property, const void* Value, IAssetRegistry& Registry);

	bool StructHasCue(const UStruct* Struct, const void* Data, IAssetRegistry& Registry)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				if (HasCue(*It, It->ContainerPtrToValuePtr<void>(Data, Index), Registry))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool HasCue(FProperty* Property, const void* Value, IAssetRegistry& Registry)
	{
		if (FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			return IsCuePath(static_cast<const FSoftObjectPtr*>(Value)->ToSoftObjectPath(), Soft->PropertyClass, Registry);
		}
		if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
		{
			const UObject* Target = Object->GetObjectPropertyValue(Value);
			return Target && Target->IsA(USoundAtomCue::StaticClass());
		}
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			return StructHasCue(StructProperty->Struct, Value, Registry);
		}
		if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(Array, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				if (HasCue(Array->Inner, Helper.GetRawPtr(Index), Registry))
				{
					return true;
				}
			}
		}
		return false;
	}

	FIntProperty* FindUnlockField(const UScriptStruct* Struct)
	{
		for (TFieldIterator<FIntProperty> It(Struct); It; ++It)
		{
			const FString Name = Struct->GetAuthoredNameForField(*It);
			if (Name.StartsWith(TEXT("Unlock"), ESearchCase::IgnoreCase) && Name.EndsWith(TEXT("Id"), ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool IsWritable(UDataTable* Table, TArray<FString>& OutLog)
	{
		if (Table->GetOutermost()->HasAnyPackageFlags(PKG_Cooked))
		{
			OutLog.Add(FString::Printf(TEXT("warning: %s is cooked and can't be changed; update it by hand."), *Table->GetName()));
			return false;
		}
		return true;
	}
}

namespace AcbDataTableLocal
{
	void RemoveUnusedUnlockItems(const TSet<int32>& Ids, TArray<UPackage*>& ToSave, TArray<FString>& OutLog)
	{
		const UAcbEditorSettings* Settings = GetDefault<UAcbEditorSettings>();
		TSet<int32> StillUsed;
		TSet<int32> RemovedIds;
		TSet<FSoftObjectPath> UnlockTables;
		for (const FAcbRowTable& Entry : Settings->RowTables)
		{
			if (Entry.UnlockNewTable.IsValid())
			{
				UnlockTables.Add(Entry.UnlockNewTable);
			}
			TArray<const UDataTable*> Sources;
			AcbTableRows::GetSourceTables(Cast<UDataTable>(Entry.Table.TryLoad()), Sources);
			AcbTableRows::GetSourceTables(Cast<UDataTable>(Entry.NewTable.TryLoad()), Sources);
			for (const UDataTable* Source : Sources)
			{
				FIntProperty* Field = Source->GetRowStruct() ? FindUnlockField(Source->GetRowStruct()) : nullptr;
				if (!Field)
				{
					continue;
				}
				for (const TPair<FName, uint8*>& Row : Source->GetRowMap())
				{
					StillUsed.Add(Field->GetPropertyValue_InContainer(Row.Value));
				}
			}
		}
		for (const FSoftObjectPath& Path : UnlockTables)
		{
			UDataTable* Table = Cast<UDataTable>(Path.TryLoad());
			const UScriptStruct* UnlockStruct = Table ? Table->GetRowStruct() : nullptr;
			FIntProperty* UniqueId = nullptr;
			if (UnlockStruct)
			{
				for (TFieldIterator<FIntProperty> It(UnlockStruct); It; ++It)
				{
					if (UnlockStruct->GetAuthoredNameForField(*It).Equals(TEXT("UniqueID"), ESearchCase::IgnoreCase))
					{
						UniqueId = *It;
						break;
					}
				}
			}
			if (!UniqueId || !IsWritable(Table, OutLog))
			{
				continue;
			}
			TArray<FName> Remove;
			for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
			{
				const int32 Id = UniqueId->GetPropertyValue_InContainer(Row.Value);
				if (Ids.Contains(Id) && !StillUsed.Contains(Id))
				{
					Remove.Add(Row.Key);
					RemovedIds.Add(Id);
				}
			}
			TArray<FString> Removed;
			for (const FName& RowName : Remove)
			{
				if (FDataTableEditorUtils::RemoveRow(Table, RowName))
				{
					Removed.Add(RowName.ToString());
				}
			}
			if (Removed.Num() > 0)
			{
				Table->MarkPackageDirty();
				ToSave.AddUnique(Table->GetOutermost());
				OutLog.Add(FString::Printf(TEXT("%s: removed unlock item(s) %s (no row uses them now)."), *Table->GetName(), *FString::Join(Removed, TEXT(", "))));
			}
		}
		for (const int32 Id : Ids)
		{
			if (!RemovedIds.Contains(Id))
			{
				OutLog.Add(FString::Printf(TEXT("Unlock item %d kept: %s."), Id,
					StillUsed.Contains(Id) ? TEXT("another row still uses it") : TEXT("it isn't in a Cue Table's Unlock New Table (vanilla unlock items are never removed)")));
			}
		}
	}
}

void AcbImportBridge::UpdateDataTables(const FString& PackagePath, const TMap<FString, FString>& RenamedCues, const TArray<FString>& DeletedCues,
	bool bDeleteEmptyRows, TArray<UPackage*>& ToSave, TArray<FString>& OutLog)
{
	using namespace AcbDataTableLocal;
	TMap<FString, FString> Changes = RenamedCues;
	for (const FString& Deleted : DeletedCues)
	{
		Changes.Add(Deleted, FString());
	}
	if (Changes.Num() == 0)
	{
		return;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	static const FName DataTableClass(TEXT("DataTable"));
	TSet<int32> FreedUnlockIds;
	for (const TPair<FString, FString>& Change : Changes)
	{
		const FString OldPackage = PackagePath / Change.Key;
		const FAssetData OldAsset = Registry.GetAssetByObjectPath(FName(*(OldPackage + TEXT(".") + Change.Key)));
		if (!OldAsset.IsValid())
		{
			continue;
		}
		FCueEdit Edit;
		Edit.OldObject = OldAsset.GetAsset();
		Edit.OldPath = OldAsset.ToSoftObjectPath();
		Edit.OldName = Change.Key;
		Edit.NewName = Change.Value;
		if (!Change.Value.IsEmpty())
		{
			Edit.NewPath = FSoftObjectPath(PackagePath / Change.Value + TEXT(".") + Change.Value);
			Edit.NewObject = Edit.NewPath.TryLoad();
			if (!Edit.NewObject)
			{
				OutLog.Add(FString::Printf(TEXT("warning: no cue asset %s after import, so DataTables still use %s."), *Change.Value, *Change.Key));
				continue;
			}
		}
		TArray<FName> Referencers;
		Registry.GetReferencers(FName(*OldPackage), Referencers);
		for (const FName& Referencer : Referencers)
		{
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Referencer, Assets);
			for (const FAssetData& Asset : Assets)
			{
				if (Asset.AssetClass != DataTableClass)
				{
					continue;
				}
				UDataTable* Table = Cast<UDataTable>(Asset.GetAsset());
				if (!Table || !Table->GetRowStruct() || !IsWritable(Table, OutLog))
				{
					continue;
				}
				FDataTableEditorUtils::BroadcastPreChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
				Table->Modify();
				int32 Count = 0;
				TArray<FString> Rows;
				TArray<FName> EmptyRows;
				const bool bDeleting = Change.Value.IsEmpty() && bDeleteEmptyRows;
				for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
				{
					const bool bHadCue = bDeleting && StructHasCue(Table->GetRowStruct(), Row.Value, Registry);
					const int32 RowCount = RewriteStruct(Table->GetRowStruct(), Row.Value, Edit);
					if (RowCount > 0)
					{
						Count += RowCount;
						Rows.Add(Row.Key.ToString());
						if (bHadCue && !StructHasCue(Table->GetRowStruct(), Row.Value, Registry))
						{
							EmptyRows.Add(Row.Key);
						}
					}
				}
				FDataTableEditorUtils::BroadcastPostChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
				if (Count > 0)
				{
					Table->MarkPackageDirty();
					ToSave.AddUnique(Table->GetOutermost());
					OutLog.Add(FString::Printf(TEXT("%s: %s -> %s in row(s) %s."), *Table->GetName(), *Change.Key,
						Change.Value.IsEmpty() ? TEXT("(none, cue deleted)") : *Change.Value, *FString::Join(Rows, TEXT(", "))));
				}
				if (EmptyRows.Num() > 0)
				{
					FIntProperty* UnlockField = FindUnlockField(Table->GetRowStruct());
					TArray<FString> Removed;
					for (const FName& RowName : EmptyRows)
					{
						if (const uint8* RowData = Table->FindRowUnchecked(RowName))
						{
							if (UnlockField && UnlockField->GetPropertyValue_InContainer(RowData) != 0)
							{
								FreedUnlockIds.Add(UnlockField->GetPropertyValue_InContainer(RowData));
							}
						}
						if (FDataTableEditorUtils::RemoveRow(Table, RowName))
						{
							Removed.Add(RowName.ToString());
						}
					}
					Table->MarkPackageDirty();
					ToSave.AddUnique(Table->GetOutermost());
					OutLog.Add(FString::Printf(TEXT("%s: removed row(s) %s (no cues left)."), *Table->GetName(), *FString::Join(Removed, TEXT(", "))));
				}
			}
		}
	}

	if (FreedUnlockIds.Num() > 0)
	{
		RemoveUnusedUnlockItems(FreedUnlockIds, ToSave, OutLog);
	}
}

bool AcbImportBridge::EnsureStagedForPackaging(const FString& ContentRelativeDir, TArray<FString>& OutLog)
{
	FString Dir = ContentRelativeDir;
	FPaths::NormalizeDirectoryName(Dir);
	Dir.RemoveFromStart(TEXT("/"));
	Dir.RemoveFromEnd(TEXT("/"));
	if (Dir.IsEmpty())
	{
		return false;
	}

	UProjectPackagingSettings* Packaging = GetMutableDefault<UProjectPackagingSettings>();
	auto Covers = [&Dir](const TArray<FDirectoryPath>& List)
	{
		for (const FDirectoryPath& Entry : List)
		{
			FString Existing = Entry.Path;
			FPaths::NormalizeDirectoryName(Existing);
			Existing.RemoveFromStart(TEXT("/Game/"));
			Existing.RemoveFromStart(TEXT("/"));
			Existing.RemoveFromEnd(TEXT("/"));
			if (Dir.Equals(Existing, ESearchCase::IgnoreCase) || Dir.StartsWith(Existing + TEXT("/"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	};

	if (Covers(Packaging->DirectoriesToAlwaysStageAsNonUFS))
	{
		OutLog.Add(TEXT("warning: Content/") + Dir + TEXT(" is staged as non-UFS (loose files next to the paks). The game reads its AWBs from inside the paks; ")
			TEXT("move it to 'Additional Non-Asset Directories To Package' unless you ship the loose file."));
		return true;
	}
	if (Covers(Packaging->DirectoriesToAlwaysStageAsUFS))
	{
		OutLog.Add(TEXT("Packaging: Content/") + Dir + TEXT(" is already in 'Additional Non-Asset Directories To Package'."));
		return true;
	}

	FDirectoryPath NewEntry;
	NewEntry.Path = Dir;
	Packaging->DirectoriesToAlwaysStageAsUFS.Add(NewEntry);
	Packaging->UpdateDefaultConfigFile();
	OutLog.Add(TEXT("Packaging: added Content/") + Dir + TEXT(" to 'Additional Non-Asset Directories To Package' (DefaultGame.ini). ")
		TEXT("Files there go to pakchunk0 unless a pak file rule moves them."));
	return true;
}

int32 AcbImportBridge::FindPackageChunk(const FString& PackageName, TArray<FString>& OutLog)
{
	if (!UAssetManager::IsValid())
	{
		OutLog.Add(TEXT("warning: no Asset Manager, so the cue sheet's chunk is unknown."));
		return INDEX_NONE;
	}
	UAssetManager& Manager = UAssetManager::Get();
	Manager.UpdateManagementDatabase();
	TArray<int32> Chunks;
	TArray<int32> Overrides;
	TArray<int32> Existing;
	if (UPackage* Package = FindPackage(nullptr, *PackageName))
	{
		Existing = Package->GetChunkIDs();
	}
	else
	{
		FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> Assets;
		Registry.Get().GetAssetsByPackageName(FName(*PackageName), Assets);
		for (const FAssetData& Asset : Assets)
		{
			for (int32 Chunk : Asset.ChunkIDs)
			{
				Existing.AddUnique(Chunk);
			}
		}
	}
	Manager.GetPackageChunkIds(FName(*PackageName), nullptr, Existing, Chunks, &Overrides);
	for (int32 Chunk : Existing)
	{
		Chunks.AddUnique(Chunk);
	}
	Chunks.Append(Overrides);
	Chunks.Remove(0);
	if (Chunks.Num() == 0)
	{
		return 0;
	}
	Chunks.Sort();
	if (Chunks.Num() > 1)
	{
		TArray<FString> Names;
		for (int32 Chunk : Chunks)
		{
			Names.Add(FString::FromInt(Chunk));
		}
		OutLog.Add(FString::Printf(TEXT("warning: %s is in chunks %s; its .awb goes to chunk %d."), *PackageName, *FString::Join(Names, TEXT(", ")), Chunks.Last()));
	}
	return Chunks.Last();
}

bool AcbImportBridge::EnsureAwbPakRule(const FString& ContentRelativeDir, const FString& SheetName, const FString& InPakName, TArray<FString>& OutLog)
{
	const FString SectionName = TEXT("[AEWAcbEditor_Awb_") + SheetName + TEXT("]");
	static const TCHAR* LegacySectionName = TEXT("[AEWAcbEditor_StreamingAwb]");
	const FString RulesFile = FPaths::ProjectConfigDir() / TEXT("DefaultPakFileRules.ini");

	FString Dir = ContentRelativeDir;
	FPaths::NormalizeDirectoryName(Dir);
	Dir.RemoveFromStart(TEXT("/"));
	Dir.RemoveFromEnd(TEXT("/"));
	FString PakName = InPakName.TrimStartAndEnd();
	PakName.RemoveFromEnd(TEXT(".pak"));
	const bool bWant = !PakName.IsEmpty() && !Dir.IsEmpty();

	TArray<FString> Lines;
	if (FPaths::FileExists(RulesFile))
	{
		FFileHelper::LoadFileToStringArray(Lines, *RulesFile);
	}
	TArray<FString> Kept;
	bool bInOurs = false;
	bool bHadOurs = false;
	for (const FString& Line : Lines)
	{
		const FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.StartsWith(TEXT("[")))
		{
			bInOurs = Trimmed.Equals(SectionName, ESearchCase::IgnoreCase) || Trimmed.Equals(LegacySectionName, ESearchCase::IgnoreCase);
			bHadOurs |= bInOurs;
		}
		if (!bInOurs)
		{
			Kept.Add(Line);
		}
	}
	while (Kept.Num() > 0 && Kept[0].TrimStartAndEnd().IsEmpty())
	{
		Kept.RemoveAt(0);
	}

	TArray<FString> Out;
	if (bWant)
	{
		Out.Add(SectionName);
		Out.Add(FString::Printf(TEXT("; Written by the AEW ACB Editor plugin: %s.awb goes into the same pak as its cue sheet."), *SheetName));
		Out.Add(FString::Printf(TEXT("OverridePaks=\"%s\""), *PakName));
		Out.Add(TEXT("bOverrideChunkManifest=true"));
		Out.Add(FString::Printf(TEXT("+Files=\".../Content/%s/%s.awb\""), *Dir, *SheetName));
		Out.Add(FString());
	}
	else if (!bHadOurs)
	{
		return true;
	}
	Out.Append(Kept);

	if (Out == Lines)
	{
		OutLog.Add(FString::Printf(TEXT("Packaging: %s.awb already goes to %s (DefaultPakFileRules.ini)."), *SheetName, *PakName));
		return true;
	}
	if (!FFileHelper::SaveStringArrayToFile(Out, *RulesFile))
	{
		OutLog.Add(TEXT("warning: could not write ") + RulesFile + TEXT(" (read-only or checked in?)."));
		return false;
	}
	if (bWant)
	{
		OutLog.Add(FString::Printf(TEXT("Packaging: %s.awb now goes to %s-<Platform>.pak (rule in Config/DefaultPakFileRules.ini)."), *SheetName, *PakName));
	}
	else
	{
		OutLog.Add(FString::Printf(TEXT("Packaging: removed the pak rule for %s.awb; it goes to pakchunk0."), *SheetName));
	}
	return true;
}

namespace AcbImportBridgeLocal
{
	struct FAwbCopyProgress : public FCopyProgress
	{
		FScopedSlowTask* Task = nullptr;
		float Last = 0.f;
		virtual bool Poll(float Fraction) override
		{
			if (Task)
			{
				Task->EnterProgressFrame((Fraction - Last) * 100.f);
				Last = Fraction;
				return !Task->ShouldCancel();
			}
			return true;
		}
	};
}

bool AcbImportBridge::CopyAwbToProject(const FString& AwbFile, const FString& AwbDirectory, TArray<FString>& OutLog)
{
	if (!FPaths::FileExists(AwbFile))
	{
		OutLog.Add(TEXT("warning: no .awb to copy at ") + AwbFile);
		return false;
	}
	FString Dir = AwbDirectory;
	FPaths::NormalizeDirectoryName(Dir);
	Dir.RemoveFromStart(TEXT("/"));
	const FString DestDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()) / Dir;
	const FString Dest = DestDir / FPaths::GetCleanFilename(AwbFile);
	const FString Source = FPaths::ConvertRelativePathToFull(AwbFile);
	if (FPaths::IsSamePath(Source, Dest))
	{
		OutLog.Add(TEXT("AWB is already in the project: ") + Dest);
		return true;
	}
	IFileManager& Files = IFileManager::Get();
	Files.MakeDirectory(*DestDir, true);
	const int64 Size = Files.FileSize(*Source);

	FScopedSlowTask Task(100.f, FText::FromString(FString::Printf(TEXT("Copying %s into the project (%.0f MB)..."),
		*FPaths::GetCleanFilename(AwbFile), double(Size) / (1024.0 * 1024.0))));
	Task.MakeDialog(true);
	AcbImportBridgeLocal::FAwbCopyProgress Progress;
	Progress.Task = &Task;
	const uint32 Result = Files.Copy(*Dest, *Source, true, true, false, &Progress);
	if (Result != COPY_OK)
	{
		OutLog.Add(FString::Printf(TEXT("warning: copying the .awb to %s failed (%s)."), *Dest,
			Result == COPY_Canceled ? TEXT("cancelled") : TEXT("file in use or disk full?")));
		return false;
	}
	OutLog.Add(FString::Printf(TEXT("Copied %s to %s (%.1f MB)."), *FPaths::GetCleanFilename(AwbFile), *Dest, double(Size) / (1024.0 * 1024.0)));
	return true;
}

FString AcbImportBridge::FindCueSheetFolder(const FString& CueSheetName)
{
	FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Assets;
	Registry.Get().GetAssetsByClass(USoundAtomCueSheet::StaticClass()->GetFName(), Assets);
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetName.ToString().Equals(CueSheetName, ESearchCase::IgnoreCase))
		{
			return Asset.PackagePath.ToString();
		}
	}
	return FString();
}

bool AcbImportBridge::SyncAwbPakRule(const FString& SheetPackageName, TArray<FString>& OutLog)
{
	const UAcbEditorSettings* Settings = GetDefault<UAcbEditorSettings>();
	if (!Settings->bAddAwbFolderToPackaging || Settings->AwbDirectory.IsEmpty())
	{
		return false;
	}
	FString Dir = Settings->AwbDirectory;
	FPaths::NormalizeDirectoryName(Dir);
	Dir.RemoveFromStart(TEXT("/"));
	const FString SheetName = FPackageName::GetShortName(SheetPackageName);
	if (!FPaths::FileExists(FPaths::ProjectContentDir() / Dir / SheetName + TEXT(".awb")))
	{
		return false;
	}
	FString PakName;
	{
		const int32 Chunk = FindPackageChunk(SheetPackageName, OutLog);
		if (Chunk > 0)
		{
			PakName = FString::Printf(TEXT("pakchunk%d"), Chunk);
		}
	}
	EnsureStagedForPackaging(Dir, OutLog);
	return EnsureAwbPakRule(Dir, SheetName, PakName, OutLog);
}

int32 AcbImportBridge::SyncAllAwbPakRules(TArray<FString>& OutLog)
{
	FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> Sheets;
	Registry.Get().GetAssetsByClass(USoundAtomCueSheet::StaticClass()->GetFName(), Sheets);
	int32 Synced = 0;
	for (const FAssetData& Sheet : Sheets)
	{
		if (FPackageName::GetLongPackagePath(Sheet.PackageName.ToString()).StartsWith(TEXT("/Game")) && SyncAwbPakRule(Sheet.PackageName.ToString(), OutLog))
		{
			++Synced;
		}
	}
	OutLog.Add(FString::Printf(TEXT("Checked %d cue sheet(s); %d have a .awb in the project and a pak rule that follows their chunk."), Sheets.Num(), Synced));
	return Synced;
}

