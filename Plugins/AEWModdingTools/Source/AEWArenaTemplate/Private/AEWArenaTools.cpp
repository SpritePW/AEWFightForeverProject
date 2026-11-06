#include "AEWArenaTools.h"
#include "AEWArenaTemplateSettings.h"
#include "AEWTableUtils.h"
#include "AEWToolUI.h"
#include "AEWPackaging.h"
#include "AEWModdingSettings.h"
#include "Framework/Docking/TabManager.h"

#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "DataTableEditorUtils.h"
#include "ISettingsModule.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/UObjectHash.h"
#include "EngineUtils.h"
#include "Components/ChildActorComponent.h"
#include "ScopedTransaction.h"
#include "Misc/ScopedSlowTask.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "StaticMeshResources.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/DataTable.h"
#include "Engine/CompositeDataTable.h"
#include "Settings/ProjectPackagingSettings.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SNullWidget.h"
#include "EditorStyleSet.h"
#include "UObject/ObjectRedirector.h"
#include "Engine/LevelScriptBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEWArena, Log, All);

#define LOCTEXT_NAMESPACE "AEWArenaTools"

namespace
{
	const UAEWArenaTemplateSettings* Settings()
	{
		return GetDefault<UAEWArenaTemplateSettings>();
	}

	FString ObjectPathOf(const FString& PackageName)
	{
		return PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
	}

	FString NormDir(FString D)
	{
		D.TrimStartAndEndInline();
		D.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (D.EndsWith(TEXT("/")))
		{
			D.LeftChopInline(1);
		}
		return D;
	}

	bool IsUnder(const FString& Path, const FString& Dir)
	{
		return Path == Dir || Path.StartsWith(Dir + TEXT("/"));
	}

	UWorld* LoadWorld(const FString& PackageName)
	{
		return LoadObject<UWorld>(nullptr, *ObjectPathOf(PackageName));
	}

	void UnloadMaps(const TArray<FString>& PackageNames)
	{
		if (!GEditor)
		{
			return;
		}
		TSet<UPackage*> InUse;
		if (UWorld* Current = GEditor->GetEditorWorldContext().World())
		{
			InUse.Add(Current->GetOutermost());
			for (ULevel* Level : Current->GetLevels())
			{
				if (Level)
				{
					InUse.Add(Level->GetOutermost());
				}
			}
		}
		TArray<UPackage*> Packages;
		for (const FString& Name : PackageNames)
		{
			UPackage* Package = FindPackage(nullptr, *Name);
			if (Package && !InUse.Contains(Package) && !Package->IsDirty())
			{
				Packages.AddUnique(Package);
			}
		}
		if (Packages.Num() > 0)
		{
			GEditor->ResetTransaction(FText::FromString(TEXT("Arena maps unloaded")));
			UPackageTools::UnloadPackages(Packages);
		}
	}

	bool ShouldOwnShared(ULevelStreaming* LS)
	{
		if (!Cast<ULevelStreamingAlwaysLoaded>(LS))
		{
			return false;
		}
		const FString Package = LS->GetWorldAssetPackageName();
		const FString Name = FPackageName::GetShortName(Package);
		for (const FString& Keep : Settings()->KeepSharedSublevels)
		{
			if (Name.Equals(Keep.TrimStartAndEnd(), ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
		return FPackageName::DoesPackageExist(Package);
	}

	int32 CountActors(UWorld* World)
	{
		int32 Count = 0;
		if (World && World->PersistentLevel)
		{
			for (AActor* A : World->PersistentLevel->Actors)
			{
				Count += A ? 1 : 0;
			}
		}
		return Count;
	}

	void ReplaceRefsIn(UPackage* Package, const TMap<UObject*, UObject*>& Replace)
	{
		if (!Package || Replace.Num() == 0)
		{
			return;
		}
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects, true);
		for (UObject* Object : Objects)
		{
			FArchiveReplaceObjectRef<UObject> Ar(Object, Replace, false, true, true);
		}
	}

	int32 SwapActorClasses(UWorld* World, const TMap<UClass*, UClass*>& Classes, const TMap<UObject*, UObject*>& Replace)
	{
		int32 Swapped = 0;
		for (ULevel* Level : World ? World->GetLevels() : TArray<ULevel*>())
		{
			if (!Level)
			{
				continue;
			}
			TArray<AActor*> Old;
			for (AActor* A : Level->Actors)
			{
				if (A && Classes.Contains(A->GetClass()))
				{
					Old.Add(A);
				}
			}
			for (AActor* A : Old)
			{
				UClass* OldClass = A->GetClass();
				UClass* NewClass = Classes.FindChecked(OldClass);
				FActorSpawnParameters Params;
				Params.OverrideLevel = Level;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AActor* New = World->SpawnActor<AActor>(NewClass, A->GetActorTransform(), Params);
				if (!New)
				{
					UE_LOG(LogAEWArena, Warning, TEXT("could not respawn %s as %s"), *A->GetActorLabel(), *NewClass->GetName());
					continue;
				}
				for (TFieldIterator<FProperty> It(OldClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
				{
					FProperty* To = FindFProperty<FProperty>(NewClass, It->GetFName());
					if (To && To->SameType(*It) && !It->HasAnyPropertyFlags(CPF_Transient))
					{
						It->CopyCompleteValue(To->ContainerPtrToValuePtr<void>(New), It->ContainerPtrToValuePtr<void>(A));
					}
				}
				FArchiveReplaceObjectRef<UObject> Ar(New, Replace, false, true, true);
				New->SetActorLabel(A->GetActorLabel());
				New->SetFolderPath(A->GetFolderPath());
				New->Tags = A->Tags;
				New->SetActorTransform(A->GetActorTransform());
				New->PostEditChange();
				World->EditorDestroyActor(A, false);
				Level->MarkPackageDirty();
				++Swapped;
			}
		}
		return Swapped;
	}

	void ClearBuildData(UWorld* World)
	{
		if (World && World->PersistentLevel)
		{
			World->PersistentLevel->Modify();
			World->PersistentLevel->MapBuildData = nullptr;
		}
	}

	template <typename T>
	T* LoadSetting(const FSoftObjectPath& Path)
	{
		return Path.IsNull() ? nullptr : Cast<T>(Path.TryLoad());
	}

	using AEWTables::FindField;

	FName GetNameField(const UStruct* Struct, const uint8* Row, const TCHAR* Base)
	{
		if (FNameProperty* P = CastField<FNameProperty>(FindField(Struct, Base)))
		{
			return P->GetPropertyValue_InContainer(Row);
		}
		return NAME_None;
	}

	bool GetIntField(const UStruct* Struct, const uint8* Row, const TCHAR* Base, int64& Out)
	{
		return AEWTables::GetInt(Struct, Row, Base, Out);
	}

	bool SetIntField(const UStruct* Struct, uint8* Row, const TCHAR* Base, int64 Value)
	{
		if (AEWTables::SetInt(Struct, Row, Base, Value))
		{
			return true;
		}
		UE_LOG(LogAEWArena, Warning, TEXT("field %s not found in %s"), Base, *Struct->GetName());
		return false;
	}

	void SetBoolField(const UStruct* Struct, uint8* Row, const TCHAR* Base, bool Value)
	{
		if (FBoolProperty* P = CastField<FBoolProperty>(FindField(Struct, Base)))
		{
			P->SetPropertyValue_InContainer(Row, Value);
		}
	}

	void SetNameField(const UStruct* Struct, uint8* Row, const TCHAR* Base, FName Value)
	{
		if (FNameProperty* P = CastField<FNameProperty>(FindField(Struct, Base)))
		{
			P->SetPropertyValue_InContainer(Row, Value);
		}
	}

	void SetStrField(const UStruct* Struct, uint8* Row, const TCHAR* Base, const FString& Value)
	{
		if (FStrProperty* P = CastField<FStrProperty>(FindField(Struct, Base)))
		{
			P->SetPropertyValue_InContainer(Row, Value);
		}
	}

	bool SetByText(const UStruct* Struct, uint8* Row, const TCHAR* Base, const FString& Text)
	{
		FString Error;
		if (AEWTables::SetByText(Struct, Row, Base, Text, Error))
		{
			return true;
		}
		UE_LOG(LogAEWArena, Warning, TEXT("%s"), *Error);
		return false;
	}

	FString QuoteText(const FString& S)
	{
		return AEWTables::InvText(S);
	}

	using AEWTables::MakeUnlocked;

	TArray<UDataTable*> ParentsOf(UDataTable* Table)
	{
		return AEWTables::GetParents(Table);
	}

	using AEWTables::RefreshComposite;

	void RemoveStrayUexp(UPackage* Package)
	{
		if (!Package || Package->HasAnyPackageFlags(PKG_FilterEditorOnly))
		{
			return;
		}
		const FString Uexp = FPaths::ChangeExtension(FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), TEXT(".uexp"));
		if (IFileManager::Get().FileExists(*Uexp) && IFileManager::Get().Delete(*Uexp, false, true, true))
		{
			UE_LOG(LogAEWArena, Log, TEXT("removed leftover cooked %s"), *Uexp);
		}
	}

	bool PrepareArenaTables(UDataTable*& OutTable, UDataTable*& OutNewRows)
	{
		const UAEWArenaTemplateSettings* S = Settings();
		UDataTable* Table = LoadSetting<UDataTable>(S->ArenaTable);
		UDataTable* NewRows = LoadSetting<UDataTable>(S->ArenaNewTable);
		if (!Table || !Table->RowStruct)
		{
			UE_LOG(LogAEWArena, Error, TEXT("Arena Table not found: %s (Project Settings > Plugins > Arena Maker)"), *S->ArenaTable.ToString());
			return false;
		}
		if (!NewRows || NewRows->RowStruct != Table->RowStruct || Cast<UCompositeDataTable>(NewRows))
		{
			UE_LOG(LogAEWArena, Error, TEXT("New Arena Rows Table not found, not a plain table, or with a different row struct: %s"), *S->ArenaNewTable.ToString());
			return false;
		}
		if (Table != NewRows && !ParentsOf(Table).Contains(NewRows))
		{
			UE_LOG(LogAEWArena, Warning, TEXT("%s is not a parent of %s - the game won't see the new arena rows"), *NewRows->GetName(), *Table->GetName());
		}
		OutTable = Table;
		OutNewRows = NewRows;
		return true;
	}

	TSet<int64> UsedArenaIds(UUserDefinedEnum* Enum, UDataTable* Table)
	{
		TSet<int64> Used;
		if (Enum)
		{
			for (int32 i = 0; i < Enum->NumEnums(); ++i)
			{
				if (!Enum->GetNameByIndex(i).ToString().EndsWith(TEXT("_MAX")))
				{
					Used.Add(Enum->GetValueByIndex(i));
				}
			}
		}
		if (Table)
		{
			for (const TPair<FName, uint8*>& R : Table->GetRowMap())
			{
				const FString N = R.Key.ToString();
				if (N.IsNumeric())
				{
					Used.Add(FCString::Atoi64(*N));
				}
			}
		}
		return Used;
	}

	int64 PickUnlockId(UDataTable* Unlock, UDataTable* BaseUnlock)
	{
		const UAEWArenaTemplateSettings* S = Settings();
		TSet<int64> Used = AEWTables::CollectInts({ BaseUnlock, Unlock }, TEXT("UniqueID"));
		if (Unlock)
		{
			for (const TPair<FName, uint8*>& R : Unlock->GetRowMap())
			{
				const FString N = R.Key.ToString();
				if (N.StartsWith(TEXT("Item")) && N.Mid(4).IsNumeric())
				{
					Used.Add(FCString::Atoi64(*N.Mid(4)));
				}
			}
		}
		return AEWTables::FirstFree(Used, S->FirstUnlockID, S->LastUnlockID);
	}

	int64 AddUnlockRow(const FString& DisplayName, int64 SourceItemId, TArray<UPackage*>& ToSave, TArray<UObject*>& Ship)
	{
		const UAEWArenaTemplateSettings* S = Settings();
		UDataTable* Unlock = LoadSetting<UDataTable>(S->UnlockTable);
		UDataTable* BaseUnlock = LoadSetting<UDataTable>(S->BaseUnlockTable);
		if (!Unlock || !Unlock->GetRowStruct())
		{
			UE_LOG(LogAEWArena, Error, TEXT("Unlock Table not found: %s - the arena keeps ItemID 0"), *S->UnlockTable.ToString());
			return 0;
		}
		if (BaseUnlock && BaseUnlock != Unlock && Cast<UCompositeDataTable>(BaseUnlock) && !ParentsOf(BaseUnlock).Contains(Unlock))
		{
			UE_LOG(LogAEWArena, Warning, TEXT("%s is not a parent of %s - the game won't see the unlock row"), *Unlock->GetName(), *BaseUnlock->GetName());
		}

		const UScriptStruct* RS = Unlock->GetRowStruct();
		const int64 Id = PickUnlockId(Unlock, BaseUnlock);
		if (Id < 0)
		{
			UE_LOG(LogAEWArena, Error, TEXT("no free unlock ID in %d-%d"), S->FirstUnlockID, S->LastUnlockID);
			return 0;
		}

		const uint8* Template = nullptr;
		for (UDataTable* T : { BaseUnlock, Unlock })
		{
			if (T && T->GetRowStruct() == RS && !Template)
			{
				if (SourceItemId > 0)
				{
					Template = T->FindRowUnchecked(FName(*FString::Printf(TEXT("Item%lld"), SourceItemId)));
				}
				if (!Template && !S->UnlockTemplateRow.IsNone())
				{
					Template = T->FindRowUnchecked(S->UnlockTemplateRow);
				}
			}
		}

		const FName RowName = AEWTables::UnlockRowName(Id);
		Unlock->Modify();
		uint8* Row = FDataTableEditorUtils::AddRow(Unlock, RowName);
		if (!Row)
		{
			UE_LOG(LogAEWArena, Error, TEXT("could not add %s to %s"), *RowName.ToString(), *Unlock->GetName());
			return 0;
		}
		if (Template)
		{
			RS->CopyScriptStruct(Row, Template);
		}
		else
		{
			UE_LOG(LogAEWArena, Warning, TEXT("no unlock template row found - the new unlock row starts empty"));
		}
		SetIntField(RS, Row, TEXT("UniqueID"), Id);
		SetByText(RS, Row, TEXT("UnlockCategory"), TEXT("Arena"));
		SetByText(RS, Row, TEXT("DisplayText"), QuoteText(DisplayName));
		MakeUnlocked(RS, Row);
		Unlock->MarkPackageDirty();
		RefreshComposite(BaseUnlock);

		ToSave.AddUnique(Unlock->GetOutermost());
		if (BaseUnlock)
		{
			ToSave.AddUnique(BaseUnlock->GetOutermost());
		}
		Ship.AddUnique(Unlock);
		if (BaseUnlock)
		{
			Ship.AddUnique(BaseUnlock);
		}
		UE_LOG(LogAEWArena, Log, TEXT("%s: unlock row %s (default unlocked)"), *Unlock->GetName(), *RowName.ToString());
		return Id;
	}

	FString LeadingNumber(const FString& Name)
	{
		int32 i = 0;
		while (i < Name.Len() && FChar::IsDigit(Name[i]))
		{
			++i;
		}
		return (i > 0 && i < Name.Len() && Name[i] == TEXT('_')) ? Name.Left(i) : FString();
	}

	FString StripNumber(const FString& Name)
	{
		const FString Num = LeadingNumber(Name);
		return Num.IsEmpty() ? Name : Name.Mid(Num.Len() + 1);
	}

	FString SwapNumber(const FString& Suffix, const FString& From, const FString& To)
	{
		if (From.IsEmpty() || To.IsEmpty())
		{
			return Suffix;
		}
		TArray<FString> Parts;
		Suffix.ParseIntoArray(Parts, TEXT("_"), false);
		for (FString& Part : Parts)
		{
			if (Part == From)
			{
				Part = To;
			}
			else if (Part.Len() > From.Len() && Part.EndsWith(From) && FChar::IsAlpha(Part[Part.Len() - From.Len() - 1]))
			{
				Part = Part.LeftChop(From.Len()) + To;
			}
		}
		return FString::Join(Parts, TEXT("_"));
	}

	bool FixLevelName(FString& V, const TMap<FString, FString>& Names, const TMap<FString, FString>& Packages)
	{
		if (V.IsEmpty())
		{
			return false;
		}
		if (const FString* N = Names.Find(V))
		{
			V = *N;
			return true;
		}
		for (const TPair<FString, FString>& P : Packages)
		{
			if (V == P.Key)
			{
				V = P.Value;
				return true;
			}
			if (V.StartsWith(P.Key + TEXT(".")))
			{
				V = P.Value + TEXT(".") + FPackageName::GetShortName(P.Value);
				return true;
			}
		}
		bool bChanged = false;
		if (V.Contains(TEXT("\"")))
		{
			for (const TPair<FString, FString>& N : Names)
			{
				const FString From = TEXT("\"") + N.Key + TEXT("\"");
				if (V.Contains(From, ESearchCase::CaseSensitive))
				{
					V.ReplaceInline(*From, *(TEXT("\"") + N.Value + TEXT("\"")), ESearchCase::CaseSensitive);
					bChanged = true;
				}
			}
			for (const TPair<FString, FString>& P : Packages)
			{
				const FString From = TEXT("\"") + P.Key + TEXT(".") + FPackageName::GetShortName(P.Key) + TEXT("\"");
				if (V.Contains(From, ESearchCase::CaseSensitive))
				{
					V.ReplaceInline(*From, *(TEXT("\"") + P.Value + TEXT(".") + FPackageName::GetShortName(P.Value) + TEXT("\"")), ESearchCase::CaseSensitive);
					bChanged = true;
				}
			}
		}
		return bChanged;
	}

	int32 FixLevelBlueprint(UWorld* World, const TMap<FString, FString>& Names, const TMap<FString, FString>& Packages)
	{
		if (!World || !World->PersistentLevel || !GetDefault<UAEWArenaTemplateSettings>()->bFixLevelBlueprints)
		{
			return 0;
		}
		ULevelScriptBlueprint* BP = World->PersistentLevel->GetLevelScriptBlueprint(true);
		if (!BP)
		{
			return 0;
		}
		int32 Changed = 0;
		TArray<UEdGraph*> Graphs;
		BP->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node)
				{
					continue;
				}
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin || Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() > 0)
					{
						continue;
					}
					FString V = Pin->DefaultValue;
					if (FixLevelName(V, Names, Packages))
					{
						UE_LOG(LogAEWArena, Log, TEXT("%s: %s.%s \"%s\" -> \"%s\""), *World->GetName(),
							*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Pin->PinName.ToString(), *Pin->DefaultValue, *V);
						Node->Modify();
						Pin->DefaultValue = V;
						++Changed;
					}
				}
			}
		}
		for (FBPVariableDescription& Var : BP->NewVariables)
		{
			FString V = Var.DefaultValue;
			if (FixLevelName(V, Names, Packages))
			{
				UE_LOG(LogAEWArena, Log, TEXT("%s: variable %s default -> %s"), *World->GetName(), *Var.VarName.ToString(), *V);
				Var.DefaultValue = V;
				++Changed;
			}
		}
		if (Changed > 0)
		{
			BP->Modify();
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);
			World->MarkPackageDirty();
		}
		return Changed;
	}

	TMap<FString, FString> ShortNamesOf(const TMap<FString, FString>& Packages)
	{
		TMap<FString, FString> Names;
		for (const TPair<FString, FString>& P : Packages)
		{
			Names.Add(FPackageName::GetShortName(P.Key), FPackageName::GetShortName(P.Value));
		}
		return Names;
	}
}

namespace
{
	FString ShortLevel(const FString& Level)
	{
		FString Package = Level;
		Package.Split(TEXT("."), &Package, nullptr);
		return FPackageName::GetShortName(Package);
	}

	FString ToLevelPath(const FString& Level)
	{
		if (Level.StartsWith(TEXT("/")))
		{
			return Level;
		}
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Worlds;
		AR.GetAssetsByClass(UWorld::StaticClass()->GetFName(), Worlds);
		for (const FAssetData& W : Worlds)
		{
			if (W.AssetName.ToString() == Level)
			{
				return W.PackageName.ToString();
			}
		}
		return FString();
	}

	FString GetFieldString(const UStruct* Struct, const uint8* Row, const TCHAR* Base)
	{
		FProperty* P = FindField(Struct, Base);
		if (FStrProperty* Str = CastField<FStrProperty>(P))
		{
			return Str->GetPropertyValue_InContainer(Row);
		}
		if (FTextProperty* Text = CastField<FTextProperty>(P))
		{
			return Text->GetPropertyValue_InContainer(Row).ToString();
		}
		if (FNameProperty* Name = CastField<FNameProperty>(P))
		{
			return Name->GetPropertyValue_InContainer(Row).ToString();
		}
		return FString();
	}

	bool SetFieldString(const UStruct* Struct, uint8* Row, const TCHAR* Base, const FString& Value)
	{
		FProperty* P = FindField(Struct, Base);
		if (FStrProperty* Str = CastField<FStrProperty>(P))
		{
			Str->SetPropertyValue_InContainer(Row, Value);
			return true;
		}
		if (FTextProperty* Text = CastField<FTextProperty>(P))
		{
			Text->SetPropertyValue_InContainer(Row, FText::AsCultureInvariant(Value));
			return true;
		}
		if (FNameProperty* Name = CastField<FNameProperty>(P))
		{
			Name->SetPropertyValue_InContainer(Row, FName(*Value));
			return true;
		}
		UE_LOG(LogAEWArena, Warning, TEXT("field %s not found in %s"), Base, *Struct->GetName());
		return false;
	}

	bool ApplyDisplayName(UUserDefinedEnum* Enum, const UStruct* RS, uint8* Row, int64 ArenaId, FString Shown, bool bAdoptEnumText = false)
	{
		const UAEWArenaTemplateSettings* S = Settings();
		if (!S->bLocalizedDisplayName || !Enum || !Row)
		{
			return false;
		}
		const int32 Index = Enum->GetIndexByValue(ArenaId);
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogAEWArena, Warning, TEXT("arena %lld has no %s entry - display name not set"), ArenaId, *Enum->GetName());
			return false;
		}
		const FString Entry = Enum->GetNameStringByIndex(Index);
		FString Namespace = S->DisplayNameNamespace.IsEmpty() ? FString(TEXT("AEWModArena")) : S->DisplayNameNamespace;
#if USE_STABLE_LOCALIZATION_KEYS
		Namespace = TextNamespaceUtil::BuildFullNamespace(Namespace, TextNamespaceUtil::EnsurePackageNamespace(Enum), true);
#endif
		const FString Key = TEXT("ARENA_") + Entry;
		bool bChanged = false;
		const FText* Old = Enum->DisplayNameMap.Find(FName(*Entry));
		if (bAdoptEnumText && Old && !Old->IsEmpty() && Old->ToString() != Entry
			&& FTextInspector::GetKey(*Old).Get(FString()) != Key)
		{
			const FString Typed = Old->ToString();
			if (Typed != Shown)
			{
				UE_LOG(LogAEWArena, Log, TEXT("arena %lld: using the name typed in %s, \"%s\""), ArenaId, *Enum->GetName(), *Typed);
				Shown = Typed;
				SetFieldString(RS, Row, TEXT("ArenaName"), Shown);
				bChanged = true;
			}
		}
		if (Shown.IsEmpty())
		{
			return bChanged;
		}
		if (!Old || Old->ToString() != Shown
			|| FTextInspector::GetNamespace(*Old).Get(FString()) != Namespace
			|| FTextInspector::GetKey(*Old).Get(FString()) != Key)
		{
			Enum->Modify();
			Enum->DisplayNameMap.Add(FName(*Entry), FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*Shown, *Namespace, *Key));
			Enum->MarkPackageDirty();
			bChanged = true;
		}
		const FString CookedNamespace = TextNamespaceUtil::StripPackageNamespace(Namespace);
		if (GetFieldString(RS, Row, TEXT("ST_Name")) != CookedNamespace)
		{
			bChanged |= SetFieldString(RS, Row, TEXT("ST_Name"), CookedNamespace);
		}
		if (GetFieldString(RS, Row, TEXT("ST_Key")) != Key)
		{
			bChanged |= SetFieldString(RS, Row, TEXT("ST_Key"), Key);
		}
		if (bChanged)
		{
			UE_LOG(LogAEWArena, Log, TEXT("arena %lld: display name \"%s\" (%s / %s)"), ArenaId, *Shown, *Namespace, *Key);
		}
		return bChanged;
	}

	FString LevelValue(const FString& Level)
	{
		const FString Path = ToLevelPath(Level);
		if (Path.IsEmpty())
		{
			UE_LOG(LogAEWArena, Warning, TEXT("map %s not found - LevelName keeps the short name, which the game can't find for a mod map"), *Level);
			return Level;
		}
		return Path;
	}
}

FString UAEWArenaTools::MakeArenaMapName(int32 ArenaId, const FString& Name)
{
	const FString Base = StripNumber(Name.TrimStartAndEnd());
	return ArenaId >= 0 ? FString::Printf(TEXT("%d_%s"), ArenaId, *Base) : Base;
}

int32 UAEWArenaTools::GetNextArenaId()
{
	UUserDefinedEnum* Enum = LoadSetting<UUserDefinedEnum>(Settings()->ArenaEnum);
	if (!Enum)
	{
		return -1;
	}
	const TSet<int64> Used = UsedArenaIds(Enum, LoadSetting<UDataTable>(Settings()->ArenaTable));
	int64 Max = -1;
	for (int64 V : Used)
	{
		Max = FMath::Max(Max, V);
	}
	return (int32)(Max + 1);
}

bool UAEWArenaTools::IsArenaIdFree(int32 ArenaId)
{
	if (ArenaId < 0 || ArenaId > 254)
	{
		return false;
	}
	UUserDefinedEnum* Enum = LoadSetting<UUserDefinedEnum>(Settings()->ArenaEnum);
	return !UsedArenaIds(Enum, LoadSetting<UDataTable>(Settings()->ArenaTable)).Contains(ArenaId);
}

TArray<FString> UAEWArenaTools::GetArenaLevelNames()
{
	TArray<FString> Out;
	if (UDataTable* Table = LoadSetting<UDataTable>(Settings()->ArenaTable))
	{
		for (const TPair<FName, uint8*>& R : Table->GetRowMap())
		{
			Out.Add(GetNameField(Table->GetRowStruct(), R.Value, TEXT("LevelName")).ToString());
		}
	}
	return Out;
}

bool UAEWArenaTools::AllowCooking(const FString& PackageName)
{
	return AEWPackaging::AllowCooking(PackageName);
}

int32 UAEWArenaTools::RegisterArena(const FString& LevelName, const FString& CopyFromLevelName, const FString& DisplayName,
	const FString& LabelFolder, int32 ChunkId, int32 ArenaId)
{
	const UAEWArenaTemplateSettings* S = Settings();
	UUserDefinedEnum* Enum = LoadSetting<UUserDefinedEnum>(S->ArenaEnum);
	if (!Enum)
	{
		UE_LOG(LogAEWArena, Error, TEXT("Arena ID Enum not found: %s (Project Settings > Plugins > Arena Maker)"), *S->ArenaEnum.ToString());
		return -1;
	}
	UDataTable* Table = nullptr;
	UDataTable* NewRows = nullptr;
	if (!PrepareArenaTables(Table, NewRows))
	{
		return -1;
	}
	const UScriptStruct* RS = Table->GetRowStruct();
	const FString Short = ShortLevel(LevelName);
	const FString Stored = LevelValue(LevelName);
	const FName LevelFName(*Stored);

	for (const TPair<FName, uint8*>& R : Table->GetRowMap())
	{
		if (ShortLevel(GetNameField(RS, R.Value, TEXT("LevelName")).ToString()) == Short)
		{
			UE_LOG(LogAEWArena, Log, TEXT("%s is already registered as arena row %s"), *Short, *R.Key.ToString());
			return FCString::Atoi(*R.Key.ToString());
		}
	}

	const uint8* SrcRow = nullptr;
	FName SrcRowName;
	for (const TPair<FName, uint8*>& R : Table->GetRowMap())
	{
		if (ShortLevel(GetNameField(RS, R.Value, TEXT("LevelName")).ToString()) == ShortLevel(CopyFromLevelName))
		{
			SrcRow = R.Value;
			SrcRowName = R.Key;
			break;
		}
	}
	if (!SrcRow)
	{
		SrcRowName = FName(TEXT("0"));
		SrcRow = Table->FindRowUnchecked(SrcRowName);
		UE_LOG(LogAEWArena, Warning, TEXT("no game arena with LevelName %s - copying row 0"), *CopyFromLevelName);
	}
	if (!SrcRow)
	{
		UE_LOG(LogAEWArena, Error, TEXT("no arena row to copy"));
		return -1;
	}

	const TSet<int64> Used = UsedArenaIds(Enum, Table);
	int64 NewValue = ArenaId;
	if (NewValue < 0)
	{
		NewValue = 0;
		for (int64 V : Used)
		{
			NewValue = FMath::Max(NewValue, V + 1);
		}
	}
	if (NewValue > 254 || Used.Contains(NewValue))
	{
		UE_LOG(LogAEWArena, Error, TEXT("arena ID %lld is in use or out of range (0-254)"), NewValue);
		return -1;
	}

	const FString Shown = DisplayName.IsEmpty() ? StripNumber(Short) : DisplayName;
	const FString ShortEntry = FString::Printf(TEXT("NewEnumerator%lld"), NewValue);
	const FName FullEntry(*FString::Printf(TEXT("%s::%s"), *Enum->GetName(), *ShortEntry));
	const FName RowName(*LexToString(NewValue));

	TArray<UPackage*> ToSave;
	TArray<UObject*> Ship = { Table, Enum, NewRows };
	for (UDataTable* P : ParentsOf(Table))
	{
		Ship.AddUnique(P);
	}

	int64 SrcItemId = 0;
	GetIntField(RS, SrcRow, TEXT("ItemID"), SrcItemId);
	const int64 UnlockId = S->bCreateUnlock ? AddUnlockRow(Shown, SrcItemId, ToSave, Ship) : 0;

	TArray<TPair<FName, int64>> Pairs;
	for (int32 i = 0; i < Enum->NumEnums(); ++i)
	{
		const FName N = Enum->GetNameByIndex(i);
		if (!N.ToString().EndsWith(TEXT("_MAX")))
		{
			Pairs.Emplace(N, Enum->GetValueByIndex(i));
		}
	}
	Pairs.Emplace(FullEntry, NewValue);
	Enum->Modify();
	Enum->SetEnums(Pairs, UEnum::ECppForm::Namespaced);
	Enum->DisplayNameMap.Add(FName(*ShortEntry), FText::FromString(Shown));
#if WITH_EDITORONLY_DATA
	Enum->UniqueNameIndex = FMath::Max<uint32>(Enum->UniqueNameIndex, (uint32)NewValue + 1);
#endif
	Enum->MarkPackageDirty();
	ToSave.AddUnique(Enum->GetOutermost());

	NewRows->Modify();
	uint8* NewRow = FDataTableEditorUtils::AddRow(NewRows, RowName);
	if (!NewRow)
	{
		UE_LOG(LogAEWArena, Error, TEXT("could not add row %s to %s"), *RowName.ToString(), *NewRows->GetName());
		return -1;
	}
	RS->CopyScriptStruct(NewRow, SrcRow);
	SetIntField(RS, NewRow, TEXT("ArenaId"), NewValue);
	SetNameField(RS, NewRow, TEXT("LevelName"), LevelFName);
	SetStrField(RS, NewRow, TEXT("ArenaName"), Shown);
	SetBoolField(RS, NewRow, TEXT("IsSelectable"), true);
	SetBoolField(RS, NewRow, TEXT("IsSelectableDev"), true);
	SetIntField(RS, NewRow, TEXT("ItemID"), UnlockId);
	ApplyDisplayName(Enum, RS, NewRow, NewValue, Shown);
	NewRows->MarkPackageDirty();
	RefreshComposite(Table);
	ToSave.AddUnique(NewRows->GetOutermost());
	ToSave.AddUnique(Table->GetOutermost());

	TArray<FString> PackagingLog;
	AEWPackaging::LabelSharedTables(Ship, ToSave, &PackagingLog);
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
	for (const FString& Line : PackagingLog)
	{
		UE_LOG(LogAEWArena, Log, TEXT("%s"), *Line);
	}
	RemoveStrayUexp(Enum->GetOutermost());

	UE_LOG(LogAEWArena, Log, TEXT("registered %s as %s: row %s in %s (copied from row %s), unlock ID %lld"),
		*Stored, *FullEntry.ToString(), *RowName.ToString(), *NewRows->GetName(), *SrcRowName.ToString(), UnlockId);
	return (int32)NewValue;
}

TArray<FString> UAEWArenaTools::FindTemplates(const FString& RootPath)
{
	const FString Root = NormDir(RootPath.IsEmpty() ? Settings()->TemplateRoot.Path : RootPath);
	TArray<FString> Out;
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(FName(*Root), Assets, true);
	for (const FAssetData& A : Assets)
	{
		if (A.AssetClass == UWorld::StaticClass()->GetFName() && A.AssetName.ToString().EndsWith(TEXT("_Template")))
		{
			Out.Add(A.PackageName.ToString());
		}
	}
	Out.Sort();
	if (RootPath.IsEmpty())
	{
		TArray<FString> Mods;
		for (const FString& Level : GetArenaLevelNames())
		{
			FString Pkg = Level;
			Pkg.Split(TEXT("."), &Pkg, nullptr);
			if (Pkg.StartsWith(TEXT("/")) && !IsUnder(FPackageName::GetLongPackagePath(Pkg), Root) && FPackageName::DoesPackageExist(Pkg))
			{
				Mods.AddUnique(Pkg);
			}
		}
		Mods.Sort();
		Out.Append(Mods);
	}
	return Out;
}

FString UAEWArenaTools::CreateArenaFromTemplate(const FString& TemplateMap, const FString& DestFolder, const FString& InName,
	bool bCreateLabel, int32 ChunkId, bool bAddToMapsToCook, bool bOpenAfter, bool bRegisterArena, const FString& DisplayName, int32 ArenaId)
{
	const FString TplPkg = FPackageName::ObjectPathToPackageName(TemplateMap);
	const FString TplDir = FPackageName::GetLongPackagePath(TplPkg);
	const FString TplName = FPackageName::GetShortName(TplPkg);
	const FString Parent = NormDir(DestFolder.IsEmpty() ? Settings()->DefaultParentFolder.Path : DestFolder);

	int32 Id = -1;
	if (bRegisterArena)
	{
		Id = ArenaId >= 0 ? ArenaId : GetNextArenaId();
		if (!IsArenaIdFree(Id))
		{
			UE_LOG(LogAEWArena, Error, TEXT("arena ID %d is in use or out of range (0-254)"), Id);
			return FString();
		}
	}
	const FString NewName = bRegisterArena ? MakeArenaMapName(Id, InName) : InName.TrimStartAndEnd();
	const FString DestDir = Parent + TEXT("/") + NewName;
	const FString NewPkg = DestDir + TEXT("/") + NewName;
	const FString TplNum = LeadingNumber(TplName);
	const FString NewNum = Id >= 0 ? LexToString(Id) : FString();

	if (StripNumber(NewName).IsEmpty() || NewName.Contains(TEXT(" ")) || !FPackageName::IsValidLongPackageName(NewPkg))
	{
		UE_LOG(LogAEWArena, Error, TEXT("invalid name/folder: %s"), *NewPkg);
		return FString();
	}
	if (FPackageName::DoesPackageExist(NewPkg))
	{
		UE_LOG(LogAEWArena, Error, TEXT("%s already exists"), *NewPkg);
		return FString();
	}
	if (bRegisterArena)
	{
		if (GetArenaLevelNames().ContainsByPredicate([&NewName](const FString& L) { return ShortLevel(L).Equals(NewName, ESearchCase::IgnoreCase); }))
		{
			UE_LOG(LogAEWArena, Error, TEXT("%s is already an arena level name - pick another name"), *NewName);
			return FString();
		}
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Worlds;
		AR.GetAssetsByClass(UWorld::StaticClass()->GetFName(), Worlds);
		for (const FAssetData& W : Worlds)
		{
			if (W.AssetName.ToString().Equals(NewName, ESearchCase::IgnoreCase))
			{
				UE_LOG(LogAEWArena, Error, TEXT("a map named %s already exists (%s) - the game finds arena maps by name only"), *NewName, *W.PackageName.ToString());
				return FString();
			}
		}
	}

	if (GEditor && GEditor->GetEditorWorldContext().World() &&
		GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName().StartsWith(TplDir))
	{
		UEditorLoadingAndSavingUtils::NewBlankMap(false);
	}

	UWorld* TplWorld = LoadWorld(TplPkg);
	if (!TplWorld)
	{
		UE_LOG(LogAEWArena, Error, TEXT("template not found: %s"), *TplPkg);
		return FString();
	}

	TMap<FString, FString> Renames;
	TSet<FString> Shared;
	for (ULevelStreaming* LS : TplWorld->GetStreamingLevels())
	{
		if (!LS)
		{
			continue;
		}
		const FString Sub = LS->GetWorldAssetPackageName();
		if (FPackageName::GetLongPackagePath(Sub) != TplDir)
		{
			if (!ShouldOwnShared(LS))
			{
				continue;
			}
			Shared.Add(Sub);
		}
		const FString SubName = FPackageName::GetShortName(Sub);
		const FString Suffix = SwapNumber(SubName.StartsWith(TplName) ? SubName.Mid(TplName.Len()) : TEXT("_") + SubName, TplNum, NewNum);
		const FString NewSub = DestDir + TEXT("/") + NewName + Suffix;
		if (FPackageName::DoesPackageExist(NewSub))
		{
			UE_LOG(LogAEWArena, Error, TEXT("%s already exists"), *NewSub);
			return FString();
		}
		Renames.Add(Sub, NewSub);
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TArray<UPackage*> ToSave;
	TArray<FString> NewMaps;
	TArray<UWorld*> CreatedWorlds;
	TArray<FString> LoadedSubs;
	TArray<FString> KeptShared;

	for (const TPair<FString, FString>& R : Renames)
	{
		LoadedSubs.Add(R.Key);
		UWorld* Src = LoadWorld(R.Key);
		if (!Src)
		{
			UE_LOG(LogAEWArena, Warning, TEXT("sublevel %s not found - skipped"), *R.Key);
			if (Shared.Contains(R.Key))
			{
				KeptShared.Add(R.Key);
			}
			continue;
		}
		if (Shared.Contains(R.Key) && CountActors(Src) <= 2)
		{
			UE_LOG(LogAEWArena, Warning, TEXT("shared sublevel %s is an empty proxy - kept shared (run the template importer again to fill it, then reopen the arena)"), *R.Key);
			KeptShared.Add(R.Key);
			continue;
		}
		UWorld* Dup = Cast<UWorld>(AssetTools.DuplicateAsset(FPackageName::GetShortName(R.Value), DestDir, Src));
		if (!Dup)
		{
			UE_LOG(LogAEWArena, Error, TEXT("could not duplicate %s"), *R.Key);
			return FString();
		}
		ClearBuildData(Dup);
		CreatedWorlds.Add(Dup);
		ToSave.Add(Dup->GetOutermost());
		NewMaps.Add(R.Value);
		UE_LOG(LogAEWArena, Log, TEXT("  %s -> %s%s"), *FPackageName::GetShortName(R.Key), *FPackageName::GetShortName(R.Value), Shared.Contains(R.Key) ? TEXT(" (own copy of a shared sublevel)") : TEXT(""));
	}
	for (const FString& Key : KeptShared)
	{
		Renames.Remove(Key);
	}

	UWorld* NewWorld = Cast<UWorld>(AssetTools.DuplicateAsset(NewName, DestDir, TplWorld));
	if (!NewWorld)
	{
		UE_LOG(LogAEWArena, Error, TEXT("could not duplicate %s"), *TplPkg);
		return FString();
	}
	ClearBuildData(NewWorld);
	int32 Repointed = 0;
	for (ULevelStreaming* LS : NewWorld->GetStreamingLevels())
	{
		if (!LS)
		{
			continue;
		}
		if (const FString* Target = Renames.Find(LS->GetWorldAssetPackageName()))
		{
			LS->Modify();
			LS->SetWorldAssetByPackageName(FName(**Target));
			++Repointed;
		}
	}
	NewWorld->MarkPackageDirty();
	ToSave.Insert(NewWorld->GetOutermost(), 0);
	NewMaps.Insert(NewPkg, 0);

	TMap<UObject*, UObject*> Replace;
	TMap<UClass*, UClass*> ClassSwaps;
	{
		IAssetRegistry& FolderAR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> FolderAssets;
		FolderAR.GetAssetsByPath(FName(*TplDir), FolderAssets, true);
		TArray<UBlueprint*> DupBlueprints;
		for (const FAssetData& A : FolderAssets)
		{
			if (A.AssetClass == UWorld::StaticClass()->GetFName() || A.AssetClass == UMapBuildDataRegistry::StaticClass()->GetFName() || A.AssetClass == UPrimaryAssetLabel::StaticClass()->GetFName())
			{
				continue;
			}
			UObject* Src = A.GetAsset();
			if (!Src)
			{
				continue;
			}
			const FString SubPath = A.PackagePath.ToString().Mid(TplDir.Len());
			const FString DupName = A.AssetName.ToString().Replace(*TplName, *NewName);
			UObject* Dup = AssetTools.DuplicateAsset(DupName, DestDir + SubPath, Src);
			if (!Dup)
			{
				UE_LOG(LogAEWArena, Warning, TEXT("could not copy %s"), *A.PackageName.ToString());
				continue;
			}
			Replace.Add(Src, Dup);
			UBlueprint* SrcBP = Cast<UBlueprint>(Src);
			UBlueprint* DupBP = Cast<UBlueprint>(Dup);
			if (SrcBP && DupBP && SrcBP->GeneratedClass && DupBP->GeneratedClass)
			{
				Replace.Add(SrcBP->GeneratedClass, DupBP->GeneratedClass);
				ClassSwaps.Add(SrcBP->GeneratedClass, DupBP->GeneratedClass);
				DupBlueprints.Add(DupBP);
			}
			ToSave.Add(Dup->GetOutermost());
			UE_LOG(LogAEWArena, Log, TEXT("  %s -> %s"), *A.AssetName.ToString(), *DupName);
		}
		for (UPackage* Package : TArray<UPackage*>(ToSave))
		{
			ReplaceRefsIn(Package, Replace);
		}
		for (UBlueprint* BP : DupBlueprints)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);
		}
	}
	UE_LOG(LogAEWArena, Log, TEXT("%s -> %s: %d own sublevels copied and re-pointed"), *TplPkg, *NewPkg, Repointed);

	{
		TMap<FString, FString> PackageMap = Renames;
		PackageMap.Add(TplPkg, NewPkg);
		TMap<FString, FString> Names = ShortNamesOf(PackageMap);
		if (TplName.EndsWith(TEXT("_Template")))
		{
			const FString Base = TplName.LeftChop(9);
			for (const TPair<FString, FString>& R : Renames)
			{
				const FString SubName = FPackageName::GetShortName(R.Key);
				if (SubName.StartsWith(TplName))
				{
					Names.FindOrAdd(Base + SubName.Mid(TplName.Len()), FPackageName::GetShortName(R.Value));
				}
			}
		}
		CreatedWorlds.Insert(NewWorld, 0);
		int32 Fixed = 0;
		for (UWorld* W : CreatedWorlds)
		{
			Fixed += FixLevelBlueprint(W, Names, PackageMap);
		}
		if (Fixed > 0)
		{
			UE_LOG(LogAEWArena, Log, TEXT("Level Blueprints: %d level name(s) updated"), Fixed);
		}
	}

	if (bCreateLabel)
	{
		TArray<UObject*> MapObjects;
		for (const FString& M : NewMaps)
		{
			if (UWorld* World = FindObject<UWorld>(nullptr, *ObjectPathOf(M)))
			{
				MapObjects.Add(World);
			}
		}
		TArray<FString> PackagingLog;
		AEWPackaging::UpdateLabel(DestDir + TEXT("/PAL_") + NewName, ChunkId, MapObjects, true, false, ToSave, &PackagingLog);
		for (const FString& Line : PackagingLog)
		{
			UE_LOG(LogAEWArena, Log, TEXT("%s"), *Line);
		}
	}

	UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
	{
		TArray<FString> Loaded = NewMaps;
		Loaded.Add(TplPkg);
		Loaded.Append(LoadedSubs);
		TplWorld = nullptr;
		NewWorld = nullptr;
		CreatedWorlds.Reset();
		UnloadMaps(Loaded);
	}

	if (bAddToMapsToCook)
	{
		UProjectPackagingSettings* PS = GetMutableDefault<UProjectPackagingSettings>();
		for (const FString& M : NewMaps)
		{
			if (!PS->MapsToCook.ContainsByPredicate([&M](const FFilePath& F) { return F.FilePath == M; }))
			{
				FFilePath FP;
				FP.FilePath = M;
				PS->MapsToCook.Add(FP);
			}
		}
		PS->SaveConfig();
		PS->UpdateDefaultConfigFile();
	}

	if (bRegisterArena)
	{
		const FString CopyFrom = TplName.EndsWith(TEXT("_Template")) ? TplName.LeftChop(9) : TplName;
		const int32 Registered = RegisterArena(NewPkg, CopyFrom, DisplayName.IsEmpty() ? StripNumber(NewName) : DisplayName, Parent, ChunkId, Id);
		if (Registered < 0)
		{
			UE_LOG(LogAEWArena, Error, TEXT("the maps were created, but the arena could not be added to the arena list"));
		}
	}

	if (bOpenAfter || ClassSwaps.Num() > 0)
	{
		FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(NewPkg, FPackageName::GetMapPackageExtension()), false, true);
	}
	if (ClassSwaps.Num() > 0 && GEditor)
	{
		UWorld* Opened = GEditor->GetEditorWorldContext().World();
		const int32 Swapped = SwapActorClasses(Opened, ClassSwaps, Replace);
		if (Swapped > 0)
		{
			UE_LOG(LogAEWArena, Log, TEXT("%d actor(s) now use the new arena's own blueprints"), Swapped);
			TArray<UPackage*> Dirty;
			for (ULevel* Level : Opened->GetLevels())
			{
				if (Level && Level->GetOutermost()->IsDirty())
				{
					Dirty.Add(Level->GetOutermost());
				}
			}
			UEditorLoadingAndSavingUtils::SavePackages(Dirty, false);
		}
		if (!bOpenAfter)
		{
			UEditorLoadingAndSavingUtils::NewBlankMap(false);
		}
	}
	UE_LOG(LogAEWArena, Log, TEXT("done: %s. Next: check the Level Blueprint, mark the lighting scenarios, build lighting."), *NewPkg);
	return NewPkg;
}

namespace
{
	bool GArenaBusy = false;

	TSet<FName> ExistingWorldNames()
	{
		TSet<FName> Names;
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Worlds;
		AR.GetAssetsByClass(UWorld::StaticClass()->GetFName(), Worlds);
		for (const FAssetData& W : Worlds)
		{
			Names.Add(W.AssetName);
		}
		return Names;
	}

	int64 RowId(FName Row)
	{
		const FString S = Row.ToString();
		return S.IsNumeric() ? FCString::Atoi64(*S) : -1;
	}

	int64 FirstModId(UDataTable* Table, UDataTable* NewRows)
	{
		int64 Max = -1;
		for (const TPair<FName, uint8*>& R : Table->GetRowMap())
		{
			if (!NewRows->GetRowMap().Contains(R.Key))
			{
				Max = FMath::Max(Max, RowId(R.Key));
			}
		}
		for (UDataTable* P : ParentsOf(Table))
		{
			if (P == NewRows)
			{
				continue;
			}
			for (const TPair<FName, uint8*>& R : P->GetRowMap())
			{
				Max = FMath::Max(Max, RowId(R.Key));
			}
		}
		return Max + 1;
	}

	bool StartsWithLevel(const FString& Name, const FString& Level)
	{
		return Name.StartsWith(Level) && (Name.Len() == Level.Len() || Name[Level.Len()] == TEXT('_'));
	}

	void RebuildEnum(UUserDefinedEnum* Enum, const TSet<int64>& Removed, const TMap<int64, int64>& Moves)
	{
		const FString Prefix = Enum->GetName() + TEXT("::");
		const TMap<FName, FText> OldDisplay = Enum->DisplayNameMap;
		TArray<TPair<FName, int64>> Pairs;
		TMap<FName, FText> NewDisplay;
		TSet<FName> Taken;
		for (int32 i = 0; i < Enum->NumEnums(); ++i)
		{
			const FString Short = Enum->GetNameStringByIndex(i);
			if (Short.EndsWith(TEXT("_MAX")))
			{
				continue;
			}
			const int64 V = Enum->GetValueByIndex(i);
			if (Removed.Contains(V))
			{
				continue;
			}
			FName ShortName(*Short);
			int64 NV = V;
			if (const int64* M = Moves.Find(V))
			{
				NV = *M;
				if (Short.StartsWith(TEXT("NewEnumerator")))
				{
					ShortName = FName(*FString::Printf(TEXT("NewEnumerator%lld"), NV));
				}
			}
			if (Taken.Contains(ShortName))
			{
				ShortName = FName(*Short);
			}
			Taken.Add(ShortName);
			Pairs.Emplace(FName(*(Prefix + ShortName.ToString())), NV);
			if (const FText* T = OldDisplay.Find(FName(*Short)))
			{
				NewDisplay.Add(ShortName, *T);
			}
		}
		Pairs.Sort([](const TPair<FName, int64>& A, const TPair<FName, int64>& B) { return A.Value < B.Value; });
		Enum->Modify();
		Enum->SetEnums(Pairs, UEnum::ECppForm::Namespaced);
		Enum->DisplayNameMap = NewDisplay;
		Enum->MarkPackageDirty();
	}

	void LeaveMapsOf(const FString& Dir)
	{
		if (GEditor && GEditor->GetEditorWorldContext().World())
		{
			const FString Open = GEditor->GetEditorWorldContext().World()->GetOutermost()->GetName();
			if (IsUnder(FPackageName::GetLongPackagePath(Open), Dir))
			{
				UEditorLoadingAndSavingUtils::NewBlankMap(true);
			}
		}
	}

	void RemoveEmptyFolder(const FString& Dir)
	{
		const FString Disk = FPackageName::LongPackageNameToFilename(Dir + TEXT("/"));
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Disk, TEXT("*.*"), true, false);
		if (Files.Num() == 0 && IFileManager::Get().DirectoryExists(*Disk))
		{
			IFileManager::Get().DeleteDirectory(*Disk, false, true);
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().RemovePath(Dir);
			UE_LOG(LogAEWArena, Log, TEXT("removed empty folder %s"), *Dir);
		}
	}

	bool RenameArenaMaps(const FString& OldLevel, int64 OldId, int64 NewId, FString& OutNewLevel,
		TMap<FString, FString>& OutPackageMoves, TArray<UPackage*>& ToSave)
	{
		OutNewLevel = OldLevel;
		const FString OldNum = LexToString(OldId);
		const FString NewNum = LexToString(NewId);
		if (LeadingNumber(OldLevel) != OldNum)
		{
			return true;
		}
		const FString NewLevel = NewNum + TEXT("_") + StripNumber(OldLevel);

		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Worlds;
		AR.GetAssetsByClass(UWorld::StaticClass()->GetFName(), Worlds);
		const FAssetData* Main = nullptr;
		for (const FAssetData& W : Worlds)
		{
			if (W.AssetName.ToString() == OldLevel)
			{
				Main = &W;
			}
			if (W.AssetName.ToString().Equals(NewLevel, ESearchCase::IgnoreCase))
			{
				UE_LOG(LogAEWArena, Error, TEXT("can't rename %s: a map named %s already exists (%s)"), *OldLevel, *NewLevel, *W.PackageName.ToString());
				return false;
			}
		}
		if (!Main)
		{
			UE_LOG(LogAEWArena, Warning, TEXT("map %s not found - only its table entries were renumbered"), *OldLevel);
			return false;
		}

		const FString OldDir = Main->PackagePath.ToString();
		const bool bMoveFolder = FPackageName::GetShortName(OldDir) == OldLevel;
		const FString NewDir = bMoveFolder ? FPackageName::GetLongPackagePath(OldDir) + TEXT("/") + NewLevel : OldDir;

		TArray<FAssetData> Assets;
		AR.GetAssetsByPath(FName(*OldDir), Assets, bMoveFolder);
		LeaveMapsOf(OldDir);

		TArray<FAssetRenameData> Renames;
		TArray<TWeakObjectPtr<UObject>> Moved;
		for (const FAssetData& A : Assets)
		{
			if (A.AssetClass == UObjectRedirector::StaticClass()->GetFName())
			{
				continue;
			}
			const FString Name = A.AssetName.ToString();
			FString NewName = Name;
			if (StartsWithLevel(Name, OldLevel))
			{
				NewName = NewLevel + SwapNumber(Name.Mid(OldLevel.Len()), OldNum, NewNum);
			}
			else if (Name == TEXT("PAL_") + OldLevel)
			{
				NewName = TEXT("PAL_") + NewLevel;
			}
			else if (!bMoveFolder)
			{
				continue;
			}
			const FString Path = A.PackagePath.ToString();
			const FString NewPath = NewDir + Path.Mid(OldDir.Len());
			if (NewName == Name && NewPath == Path)
			{
				continue;
			}
			UObject* Object = A.GetAsset();
			if (!Object)
			{
				UE_LOG(LogAEWArena, Warning, TEXT("could not load %s - not renamed"), *A.PackageName.ToString());
				continue;
			}
			Renames.Emplace(TWeakObjectPtr<UObject>(Object), NewPath, NewName);
			Moved.Add(Object);
			OutPackageMoves.Add(A.PackageName.ToString(), NewPath + TEXT("/") + NewName);
		}
		if (Renames.Num() == 0)
		{
			return true;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		if (!AssetTools.RenameAssets(Renames))
		{
			UE_LOG(LogAEWArena, Error, TEXT("renaming %s to %s failed - check the Message Log"), *OldLevel, *NewLevel);
			return false;
		}
		OutNewLevel = NewLevel;

		const FString NewMain = NewDir + TEXT("/") + NewLevel;
		if (UWorld* World = LoadWorld(NewMain))
		{
			for (ULevelStreaming* LS : World->GetStreamingLevels())
			{
				if (LS)
				{
					if (const FString* Target = OutPackageMoves.Find(LS->GetWorldAssetPackageName()))
					{
						LS->Modify();
						LS->SetWorldAssetByPackageName(FName(**Target));
					}
				}
			}
			World->MarkPackageDirty();
		}

		TArray<UObjectRedirector*> Redirectors;
		for (const FString& Dir : { OldDir, NewDir })
		{
			TArray<FAssetData> Left;
			AR.GetAssetsByPath(FName(*Dir), Left, true);
			for (const FAssetData& A : Left)
			{
				if (A.AssetClass == UObjectRedirector::StaticClass()->GetFName())
				{
					if (UObjectRedirector* R = Cast<UObjectRedirector>(A.GetAsset()))
					{
						Redirectors.AddUnique(R);
					}
				}
			}
		}

		const TMap<FString, FString> Names = ShortNamesOf(OutPackageMoves);
		for (const TWeakObjectPtr<UObject>& O : Moved)
		{
			if (UWorld* W = Cast<UWorld>(O.Get()))
			{
				FixLevelBlueprint(W, Names, OutPackageMoves);
			}
		}

		TArray<UObject*> MapObjects;
		for (const TWeakObjectPtr<UObject>& O : Moved)
		{
			if (O.IsValid())
			{
				ToSave.AddUnique(O->GetOutermost());
				if (O->IsA<UWorld>())
				{
					MapObjects.Add(O.Get());
				}
			}
		}
		const FString LabelPkg = NewDir + TEXT("/PAL_") + NewLevel;
		if (UPrimaryAssetLabel* Label = LoadObject<UPrimaryAssetLabel>(nullptr, *ObjectPathOf(LabelPkg), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Label->Modify();
			Label->ExplicitAssets.Reset();
			TArray<FString> PackagingLog;
			AEWPackaging::UpdateLabel(LabelPkg, Label->Rules.ChunkId, MapObjects, Label->bLabelAssetsInMyDirectory, Label->Rules.bApplyRecursively, ToSave, &PackagingLog);
		}

		UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		if (Redirectors.Num() > 0)
		{
			AssetTools.FixupReferencers(Redirectors);
		}
		{
			TArray<FString> MovedMaps;
			for (const TWeakObjectPtr<UObject>& O : Moved)
			{
				if (O.IsValid() && O->IsA<UWorld>())
				{
					MovedMaps.Add(O->GetOutermost()->GetName());
				}
			}
			UnloadMaps(MovedMaps);
		}
		if (bMoveFolder)
		{
			RemoveEmptyFolder(OldDir);
		}
		UE_LOG(LogAEWArena, Log, TEXT("renamed %s -> %s (%d asset(s))"), *OldLevel, *NewLevel, Renames.Num());
		return true;
	}

	void UpdateMapsToCook(const TMap<FString, FString>& Moves, const TArray<FString>& RemovedLevels)
	{
		UProjectPackagingSettings* PS = GetMutableDefault<UProjectPackagingSettings>();
		bool bChanged = false;
		for (int32 i = PS->MapsToCook.Num() - 1; i >= 0; --i)
		{
			FString& Path = PS->MapsToCook[i].FilePath;
			if (const FString* To = Moves.Find(Path))
			{
				Path = *To;
				bChanged = true;
				continue;
			}
			const FString Short = FPackageName::GetShortName(Path);
			for (const FString& Level : RemovedLevels)
			{
				if (StartsWithLevel(Short, Level) && !FPackageName::DoesPackageExist(Path))
				{
					PS->MapsToCook.RemoveAt(i);
					bChanged = true;
					break;
				}
			}
		}
		if (bChanged)
		{
			PS->SaveConfig();
			PS->UpdateDefaultConfigFile();
		}
	}

	int32 ApplyArenaChanges(const TArray<FName>& RowsToRemove, bool bRenumber)
	{
		const UAEWArenaTemplateSettings* S = Settings();
		UUserDefinedEnum* Enum = LoadSetting<UUserDefinedEnum>(S->ArenaEnum);
		UDataTable* Table = nullptr;
		UDataTable* NewRows = nullptr;
		if (!Enum || !PrepareArenaTables(Table, NewRows))
		{
			UE_LOG(LogAEWArena, Error, TEXT("arena enum or tables not found (Project Settings > Plugins > Arena Maker)"));
			return -1;
		}
		TGuardValue<bool> Busy(GArenaBusy, true);
		const UScriptStruct* RS = Table->GetRowStruct();
		UDataTable* Unlock = LoadSetting<UDataTable>(S->UnlockTable);
		UDataTable* BaseUnlock = LoadSetting<UDataTable>(S->BaseUnlockTable);

		TArray<UPackage*> ToSave;
		TArray<UObject*> Ship = { Table, Enum, NewRows };
		for (UDataTable* P : ParentsOf(Table))
		{
			Ship.AddUnique(P);
		}

		TSet<int64> Removed;
		TArray<FString> RemovedLevels;
		NewRows->Modify();
		for (const FName& Row : RowsToRemove)
		{
			uint8* Data = NewRows->FindRowUnchecked(Row);
			if (!Data)
			{
				continue;
			}
			int64 Id = RowId(Row);
			GetIntField(RS, Data, TEXT("ArenaId"), Id);
			int64 ItemId = 0;
			GetIntField(RS, Data, TEXT("ItemID"), ItemId);
			const FString Level = GetNameField(RS, Data, TEXT("LevelName")).ToString();

			if (Unlock && ItemId >= S->FirstUnlockID && ItemId <= S->LastUnlockID)
			{
				const FName UnlockRow = AEWTables::UnlockRowName(ItemId);
				if (Unlock->GetRowMap().Contains(UnlockRow))
				{
					Unlock->Modify();
					FDataTableEditorUtils::RemoveRow(Unlock, UnlockRow);
					Unlock->MarkPackageDirty();
					ToSave.AddUnique(Unlock->GetOutermost());
					Ship.AddUnique(Unlock);
					UE_LOG(LogAEWArena, Log, TEXT("%s: removed unlock row %s"), *Unlock->GetName(), *UnlockRow.ToString());
				}
			}
			FDataTableEditorUtils::RemoveRow(NewRows, Row);
			Removed.Add(RowId(Row));
			if (Id != RowId(Row))
			{
				Removed.Add(Id);
			}
			RemovedLevels.Add(ShortLevel(Level));
			UE_LOG(LogAEWArena, Log, TEXT("removed arena %s (%s) from %s"), *Row.ToString(), *Level, *NewRows->GetName());
		}

		TMap<int64, int64> Moves;
		if (bRenumber)
		{
			const int64 First = FirstModId(Table, NewRows);
			TArray<int64> Ids;
			for (const TPair<FName, uint8*>& R : NewRows->GetRowMap())
			{
				const int64 Id = RowId(R.Key);
				if (Id >= First)
				{
					Ids.Add(Id);
				}
			}
			Ids.Sort();
			for (int32 i = 0; i < Ids.Num(); ++i)
			{
				if (Ids[i] != First + i)
				{
					Moves.Add(Ids[i], First + i);
				}
			}
		}

		TMap<FString, FString> PackageMoves;
		TArray<int64> Order;
		Moves.GenerateKeyArray(Order);
		Order.Sort();
		for (int64 OldId : Order)
		{
			const int64 NewId = Moves[OldId];
			const FName OldRow(*LexToString(OldId));
			const FName NewRow(*LexToString(NewId));
			if (!FDataTableEditorUtils::RenameRow(NewRows, OldRow, NewRow))
			{
				UE_LOG(LogAEWArena, Error, TEXT("could not rename arena row %s to %s"), *OldRow.ToString(), *NewRow.ToString());
				Moves.Remove(OldId);
				continue;
			}
			uint8* Data = NewRows->FindRowUnchecked(NewRow);
			SetIntField(RS, Data, TEXT("ArenaId"), NewId);
			const FString OldStored = GetNameField(RS, Data, TEXT("LevelName")).ToString();
			const FString OldLevel = ShortLevel(OldStored);
			FString NewLevel = OldLevel;
			RenameArenaMaps(OldLevel, OldId, NewId, NewLevel, PackageMoves, ToSave);
			if (NewLevel != OldLevel)
			{
				SetNameField(RS, Data, TEXT("LevelName"), FName(*LevelValue(NewLevel)));
			}
			UE_LOG(LogAEWArena, Log, TEXT("arena %lld -> %lld (%s)"), OldId, NewId, *NewLevel);
		}

		if (Removed.Num() == 0 && Moves.Num() == 0)
		{
			return 0;
		}

		RebuildEnum(Enum, Removed, Moves);
		ToSave.AddUnique(Enum->GetOutermost());
		NewRows->MarkPackageDirty();
		RefreshComposite(Table);
		ToSave.AddUnique(NewRows->GetOutermost());
		ToSave.AddUnique(Table->GetOutermost());
		if (Unlock && ToSave.Contains(Unlock->GetOutermost()) && BaseUnlock)
		{
			RefreshComposite(BaseUnlock);
			ToSave.AddUnique(BaseUnlock->GetOutermost());
			Ship.AddUnique(BaseUnlock);
		}

		TArray<FString> PackagingLog;
		AEWPackaging::LabelSharedTables(Ship, ToSave, &PackagingLog);
		UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		RemoveStrayUexp(Enum->GetOutermost());
		UpdateMapsToCook(PackageMoves, RemovedLevels);

		UE_LOG(LogAEWArena, Log, TEXT("arena list updated: %d removed, %d renumbered"), RowsToRemove.Num(), Moves.Num());
		return Removed.Num() + Moves.Num();
	}

	int32 RepairArenaMaps(UDataTable* NewRows, const TSet<FName>& Existing)
	{
		const UAEWArenaTemplateSettings* S = Settings();
		const UScriptStruct* RS = NewRows->GetRowStruct();
		TGuardValue<bool> Busy(GArenaBusy, true);
		TSet<FString> Used;
		for (const TPair<FName, uint8*>& R : NewRows->GetRowMap())
		{
			Used.Add(ShortLevel(GetNameField(RS, R.Value, TEXT("LevelName")).ToString()));
		}
		TMap<FString, FString> PackageMoves;
		TArray<UPackage*> ToSave;
		int32 Changed = 0;
		NewRows->Modify();
		for (const TPair<FName, uint8*>& R : NewRows->GetRowMap())
		{
			const FString RowId = R.Key.ToString();
			if (!RowId.IsNumeric())
			{
				continue;
			}
			FString Short = ShortLevel(GetNameField(RS, R.Value, TEXT("LevelName")).ToString());
			if (!Existing.Contains(FName(*Short)))
			{
				const FString Base = StripNumber(Short);
				TArray<FString> Candidates;
				for (const FName& W : Existing)
				{
					const FString Name = W.ToString();
					if (Name != Short && !LeadingNumber(Name).IsEmpty() && StripNumber(Name) == Base && !Used.Contains(Name))
					{
						Candidates.Add(Name);
					}
				}
				if (Candidates.Num() != 1)
				{
					continue;
				}
				UE_LOG(LogAEWArena, Log, TEXT("arena %s: map %s not found, using %s"), *RowId, *Short, *Candidates[0]);
				Used.Add(Candidates[0]);
				Short = Candidates[0];
				SetNameField(RS, R.Value, TEXT("LevelName"), FName(*LevelValue(Short)));
				++Changed;
			}
			const FString Prefix = LeadingNumber(Short);
			if (!Prefix.IsEmpty() && Prefix != RowId)
			{
				FString NewLevel = Short;
				if (RenameArenaMaps(Short, FCString::Atoi64(*Prefix), FCString::Atoi64(*RowId), NewLevel, PackageMoves, ToSave) && NewLevel != Short)
				{
					SetNameField(RS, R.Value, TEXT("LevelName"), FName(*LevelValue(NewLevel)));
					UE_LOG(LogAEWArena, Log, TEXT("arena %s: maps renamed %s -> %s to match the ID"), *RowId, *Short, *NewLevel);
					++Changed;
				}
			}
		}
		if (Changed == 0)
		{
			return 0;
		}
		NewRows->MarkPackageDirty();
		ToSave.AddUnique(NewRows->GetOutermost());
		if (UDataTable* Table = LoadSetting<UDataTable>(S->ArenaTable))
		{
			RefreshComposite(Table);
			ToSave.AddUnique(Table->GetOutermost());
		}
		UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		UpdateMapsToCook(PackageMoves, {});
		return Changed;
	}

	bool GArenaPassQuiet = false;

	void Notify(const FString& Message)
	{
		if (GArenaPassQuiet)
		{
			UE_LOG(LogAEWArena, Log, TEXT("%s"), *Message);
			return;
		}
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 6.f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

bool UAEWArenaTools::IsBusy()
{
	return GArenaBusy;
}

void UAEWArenaTools::HandleRemovedLevels(const TSet<FName>& Levels)
{
	if (GArenaBusy || Levels.Num() == 0)
	{
		return;
	}
	const UAEWArenaTemplateSettings* S = Settings();
	UDataTable* NewRows = LoadSetting<UDataTable>(S->ArenaNewTable);
	if (!NewRows || !NewRows->GetRowStruct())
	{
		return;
	}
	const TSet<FName> Existing = ExistingWorldNames();
	TArray<FName> Rows;
	TArray<FString> Names;
	for (const TPair<FName, uint8*>& R : NewRows->GetRowMap())
	{
		const FName Level(*ShortLevel(GetNameField(NewRows->GetRowStruct(), R.Value, TEXT("LevelName")).ToString()));
		if (Levels.Contains(Level) && !Existing.Contains(Level))
		{
			Rows.Add(R.Key);
			Names.Add(Level.ToString());
		}
	}
	if (Rows.Num() == 0)
	{
		return;
	}
	const int32 Changed = ApplyArenaChanges(Rows, S->bRenumberOnDelete);
	if (Changed >= 0)
	{
		Notify(FString::Printf(TEXT("Removed %s from the arena list%s"), *FString::Join(Names, TEXT(", ")),
			S->bRenumberOnDelete ? TEXT(" and renumbered the mod arenas") : TEXT("")));
	}
}

int32 UAEWArenaTools::SyncArenaList(bool bRenumber, bool bAsk)
{
	const UAEWArenaTemplateSettings* S = Settings();
	UDataTable* NewRows = LoadSetting<UDataTable>(S->ArenaNewTable);
	if (!NewRows || !NewRows->GetRowStruct())
	{
		UE_LOG(LogAEWArena, Error, TEXT("New Arena Rows Table not found: %s"), *S->ArenaNewTable.ToString());
		return -1;
	}
	TSet<FName> Existing = ExistingWorldNames();
	if (RepairArenaMaps(NewRows, Existing) > 0)
	{
		Existing = ExistingWorldNames();
	}
	if (S->bLocalizedDisplayName)
	{
		UUserDefinedEnum* Enum = LoadSetting<UUserDefinedEnum>(S->ArenaEnum);
		if (Enum)
		{
			const UScriptStruct* RS = NewRows->GetRowStruct();
			NewRows->Modify();
			int32 Named = 0;
			for (const TPair<FName, uint8*>& R : NewRows->GetRowMap())
			{
				int64 Id = -1;
				if (!GetIntField(RS, R.Value, TEXT("ArenaId"), Id) && R.Key.ToString().IsNumeric())
				{
					Id = FCString::Atoi64(*R.Key.ToString());
				}
				if (ApplyDisplayName(Enum, RS, R.Value, Id, GetFieldString(RS, R.Value, TEXT("ArenaName")), true))
				{
					++Named;
				}
			}
			if (Named > 0)
			{
				NewRows->MarkPackageDirty();
				TArray<UPackage*> ToSave = { NewRows->GetOutermost(), Enum->GetOutermost() };
				if (UDataTable* Table = LoadSetting<UDataTable>(S->ArenaTable))
				{
					RefreshComposite(Table);
					ToSave.AddUnique(Table->GetOutermost());
				}
				UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
				RemoveStrayUexp(Enum->GetOutermost());
				UE_LOG(LogAEWArena, Log, TEXT("display names set for %d arena(s)"), Named);
			}
		}
	}
	{
		int32 Converted = 0;
		for (const TPair<FName, uint8*>& R : NewRows->GetRowMap())
		{
			const FString Level = GetNameField(NewRows->GetRowStruct(), R.Value, TEXT("LevelName")).ToString();
			const FString Path = Level.StartsWith(TEXT("/")) ? Level : ToLevelPath(Level);
			if (!Path.IsEmpty() && Path != Level)
			{
				if (Converted == 0)
				{
					NewRows->Modify();
				}
				SetNameField(NewRows->GetRowStruct(), R.Value, TEXT("LevelName"), FName(*Path));
				UE_LOG(LogAEWArena, Log, TEXT("arena %s: LevelName %s -> %s"), *R.Key.ToString(), *Level, *Path);
				++Converted;
			}
		}
		if (Converted > 0)
		{
			NewRows->MarkPackageDirty();
			TArray<UPackage*> ToSave = { NewRows->GetOutermost() };
			if (UDataTable* Table = LoadSetting<UDataTable>(S->ArenaTable))
			{
				RefreshComposite(Table);
				ToSave.AddUnique(Table->GetOutermost());
			}
			UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		}
	}
	TArray<FName> Rows;
	FString List;
	for (const TPair<FName, uint8*>& R : NewRows->GetRowMap())
	{
		const FName Level(*ShortLevel(GetNameField(NewRows->GetRowStruct(), R.Value, TEXT("LevelName")).ToString()));
		if (!Existing.Contains(Level))
		{
			Rows.Add(R.Key);
			List += FString::Printf(TEXT("\n  %s  %s"), *R.Key.ToString(), *Level.ToString());
		}
	}
	if (bAsk)
	{
		const FString Message = Rows.Num() > 0
			? FString::Printf(TEXT("These mod arenas have no map any more and will be removed from the arena table, the enum and the unlock table:%s%s"),
				*List, bRenumber ? TEXT("\n\nThe remaining mod arenas are then renumbered without gaps.") : TEXT(""))
			: FString(TEXT("Every mod arena still has its map.")) + (bRenumber ? TEXT("\n\nRenumber the mod arenas to close any gaps in their IDs?") : TEXT(""));
		if (Rows.Num() == 0 && !bRenumber)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
			return 0;
		}
		if (FMessageDialog::Open(EAppMsgType::OkCancel, FText::FromString(Message)) != EAppReturnType::Ok)
		{
			return 0;
		}
	}
	return ApplyArenaChanges(Rows, bRenumber);
}

bool UAEWArenaTools::UnregisterArena(int32 ArenaId, bool bRenumber)
{
	UDataTable* NewRows = LoadSetting<UDataTable>(Settings()->ArenaNewTable);
	const FName Row(*LexToString(ArenaId));
	if (!NewRows || !NewRows->GetRowMap().Contains(Row))
	{
		UE_LOG(LogAEWArena, Error, TEXT("arena %d is not in the New Arena Rows Table - only mod arenas can be removed"), ArenaId);
		return false;
	}
	return ApplyArenaChanges({ Row }, bRenumber) > 0;
}

void UAEWArenaTools::OpenDialog()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("AEWArenaTemplate")));
}

void UAEWArenaTools::OpenSettings()
{
	AEWToolUI::OpenSettings(GetDefault<UAEWArenaTemplateSettings>()->GetSectionName());
}

void UAEWArenaTools::GetArenaList(TArray<FAEWArenaListEntry>& Out)
{
	Out.Reset();
	UDataTable* Table = LoadSetting<UDataTable>(Settings()->ArenaTable);
	if (!Table || !Table->GetRowStruct())
	{
		return;
	}
	const UScriptStruct* RS = Table->GetRowStruct();
	for (const TPair<FName, uint8*>& R : Table->GetRowMap())
	{
		FAEWArenaListEntry Entry;
		Entry.Id = R.Key.ToString();
		Entry.LevelName = GetNameField(RS, R.Value, TEXT("LevelName")).ToString();
		if (FBoolProperty* P = CastField<FBoolProperty>(FindField(RS, TEXT("IsSelectable"))))
		{
			Entry.bSelectable = P->GetPropertyValue_InContainer(R.Value);
		}
		Out.Add(Entry);
	}
	Out.Sort([](const FAEWArenaListEntry& A, const FAEWArenaListEntry& B)
	{
		return A.Id.IsNumeric() && B.Id.IsNumeric() ? FCString::Atoi(*A.Id) < FCString::Atoi(*B.Id) : A.Id < B.Id;
	});
}

bool UAEWArenaTools::RepointStreamingLevel(ULevelStreaming* StreamingLevel, const FString& NewPackageName)
{
	if (StreamingLevel == nullptr || NewPackageName.IsEmpty())
	{
		return false;
	}
	StreamingLevel->Modify();
	StreamingLevel->SetWorldAssetByPackageName(FName(*NewPackageName));
	StreamingLevel->MarkPackageDirty();
	if (UObject* Outer = StreamingLevel->GetOuter())
	{
		Outer->MarkPackageDirty();
	}
	return true;
}

namespace
{
	UWorld* OpenModArena()
	{
		UWorld* Current = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (GArenaBusy || !Current || GEditor->PlayWorld || Current->GetOutermost()->IsDirty())
		{
			return nullptr;
		}
		const FString MapPkg = Current->GetOutermost()->GetName();
		if (IsUnder(FPackageName::GetLongPackagePath(MapPkg), NormDir(Settings()->TemplateRoot.Path)))
		{
			return nullptr;
		}
		for (const FString& Level : UAEWArenaTools::GetArenaLevelNames())
		{
			FString Pkg = Level;
			Pkg.Split(TEXT("."), &Pkg, nullptr);
			if (Pkg.Equals(MapPkg, ESearchCase::IgnoreCase))
			{
				return Current;
			}
		}
		return nullptr;
	}

	bool IsBarricadeSet(AActor* Actor)
	{
		if (!Actor || !Actor->GetClass()->GetName().Contains(TEXT("Barricade")))
		{
			return false;
		}
		int32 Children = 0;
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (UActorComponent* C : Components)
		{
			if (UChildActorComponent* Child = Cast<UChildActorComponent>(C))
			{
				Children += Child->GetChildActorClass() ? 1 : 0;
			}
			else if (Cast<UPrimitiveComponent>(C))
			{
				return false;
			}
		}
		return Children > 0;
	}
}

namespace
{
	bool IsBarricadePiece(AActor* Actor)
	{
		if (!Actor || !Actor->GetClass()->GetName().Contains(TEXT("Barricade")))
		{
			return false;
		}
		TInlineComponentArray<UBoxComponent*> Boxes(Actor);
		for (UBoxComponent* B : Boxes)
		{
			if (B->GetFName() == TEXT("CollisionReactionArea"))
			{
				return true;
			}
		}
		return false;
	}

	const TCHAR* BarricadeGroupPrefix = TEXT("AEWBarricadeGroup:");

	void AddBarricadeGroup(UWorld* World, TArray<AActor*>& Actors)
	{
		TSet<FName> Groups;
		for (AActor* A : Actors)
		{
			for (const FName& Tag : A->Tags)
			{
				if (Tag.ToString().StartsWith(BarricadeGroupPrefix))
				{
					Groups.Add(Tag);
				}
			}
		}
		if (Groups.Num() == 0 || !World)
		{
			return;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			for (const FName& Tag : It->Tags)
			{
				if (Groups.Contains(Tag))
				{
					Actors.AddUnique(*It);
				}
			}
		}
	}

	void SetActorArray(UObject* Object, const FName Name, const TArray<AActor*>& Items)
	{
		FArrayProperty* Array = Object ? FindFProperty<FArrayProperty>(Object->GetClass(), Name) : nullptr;
		FObjectProperty* Inner = Array ? CastField<FObjectProperty>(Array->Inner) : nullptr;
		if (!Inner)
		{
			return;
		}
		FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Object));
		Helper.Resize(Items.Num());
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			Inner->SetObjectPropertyValue(Helper.GetRawPtr(i), Items[i]);
		}
	}

	bool IsBarricadeGuard(AActor* Actor)
	{
		return Actor && Actor->GetClass()->GetName() == TEXT("BP_BarricadeGuard_C");
	}

	const TCHAR* BarricadeSettingsPrefix = TEXT("AEWBarricadeSettings:");

	UClass* LoadBarricadePieceClass(TArray<FString>& Tried)
	{
		UClass* PieceClass = nullptr;
		const FString Base = TEXT("/Game/GameData/Resources/Models/EmvData/Arean/Common/Barricade/");
		for (const FString& Path : { Settings()->BarricadeClass.ToString(), Base + TEXT("0000_Base/BP_Barricade.BP_Barricade_C"), Base + TEXT("0001_Dynamite/BP_Barricade_0001.BP_Barricade_0001_C"), Base + TEXT("0001_Dynamite/BP_Barricade_Long_0001.BP_Barricade_Long_0001_C") })
		{
			if (!PieceClass && !Path.IsEmpty() && !Tried.Contains(Path))
			{
				Tried.Add(Path);
				PieceClass = FSoftClassPath(Path).TryLoadClass<AActor>();
			}
		}
		return PieceClass;
	}

	UClass* BarricadeBaseClass(UClass* PieceClass)
	{
		UClass* Base = PieceClass;
		for (UClass* C = PieceClass; C; C = C->GetSuperClass())
		{
			if (C->FindPropertyByName(TEXT("BarricadeCondition")) && C->FindFunctionByName(TEXT("ResetShake"), EIncludeSuperFlag::ExcludeSuper))
			{
				Base = C;
			}
		}
		return Base;
	}

	TSet<FString> GRebuiltGuardFolders;

	AActor* SpawnBarricadeSettings(UWorld* World, ULevel* Level, UClass* PieceClass, const TArray<AActor*>& Pieces, const TArray<AActor*>& Meshes,
		const FName Group, const FString& Label, const FName Folder, const FTransform& Where, bool bClimbable)
	{
		const FString ArenaFolder = FPackageName::GetLongPackagePath(World->GetOutermost()->GetName());
		const bool bRebuild = !GRebuiltGuardFolders.Contains(ArenaFolder);
		UBlueprint* GuardBP = UAEWArenaTools::EnsureBarricadeGuardBlueprint(ArenaFolder, BarricadeBaseClass(PieceClass), bRebuild);
		if (!GuardBP || !GuardBP->GeneratedClass)
		{
			return nullptr;
		}
		GRebuiltGuardFolders.Add(ArenaFolder);
		FActorSpawnParameters Params;
		Params.OverrideLevel = Level;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Guard = World->SpawnActor<AActor>(GuardBP->GeneratedClass, FTransform(Where.GetRotation(), Where.GetLocation()), Params);
		if (!Guard)
		{
			return nullptr;
		}
		SetActorArray(Guard, TEXT("Pieces"), Pieces);
		SetActorArray(Guard, TEXT("Meshes"), Meshes);
		if (FBoolProperty* Climb = FindFProperty<FBoolProperty>(Guard->GetClass(), TEXT("bCanBeClimbed")))
		{
			Climb->SetPropertyValue_InContainer(Guard, bClimbable);
		}
		Guard->Tags.AddUnique(Group);
		for (AActor* P : Pieces)
		{
			P->Modify();
			P->Tags.AddUnique(Group);
		}
		Guard->SetActorLabel(Label + TEXT("_Settings"));
		Guard->SetFolderPath(Folder);
		Guard->PostEditChange();
		return Guard;
	}

	FBoolProperty* NoClimbProperty(UClass* Class)
	{
		return Class ? FindFProperty<FBoolProperty>(Class, FName(TEXT("\u767b\u308c\u306a\u3044"))) : nullptr;
	}

	void GatherWorldVerts(UStaticMeshComponent* SM, TArray<FVector>& Out)
	{
		UStaticMesh* Mesh = SM ? SM->GetStaticMesh() : nullptr;
		FStaticMeshRenderData* Render = Mesh ? Mesh->RenderData.Get() : nullptr;
		const FTransform T = SM ? SM->GetComponentTransform() : FTransform::Identity;
		if (Render && Render->LODResources.Num() > 0)
		{
			const FPositionVertexBuffer& Positions = Render->LODResources[0].VertexBuffers.PositionVertexBuffer;
			const int32 Count = Positions.GetNumVertices();
			UE_LOG(LogAEWArena, Log, TEXT("Make Barricade: %s LOD0 has %d vertices, CPU data %s"), *Mesh->GetName(), Count, Positions.GetVertexData() ? TEXT("yes") : TEXT("no"));
			if (Count > 0 && Positions.GetVertexData())
			{
				const int32 Step = FMath::Max(1, Count / 20000);
				for (int32 i = 0; i < Count; i += Step)
				{
					Out.Add(T.TransformPosition(Positions.VertexPosition(i)));
				}
				return;
			}
		}
		if (Mesh)
		{
			const FBox B = Mesh->GetBoundingBox();
			for (int32 i = 0; i < 8; ++i)
			{
				Out.Add(T.TransformPosition(FVector((i & 1) ? B.Max.X : B.Min.X, (i & 2) ? B.Max.Y : B.Min.Y, (i & 4) ? B.Max.Z : B.Min.Z)));
			}
		}
	}

	bool FitBarricadeFrame(const TArray<FVector>& Verts, const FVector& RingLocation, FTransform& OutFrame, FBox& OutLocalBox)
	{
		if (Verts.Num() == 0)
		{
			return false;
		}
		float BestArea = TNumericLimits<float>::Max();
		float BestYaw = 0.f;
		for (int32 Deg = 0; Deg < 180; ++Deg)
		{
			const float C = FMath::Cos(FMath::DegreesToRadians(float(Deg)));
			const float S = FMath::Sin(FMath::DegreesToRadians(float(Deg)));
			float MinX = TNumericLimits<float>::Max(), MaxX = -MinX, MinY = MinX, MaxY = -MinX;
			for (const FVector& V : Verts)
			{
				const float X = V.X * C + V.Y * S;
				const float Y = -V.X * S + V.Y * C;
				MinX = FMath::Min(MinX, X); MaxX = FMath::Max(MaxX, X);
				MinY = FMath::Min(MinY, Y); MaxY = FMath::Max(MaxY, Y);
			}
			const float Area = (MaxX - MinX) * (MaxY - MinY);
			if (Area < BestArea - 0.01f || (FMath::IsNearlyEqual(Area, BestArea, 0.01f) && (MaxX - MinX) >= (MaxY - MinY)))
			{
				BestArea = Area;
				BestYaw = (MaxX - MinX) >= (MaxY - MinY) ? float(Deg) : float(Deg) + 90.f;
			}
		}
		FRotator Rot(0.f, BestYaw, 0.f);
		FBox Box(ForceInit);
		for (const FVector& V : Verts)
		{
			Box += Rot.UnrotateVector(V);
		}
		const FVector Base = Rot.RotateVector(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z));
		FVector ToRing = RingLocation - Base;
		ToRing.Z = 0.f;
		if (!ToRing.IsNearlyZero() && FVector::DotProduct(Rot.RotateVector(FVector::RightVector), ToRing) < 0.f)
		{
			Rot.Yaw += 180.f;
		}
		Rot.Normalize();
		OutFrame = FTransform(Rot, Base);
		OutLocalBox = FBox(ForceInit);
		for (const FVector& V : Verts)
		{
			OutLocalBox += OutFrame.InverseTransformPosition(V);
		}
		return true;
	}

}

int32 UAEWArenaTools::MakeBarricadeFromSelected()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	const FText Title = LOCTEXT("MakeBarricadeTitle", "Make Barricade From Selected");
	if (!World || GEditor->PlayWorld)
	{
		return 0;
	}
	TArray<AActor*> Sources;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* A = Cast<AActor>(*It);
		if (!A || IsBarricadePiece(A))
		{
			continue;
		}
		if (A->Tags.ContainsByPredicate([](const FName& T) { return T.ToString().StartsWith(BarricadeGroupPrefix); }))
		{
			UE_LOG(LogAEWArena, Warning, TEXT("%s is already a barricade - run Restore Meshes From Barricades first to remake it"), *A->GetActorLabel());
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(A);
		if (Meshes.Num() > 0)
		{
			Sources.Add(A);
		}
	}
	if (Sources.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("MakeBarricadeNone", "Select one or more mesh actors (e.g. FamilyMart_FM_board) in the level first."), &Title);
		return 0;
	}

	TArray<FString> Tried;
	UClass* PieceClass = LoadBarricadePieceClass(Tried);
	if (!PieceClass)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("No barricade class could be loaded. Set Barricade Class (Project Settings > Arena Maker) to a barricade blueprint in the project. Tried:\n  ") + FString::Join(Tried, TEXT("\n  "))), &Title);
		return 0;
	}
	if (Tried.Num() > 1)
	{
		UE_LOG(LogAEWArena, Warning, TEXT("Barricade Class %s could not be loaded - using %s"), *Settings()->BarricadeClass.ToString(), *PieceClass->GetPathName());
	}

	FVector RingLocation = FVector::ZeroVector;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("BP_Ring_C"))
		{
			RingLocation = It->GetActorLocation();
			break;
		}
	}

	int32 Made = 0;
	TArray<FString> Report;
	const FScopedTransaction Transaction(Title);
	for (AActor* Source : Sources)
	{
		ULevel* Level = Source->GetLevel();
		Level->Modify();
		TArray<FVector> Verts;
		TInlineComponentArray<UStaticMeshComponent*> FrameMeshes(Source);
		for (UStaticMeshComponent* SM : FrameMeshes)
		{
			GatherWorldVerts(SM, Verts);
		}
		FTransform PieceTransform(Source->GetActorRotation(), Source->GetActorLocation());
		FBox FittedBox(ForceInit);
		const bool bFitted = FitBarricadeFrame(Verts, RingLocation, PieceTransform, FittedBox);
		auto SpawnPiece = [&](const FTransform& Where, USkeletalMeshComponent*& OutMesh) -> AActor*
		{
			FActorSpawnParameters Params;
			Params.OverrideLevel = Level;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* P = World->SpawnActor<AActor>(PieceClass, Where, Params);
			OutMesh = P ? Cast<USkeletalMeshComponent>(P->GetRootComponent()) : nullptr;
			if (P && !OutMesh)
			{
				TInlineComponentArray<USkeletalMeshComponent*> Skels(P);
				OutMesh = Skels.Num() > 0 ? Skels[0] : nullptr;
			}
			if (P && !OutMesh)
			{
				World->EditorDestroyActor(P, true);
				return nullptr;
			}
			if (P)
			{
				P->Modify();
				P->SetActorTransform(Where);
			}
			return P;
		};

		bool bNoCollision = false;
		int32 MeshCount = 0;
		TInlineComponentArray<UStaticMeshComponent*> SourceMeshes(Source);
		for (UStaticMeshComponent* SM : SourceMeshes)
		{
			if (!SM->GetStaticMesh())
			{
				continue;
			}
			++MeshCount;
			UBodySetup* Body = SM->GetStaticMesh()->GetBodySetup();
			bNoCollision = bNoCollision || SM->GetCollisionEnabled() == ECollisionEnabled::NoCollision || !Body
				|| (Body->AggGeom.GetElementCount() == 0 && Body->CollisionTraceFlag != CTF_UseComplexAsSimple);
		}
		if (MeshCount == 0 || !bFitted)
		{
			Report.Add(Source->GetActorLabel() + TEXT(": has no static mesh to measure"));
			continue;
		}

		USkeletalMeshComponent* FirstMesh = nullptr;
		AActor* First = SpawnPiece(PieceTransform, FirstMesh);
		if (!First)
		{
			Report.Add(Source->GetActorLabel() + TEXT(": could not create a barricade piece"));
			continue;
		}
		const FBoxSphereBounds OldBounds = FirstMesh->SkeletalMesh ? FirstMesh->SkeletalMesh->GetBounds() : FBoxSphereBounds(FVector(0.f, 0.f, 55.f), FVector(138.f, 28.f, 55.f), 150.f);

		const float Length = FittedBox.GetSize().X;
		const float Segment = FMath::Max(OldBounds.BoxExtent.X * 2.f, 1.f);
		const int32 Count = Length > Segment * 1.5f ? FMath::Max(1, FMath::RoundToInt(Length / Segment)) : 1;
		UE_LOG(LogAEWArena, Log, TEXT("Make Barricade: %s - %d vertices, frame %s, local box %s, barricade bounds origin %s extent %s, length %.1f, %d piece(s)"),
			*Source->GetActorLabel(), Verts.Num(), *PieceTransform.ToString(), *FittedBox.ToString(),
			*OldBounds.Origin.ToString(), *OldBounds.BoxExtent.ToString(), Length, Count);
		auto SegmentTransform = [&](int32 i)
		{
			const float X = FittedBox.Min.X + (i + 0.5f) * Length / Count;
			return FTransform(FVector(X, 0.f, 0.f)) * PieceTransform;
		};
		const float HalfSection = 0.5f * Length / Count;
		const float Height = FMath::Max(FittedBox.Max.Z, 1.f);

		const FString Label = Source->GetActorLabel() + TEXT("_Barricade");
		const FName Group(*(FString(BarricadeGroupPrefix) + Label));
		bool bClimbWarned = false;
		auto Finish = [&](AActor* P, USkeletalMeshComponent* Mesh, const FString& PieceLabel)
		{
			Mesh->Modify();
			Mesh->SetSkeletalMesh(nullptr);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			const float LengthScale = HalfSection / FMath::Max(OldBounds.BoxExtent.X, 1.f);
			const float HeightScale = Height / FMath::Max(OldBounds.Origin.Z + OldBounds.BoxExtent.Z, 1.f);
			TInlineComponentArray<UBoxComponent*> Boxes(P);
			for (UBoxComponent* B : Boxes)
			{
				if (B->GetAttachParent() != Mesh)
				{
					continue;
				}
				B->Modify();
				const FQuat Rot = B->GetRelativeRotation().Quaternion();
				const FVector Scale = B->GetRelativeScale3D().GetAbs().ComponentMax(FVector(KINDA_SMALL_NUMBER));
				const FVector RootExt = Rot.RotateVector(B->GetUnscaledBoxExtent() * Scale).GetAbs();
				const FVector NewRootExt(RootExt.X * LengthScale, RootExt.Y, RootExt.Z * HeightScale);
				B->SetBoxExtent(Rot.UnrotateVector(NewRootExt).GetAbs() / Scale);
				const FVector Loc = B->GetRelativeLocation();
				B->SetRelativeLocation(FVector(Loc.X * LengthScale, Loc.Y, Loc.Z * HeightScale));
			}
			if (FBoolProperty* NoClimb = NoClimbProperty(P->GetClass()))
			{
				NoClimb->SetPropertyValue_InContainer(P, true);
			}
			else if (!bClimbWarned)
			{
				bClimbWarned = true;
				UE_LOG(LogAEWArena, Warning, TEXT("%s has no cannot-climb variable - %s can still be climbed"), *PieceClass->GetName(), *Source->GetActorLabel());
			}
			P->Tags.AddUnique(Group);
			P->SetActorLabel(PieceLabel);
			P->SetFolderPath(Source->GetFolderPath());
			P->PostEditChange();
		};

		First->SetActorTransform(SegmentTransform(0));
		Finish(First, FirstMesh, Label);
		TArray<AActor*> Pieces = { First };
		int32 Filled = 1;
		for (int32 i = 1; i < Count; ++i)
		{
			USkeletalMeshComponent* FillMesh = nullptr;
			AActor* Fill = SpawnPiece(SegmentTransform(i), FillMesh);
			if (!Fill)
			{
				continue;
			}
			Finish(Fill, FillMesh, FString::Printf(TEXT("%s_Fill%d"), *Label, i));
			Pieces.Add(Fill);
			++Filled;
		}

		const bool bGuarded = SpawnBarricadeSettings(World, Level, PieceClass, Pieces, { Source }, Group, Label, Source->GetFolderPath(), PieceTransform, false) != nullptr;
		if (!bGuarded)
		{
			UE_LOG(LogAEWArena, Warning, TEXT("%s: could not add the barricade settings actor - knock-down settings won't apply"), *Source->GetActorLabel());
		}

		Source->Modify();
		Source->Tags.AddUnique(Group);
		++Made;
		Report.Add(FString::Printf(TEXT("%s -> %d x %s (%s)%s"), *Source->GetActorLabel(), Filled, *PieceClass->GetName(), *Label,
			bNoCollision ? TEXT(" - the mesh has no collision, so wrestlers may walk through it; add collision in the Static Mesh editor") : TEXT("")));
	}
	GEditor->SelectNone(true, true);
	GEditor->RedrawLevelEditingViewports();
	if (Made > 0 || Report.Num() > 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("%d barricade(s) made:\n  %s\n\nYour mesh stays as it is; invisible barricade pieces (one per barricade length, no barricade model) were placed along it, facing the ring, with their reaction, hit and weapon boxes fitted to each section. Settings are on the <name>_Barricade_Settings actor - its Details (Arena Override section) has Can Be Climbed (off to start with), Allow Barricade Moves, Can Be Knocked Down and Starts Down. Save the map to keep it."), Made, *FString::Join(Report, TEXT("\n  ")))), &Title);
	}
	return Made;
}

int32 UAEWArenaTools::RestoreMeshesFromBarricades()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	const FText Title = LOCTEXT("RestoreMeshesTitle", "Restore Meshes From Barricades");
	if (!World || GEditor->PlayWorld)
	{
		return 0;
	}
	auto MeshesOf = [](AActor* A)
	{
		TArray<UStaticMeshComponent*> Out;
		if (A && IsBarricadePiece(A))
		{
			for (UActorComponent* C : A->GetInstanceComponents())
			{
				UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
				if (SM && SM->GetStaticMesh())
				{
					Out.Add(SM);
				}
			}
		}
		return Out;
	};
	auto IsGrouped = [](AActor* A)
	{
		return A && A->Tags.ContainsByPredicate([](const FName& T) { return T.ToString().StartsWith(BarricadeGroupPrefix); });
	};
	TArray<AActor*> Targets;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* A = Cast<AActor>(*It);
		if (IsGrouped(A) || MeshesOf(A).Num() > 0)
		{
			Targets.Add(A);
		}
	}
	if (Targets.Num() == 0)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsGrouped(*It) || MeshesOf(*It).Num() > 0)
			{
				Targets.Add(*It);
			}
		}
	}
	AddBarricadeGroup(World, Targets);
	int32 PieceCount = 0;
	for (AActor* A : Targets)
	{
		PieceCount += IsBarricadePiece(A) ? 1 : 0;
	}
	if (PieceCount == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("RestoreMeshesNone", "No barricades made from your meshes were found in this level."), &Title);
		return 0;
	}
	if (FMessageDialog::Open(EAppMsgType::OkCancel, FText::FromString(FString::Printf(TEXT("Remove %d barricade piece(s) made from your meshes and keep (or bring back) the plain meshes?"), PieceCount)), &Title) != EAppReturnType::Ok)
	{
		return 0;
	}

	const FScopedTransaction Transaction(Title);
	int32 Restored = 0;
	TArray<FString> Names;
	for (AActor* A : Targets)
	{
		if (!IsValid(A))
		{
			continue;
		}
		if (IsBarricadeGuard(A))
		{
			World->EditorDestroyActor(A, true);
			continue;
		}
		if (!IsBarricadePiece(A))
		{
			A->Modify();
			A->Tags.RemoveAll([](const FName& T) { return T.ToString().StartsWith(BarricadeGroupPrefix); });
			Names.AddUnique(A->GetActorLabel());
			continue;
		}
		ULevel* Level = A->GetLevel();
		Level->Modify();
		FString Label = A->GetActorLabel();
		Label.RemoveFromEnd(TEXT("_Barricade"));
		const TArray<UStaticMeshComponent*> Meshes = MeshesOf(A);
		int32 Index = 0;
		for (UStaticMeshComponent* SM : Meshes)
		{
			FActorSpawnParameters Params;
			Params.OverrideLevel = Level;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SM->GetComponentTransform(), Params);
			if (!Actor)
			{
				continue;
			}
			UStaticMeshComponent* Out = Actor->GetStaticMeshComponent();
			Out->SetMobility(SM->Mobility);
			Out->SetStaticMesh(SM->GetStaticMesh());
			for (int32 i = 0; i < SM->GetNumOverrideMaterials(); ++i)
			{
				Out->SetMaterial(i, SM->OverrideMaterials[i]);
			}
			Out->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Actor->SetActorTransform(SM->GetComponentTransform());
			Actor->SetActorLabel(Meshes.Num() > 1 ? FString::Printf(TEXT("%s_%d"), *Label, Index) : Label);
			Actor->SetFolderPath(A->GetFolderPath());
			Actor->PostEditChange();
			++Index;
		}
		if (Index > 0)
		{
			Names.AddUnique(Label);
		}
		World->EditorDestroyActor(A, true);
		++Restored;
	}
	GEditor->SelectNone(true, true);
	GEditor->RedrawLevelEditingViewports();
	Notify(FString::Printf(TEXT("Removed %d barricade piece(s); plain meshes: %s. Save the map to keep it."), Restored, *FString::Join(Names, TEXT(", "))));
	return Restored;
}

int32 UAEWArenaTools::AddBarricadeSettingsToSelected()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	const FText Title = LOCTEXT("AddBarricadeSettingsTitle", "Add Barricade Settings To Selected");
	if (!World || GEditor->PlayWorld)
	{
		return 0;
	}
	TArray<AActor*> Pieces;
	int32 AlreadyCovered = 0;
	for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
	{
		AActor* A = Cast<AActor>(*It);
		if (!A || !IsBarricadePiece(A))
		{
			continue;
		}
		if (A->Tags.ContainsByPredicate([](const FName& T) { return T.ToString().StartsWith(BarricadeGroupPrefix) || T.ToString().StartsWith(BarricadeSettingsPrefix); }))
		{
			++AlreadyCovered;
			continue;
		}
		Pieces.Add(A);
	}
	if (Pieces.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(AlreadyCovered > 0
			? FString(TEXT("The selected barricade pieces already have a Settings actor."))
			: FString(TEXT("Select one or more barricade pieces in the level first (e.g. the pieces of a split barricade set)."))), &Title);
		return 0;
	}
	FVector Center = FVector::ZeroVector;
	for (AActor* P : Pieces)
	{
		Center += P->GetActorLocation();
	}
	Center /= Pieces.Num();
	FBoolProperty* NoClimb = NoClimbProperty(Pieces[0]->GetClass());
	const bool bClimbable = !NoClimb || !NoClimb->GetPropertyValue_InContainer(Pieces[0]);
	const FString Label = Pieces.Num() == 1 ? Pieces[0]->GetActorLabel() : Pieces[0]->GetActorLabel() + FString::Printf(TEXT("_x%d"), Pieces.Num());
	const FScopedTransaction Transaction(Title);
	Pieces[0]->GetLevel()->Modify();
	AActor* Guard = SpawnBarricadeSettings(World, Pieces[0]->GetLevel(), Pieces[0]->GetClass(), Pieces, {}, FName(*(FString(BarricadeSettingsPrefix) + Label)), Label, Pieces[0]->GetFolderPath(), FTransform(Center), bClimbable);
	if (!Guard)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AddBarricadeSettingsFailed", "Could not create the barricade settings blueprint - see the Output Log."), &Title);
		return 0;
	}
	GEditor->SelectNone(true, true);
	GEditor->SelectActor(Guard, true, true);
	Notify(FString::Printf(TEXT("Added %s for %d barricade piece(s) - its Details (Arena Override section) has Can Be Climbed, Allow Barricade Moves, Can Be Knocked Down and Starts Down. Save the map to keep it."), *Guard->GetActorLabel(), Pieces.Num()));
	return Pieces.Num();
}

bool UAEWArenaTools::IsConvertingArenas()
{
	return GArenaPassQuiet;
}

int32 UAEWArenaTools::ConvertExistingArenas()
{
	UAEWArenaTemplateSettings* S = GetMutableDefault<UAEWArenaTemplateSettings>();
	if (S->bExistingArenasConverted || GArenaBusy || GArenaPassQuiet || !GEditor || GEditor->PlayWorld)
	{
		return 0;
	}
	UWorld* Current = GEditor->GetEditorWorldContext().World();
	if (Current && Current->GetOutermost()->IsDirty())
	{
		return 0;
	}
	const FString Restore = Current && FPackageName::DoesPackageExist(Current->GetOutermost()->GetName()) ? Current->GetOutermost()->GetName() : FString();
	const FString Root = NormDir(S->TemplateRoot.Path);
	TArray<FString> Maps;
	for (const FString& Level : GetArenaLevelNames())
	{
		FString Pkg = Level;
		Pkg.Split(TEXT("."), &Pkg, nullptr);
		if (Pkg.StartsWith(TEXT("/")) && !IsUnder(FPackageName::GetLongPackagePath(Pkg), Root) && FPackageName::DoesPackageExist(Pkg))
		{
			Maps.AddUnique(Pkg);
		}
	}

	int32 Sublevels = 0;
	int32 Pieces = 0;
	TArray<FString> Changed;
	{
		TGuardValue<bool> Quiet(GArenaPassQuiet, true);
		FScopedSlowTask Task(Maps.Num(), LOCTEXT("ConvertingArenas", "Updating existing arenas (shared sublevels, barricade sets)"));
		Task.MakeDialog();
		for (const FString& Map : Maps)
		{
			Task.EnterProgressFrame(1.f, FText::FromString(FPackageName::GetShortName(Map)));
			if (!FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(Map, FPackageName::GetMapPackageExtension()), false, true))
			{
				UE_LOG(LogAEWArena, Warning, TEXT("one-time pass: could not open %s"), *Map);
				continue;
			}
			const int32 Own = MakeSharedSublevelsOwn();
			const int32 Split = SplitBarricadeSets();
			Sublevels += Own;
			Pieces += Split;
			if (Own > 0 || Split > 0)
			{
				Changed.Add(FString::Printf(TEXT("%s (%d sublevel(s), %d barricade piece(s))"), *FPackageName::GetShortName(Map), Own, Split));
			}
		}
		if (Restore.IsEmpty())
		{
			UEditorLoadingAndSavingUtils::NewBlankMap(false);
		}
		else
		{
			FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(Restore, FPackageName::GetMapPackageExtension()), false, true);
		}
	}

	S->bExistingArenasConverted = true;
	S->SaveConfig();
	S->UpdateDefaultConfigFile();
	UE_LOG(LogAEWArena, Log, TEXT("one-time pass over %d arena(s): %d updated"), Maps.Num(), Changed.Num());
	for (const FString& Line : Changed)
	{
		UE_LOG(LogAEWArena, Log, TEXT("  %s"), *Line);
	}
	if (Changed.Num() > 0)
	{
		Notify(FString::Printf(TEXT("Existing arenas updated once: %d shared sublevel(s) copied, %d barricade piece(s) split.\n%s"), Sublevels, Pieces, *FString::Join(Changed, TEXT("\n"))));
	}
	return Changed.Num();
}

int32 UAEWArenaTools::SplitBarricadeSets()
{
	UWorld* World = OpenModArena();
	if (!World)
	{
		return 0;
	}
	const FString Folder = FPackageName::GetLongPackagePath(World->GetOutermost()->GetName());
	TArray<AActor*> Sets;
	TArray<FString> Skipped;
	for (ULevel* Level : World->GetLevels())
	{
		if (!Level || FPackageName::GetLongPackagePath(Level->GetOutermost()->GetName()) != Folder)
		{
			continue;
		}
		for (AActor* A : Level->Actors)
		{
			if (!A || A->IsChildActor() || !A->GetClass()->GetName().Contains(TEXT("Barricade")))
			{
				continue;
			}
			if (IsBarricadeSet(A))
			{
				Sets.Add(A);
			}
			else
			{
				TInlineComponentArray<UChildActorComponent*> Children(A);
				if (Children.Num() > 0)
				{
					Skipped.Add(A->GetActorLabel());
				}
			}
		}
	}
	for (const FString& Name : Skipped)
	{
		UE_LOG(LogAEWArena, Warning, TEXT("%s holds meshes as well as barricade pieces - left as one actor"), *Name);
	}
	if (Sets.Num() == 0)
	{
		return 0;
	}
	for (ULevel* Level : World->GetLevels())
	{
		if (Level && Level->GetOutermost()->IsDirty())
		{
			return 0;
		}
	}

	TGuardValue<bool> Busy(GArenaBusy, true);
	int32 Pieces = 0;
	TSet<UPackage*> Changed;
	FString Done;
	{
		const FScopedTransaction Transaction(LOCTEXT("SplitBarricades", "Split Barricade Sets"));
		for (AActor* Set : Sets)
		{
			ULevel* Level = Set->GetLevel();
			const FString SetLabel = Set->GetActorLabel();
			const FName SetFolder = Set->GetFolderPath();
			const FName PieceFolder = SetFolder.IsNone() ? FName(*SetLabel) : FName(*(SetFolder.ToString() + TEXT("/") + SetLabel));
			Level->Modify();
			int32 Count = 0;
			TArray<AActor*> SetPieces;
			TInlineComponentArray<UChildActorComponent*> Children(Set);
			for (UChildActorComponent* Child : Children)
			{
				UClass* PieceClass = Child->GetChildActorClass();
				if (!PieceClass)
				{
					continue;
				}
				AActor* Source = Child->GetChildActor();
				const FTransform Where = Source ? Source->GetActorTransform() : Child->GetComponentTransform();
				FActorSpawnParameters Params;
				Params.OverrideLevel = Level;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AActor* Piece = World->SpawnActor<AActor>(PieceClass, Where, Params);
				if (!Piece)
				{
					UE_LOG(LogAEWArena, Warning, TEXT("could not spawn %s from %s.%s"), *PieceClass->GetName(), *SetLabel, *Child->GetName());
					continue;
				}
				if (Source)
				{
					EditorUtilities::CopyActorProperties(Source, Piece, EditorUtilities::FCopyOptions(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties));
				}
				Piece->SetActorTransform(Where);
				Piece->SetActorLabel(SetLabel + TEXT("_") + Child->GetName());
				Piece->SetFolderPath(PieceFolder);
				Piece->PostEditChange();
				if (IsBarricadePiece(Piece))
				{
					SetPieces.Add(Piece);
				}
				++Count;
			}
			if (Count == 0)
			{
				continue;
			}
			if (SetPieces.Num() > 0)
			{
				SpawnBarricadeSettings(World, Level, SetPieces[0]->GetClass(), SetPieces, {}, FName(*(FString(BarricadeSettingsPrefix) + SetLabel)), SetLabel, PieceFolder, Set->GetActorTransform(), true);
			}
			World->EditorDestroyActor(Set, true);
			Changed.Add(Level->GetOutermost());
			Pieces += Count;
			Done += FString::Printf(TEXT("\n  %s -> %d pieces (folder %s)"), *SetLabel, Count, *PieceFolder.ToString());
			UE_LOG(LogAEWArena, Log, TEXT("split %s into %d barricade pieces"), *SetLabel, Count);
		}
	}
	if (Changed.Num() > 0)
	{
		UEditorLoadingAndSavingUtils::SavePackages(Changed.Array(), false);
		GEditor->RedrawLevelEditingViewports();
		Notify(FString::Printf(TEXT("Barricade sets split into single pieces you can move, delete or swap:%s\nEach set has a <set>_Settings actor for climbing, barricade moves and knock-down."), *Done));
	}
	return Pieces;
}

int32 UAEWArenaTools::MakeSharedSublevelsOwn()
{
	UWorld* Current = OpenModArena();
	if (!Current)
	{
		return 0;
	}
	const FString MapPkg = Current->GetOutermost()->GetName();
	const FString Folder = FPackageName::GetLongPackagePath(MapPkg);
	const FString MapName = FPackageName::GetShortName(MapPkg);

	TArray<FString> Subs;
	TArray<FString> Empty;
	for (ULevelStreaming* LS : Current->GetStreamingLevels())
	{
		if (!LS || FPackageName::GetLongPackagePath(LS->GetWorldAssetPackageName()) == Folder || !ShouldOwnShared(LS))
		{
			continue;
		}
		const FString Sub = LS->GetWorldAssetPackageName();
		ULevel* Loaded = LS->GetLoadedLevel();
		UWorld* SubWorld = Loaded ? Cast<UWorld>(Loaded->GetOuter()) : nullptr;
		(SubWorld && CountActors(SubWorld) <= 2 ? Empty : Subs).Add(Sub);
	}
	for (const FString& E : Empty)
	{
		UE_LOG(LogAEWArena, Warning, TEXT("%s streams %s, an empty proxy - it stays shared until the template importer fills it"), *MapName, *FPackageName::GetShortName(E));
	}
	if (Subs.Num() == 0)
	{
		return 0;
	}
	for (ULevel* Level : Current->GetLevels())
	{
		if (Level && Level->GetOutermost()->IsDirty())
		{
			return 0;
		}
	}

	TGuardValue<bool> Busy(GArenaBusy, true);
	UEditorLoadingAndSavingUtils::NewBlankMap(false);
	UWorld* World = LoadWorld(MapPkg);
	if (!World)
	{
		UE_LOG(LogAEWArena, Error, TEXT("could not load %s"), *MapPkg);
		FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(MapPkg, FPackageName::GetMapPackageExtension()), false, true);
		return 0;
	}
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TArray<UPackage*> ToSave;
	TArray<UObject*> Copies;
	TArray<FString> Loaded;
	FString Done;
	for (ULevelStreaming* LS : World->GetStreamingLevels())
	{
		const FString Sub = LS ? LS->GetWorldAssetPackageName() : FString();
		if (!Subs.Contains(Sub))
		{
			continue;
		}
		Loaded.Add(Sub);
		const FString NewSub = Folder + TEXT("/") + MapName + TEXT("_") + FPackageName::GetShortName(Sub);
		if (FPackageName::DoesPackageExist(NewSub))
		{
			UE_LOG(LogAEWArena, Warning, TEXT("%s already exists - streaming it instead of copying again"), *NewSub);
			RepointStreamingLevel(LS, NewSub);
			Done += TEXT("\n  ") + FPackageName::GetShortName(NewSub) + TEXT(" (already there)");
			continue;
		}
		UWorld* Src = LoadWorld(Sub);
		if (!Src || CountActors(Src) <= 2)
		{
			UE_LOG(LogAEWArena, Warning, TEXT("%s is missing or an empty proxy - kept shared"), *Sub);
			continue;
		}
		UWorld* Dup = Cast<UWorld>(AssetTools.DuplicateAsset(FPackageName::GetShortName(NewSub), Folder, Src));
		if (!Dup)
		{
			UE_LOG(LogAEWArena, Error, TEXT("could not duplicate %s"), *Sub);
			continue;
		}
		ClearBuildData(Dup);
		ToSave.Add(Dup->GetOutermost());
		Copies.Add(Dup);
		Loaded.Add(NewSub);
		RepointStreamingLevel(LS, NewSub);
		Done += TEXT("\n  ") + FPackageName::GetShortName(Sub) + TEXT(" -> ") + FPackageName::GetShortName(NewSub);
		UE_LOG(LogAEWArena, Log, TEXT("%s -> %s"), *Sub, *NewSub);
	}
	ToSave.Insert(World->GetOutermost(), 0);

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Labels;
	AR.GetAssetsByPath(FName(*Folder), Labels, false);
	int32 Chunk = AEWPackaging::Settings()->ChunkId;
	FString LabelPkg = Folder + TEXT("/PAL_") + MapName;
	for (const FAssetData& A : Labels)
	{
		if (A.AssetClass == UPrimaryAssetLabel::StaticClass()->GetFName())
		{
			if (UPrimaryAssetLabel* L = Cast<UPrimaryAssetLabel>(A.GetAsset()))
			{
				LabelPkg = A.PackageName.ToString();
				Chunk = L->Rules.ChunkId;
			}
			break;
		}
	}
	if (Copies.Num() > 0)
	{
		TArray<FString> PackagingLog;
		AEWPackaging::UpdateLabel(LabelPkg, Chunk, Copies, true, false, ToSave, &PackagingLog);
		for (const FString& Line : PackagingLog)
		{
			UE_LOG(LogAEWArena, Log, TEXT("%s"), *Line);
		}
	}
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
	Loaded.Add(MapPkg);
	World = nullptr;
	Copies.Reset();
	UnloadMaps(Loaded);
	FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(MapPkg, FPackageName::GetMapPackageExtension()), false, true);

	if (!Done.IsEmpty())
	{
		UE_LOG(LogAEWArena, Log, TEXT("%s now streams its own copies of its shared sublevels:%s"), *MapName, *Done.Replace(TEXT("\n"), TEXT(" ")));
		Notify(FString::Printf(TEXT("%s now has its own copies of its shared sublevels:%s"), *MapName, *Done));
	}
	return Done.IsEmpty() ? 0 : Subs.Num();
}

#undef LOCTEXT_NAMESPACE
