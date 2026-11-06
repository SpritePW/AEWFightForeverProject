#include "AEWPackaging.h"
#include "AEWModdingSettings.h"
#include "AEWTableUtils.h"

#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/CompositeDataTable.h"
#include "Engine/DataTable.h"
#include "Engine/PrimaryAssetLabel.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Settings/ProjectPackagingSettings.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Commandlets/ChunkDependencyInfo.h"

namespace AEWPackaging
{
	static void AddLog(TArray<FString>* Log, const FString& Line)
	{
		if (Log)
		{
			Log->Add(Line);
		}
	}

	static FString NameList(TArray<FString> Names, int32 Max = 30)
	{
		Names.Sort();
		TArray<FString> Lines;
		for (int32 i = 0; i < Names.Num() && i < Max; ++i)
		{
			Lines.Add(TEXT("    ") + Names[i]);
		}
		if (Names.Num() > Max)
		{
			Lines.Add(FString::Printf(TEXT("    ... and %d more"), Names.Num() - Max));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	static TArray<FString> ObjectNames(const TArray<UObject*>& Objects)
	{
		TArray<FString> Names;
		for (UObject* Object : Objects)
		{
			if (Object)
			{
				Names.Add(Object->GetName());
			}
		}
		return Names;
	}

	static void AddSection(TArray<FString>& Log, const FString& Header, const TArray<FString>& Names)
	{
		if (Names.Num() == 0)
		{
			return;
		}
		Log.Add(TEXT(""));
		Log.Add(Header);
		Log.Add(NameList(Names));
	}

	static FString NormDir(FString D)
	{
		D.TrimStartAndEndInline();
		D.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (D.EndsWith(TEXT("/")))
		{
			D.LeftChopInline(1);
		}
		return D;
	}

	const UAEWModdingSettings* Settings()
	{
		return GetDefault<UAEWModdingSettings>();
	}

	int32 NextFreeChunkId()
	{
		const UAEWModdingSettings* S = Settings();
		TSet<int32> Used;
		Used.Add(S->NewTablesChunkId);
		Used.Add(S->BaseTablesChunkId);
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Labels;
		AR.GetAssetsByClass(UPrimaryAssetLabel::StaticClass()->GetFName(), Labels, true);
		for (const FAssetData& A : Labels)
		{
			if (UPrimaryAssetLabel* L = Cast<UPrimaryAssetLabel>(A.GetAsset()))
			{
				if (L->Rules.ChunkId > 0)
				{
					Used.Add(L->Rules.ChunkId);
				}
			}
		}
		int32 Id = FMath::Max(1, S->ChunkId);
		while (Used.Contains(Id))
		{
			++Id;
		}
		return Id;
	}

	int32 ChunkForLabel(const FString& LabelPackage)
	{
		const FString PackageName = NormDir(LabelPackage);
		const FString Name = FPackageName::GetShortName(PackageName);
		if (UPrimaryAssetLabel* L = LoadObject<UPrimaryAssetLabel>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (L->Rules.ChunkId > 0)
			{
				return L->Rules.ChunkId;
			}
		}
		return NextFreeChunkId();
	}

	UPrimaryAssetLabel* UpdateLabel(const FString& LabelPackage, int32 ChunkId, const TArray<UObject*>& Assets,
		bool bLabelDirectory, bool bRecursive, TArray<UPackage*>& ToSave, TArray<FString>* Log)
	{
		const FString PackageName = NormDir(LabelPackage);
		const FString Folder = FPackageName::GetLongPackagePath(PackageName);
		const FString Name = FPackageName::GetShortName(PackageName);
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			AddLog(Log, TEXT("warning: invalid label path ") + PackageName);
			return nullptr;
		}

		UPrimaryAssetLabel* Label = LoadObject<UPrimaryAssetLabel>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const bool bNew = Label == nullptr;
		if (!Label)
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			Label = Cast<UPrimaryAssetLabel>(AssetTools.CreateAsset(Name, Folder, UPrimaryAssetLabel::StaticClass(), nullptr));
		}
		if (!Label)
		{
			AddLog(Log, TEXT("warning: could not create ") + PackageName);
			return nullptr;
		}

		Label->Modify();
		Label->Rules.ChunkId = ChunkId;
		if (bNew || Label->Rules.CookRule == EPrimaryAssetCookRule::Unknown)
		{
			Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
		}
		Label->Rules.bApplyRecursively = bRecursive;
		Label->Rules.Priority = 10;
		Label->bLabelAssetsInMyDirectory = bLabelDirectory;
		int32 Added = 0;
		for (UObject* Object : Assets)
		{
			if (!Object)
			{
				continue;
			}
			const TSoftObjectPtr<UObject> Soft(Object);
			if (!Label->ExplicitAssets.Contains(Soft))
			{
				Label->ExplicitAssets.Add(Soft);
				++Added;
			}
		}
		Label->MarkPackageDirty();
		ToSave.AddUnique(Label->GetOutermost());
		if (bNew || Added > 0)
		{
			AddLog(Log, FString::Printf(TEXT("%s%s (chunk %d): %d asset(s) added%s"), *Name, bNew ? TEXT(" [new]") : TEXT(""), ChunkId, Added,
				bLabelDirectory ? (bRecursive ? TEXT(", plus its folder and sub-folders") : TEXT(", plus its folder")) : TEXT("")));
		}

		TArray<FString> Opened;
		AllowCooking(PackageName, &Opened);
		AllowCookingOf(Assets, &Opened);
		for (const FString& Dir : Opened)
		{
			AddLog(Log, TEXT("Now cookable (removed from 'Directories to never cook'): ") + Dir);
		}
		return Label;
	}

	static bool IsNewTable(const UObject* Object)
	{
		if (!Object || !Object->IsA<UDataTable>() || Object->IsA<UCompositeDataTable>())
		{
			return false;
		}
		const FString Path = Object->GetOutermost()->GetName();
		return Object->GetName().EndsWith(TEXT("_New")) || Path.Contains(TEXT("/New/"));
	}

	static void CollectWithParents(UObject* Object, TArray<UObject*>& All)
	{
		if (!Object || All.Contains(Object))
		{
			return;
		}
		All.Add(Object);
		if (UDataTable* Table = Cast<UDataTable>(Object))
		{
			for (UDataTable* Parent : AEWTables::GetParents(Table))
			{
				CollectWithParents(Parent, All);
			}
		}
	}

	static bool IsUnchangedComposite(UDataTable* Table)
	{
		if (!Cast<UCompositeDataTable>(Table))
		{
			return false;
		}
		bool bHasNew = false;
		for (UDataTable* Parent : AEWTables::GetParents(Table))
		{
			if (IsNewTable(Parent))
			{
				bHasNew = true;
				if (Parent->GetRowMap().Num() > 0)
				{
					return false;
				}
			}
		}
		return bHasNew;
	}

	static TArray<UObject*> PruneUnchangedTables(TArray<UObject*>& All, TArray<FString>* Log)
	{
		TArray<UObject*> Dropped;
		TSet<UObject*> Drop;
		TSet<UObject*> Keep;
		for (UObject* Object : All)
		{
			UDataTable* Table = Cast<UDataTable>(Object);
			if (!Cast<UCompositeDataTable>(Table))
			{
				continue;
			}
			const bool bUnchanged = IsUnchangedComposite(Table);
			(bUnchanged ? Drop : Keep).Add(Table);
			for (UDataTable* Parent : AEWTables::GetParents(Table))
			{
				(bUnchanged ? Drop : Keep).Add(Parent);
			}
		}
		for (UObject* Object : All)
		{
			if (IsNewTable(Object) && CastChecked<UDataTable>(Object)->GetRowMap().Num() == 0 && !Keep.Contains(Object))
			{
				Drop.Add(Object);
			}
		}
		for (int32 i = All.Num() - 1; i >= 0; --i)
		{
			if (Drop.Contains(All[i]) && !Keep.Contains(All[i]))
			{
				Dropped.Add(All[i]);
				All.RemoveAt(i);
			}
		}
		if (Log && Dropped.Num() > 0)
		{
			AddSection(*Log, FString::Printf(TEXT("Not labelled, no new rows (%d):"), Dropped.Num()), ObjectNames(Dropped));
		}
		return Dropped;
	}

	static void RemoveFromLabel(const FString& LabelPackage, const TArray<UObject*>& Objects, TArray<UPackage*>& ToSave)
	{
		if (LabelPackage.IsEmpty() || Objects.Num() == 0)
		{
			return;
		}
		const FString Package = NormDir(LabelPackage);
		UPrimaryAssetLabel* Label = LoadObject<UPrimaryAssetLabel>(nullptr, *(Package + TEXT(".") + FPackageName::GetShortName(Package)), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Label)
		{
			return;
		}
		const int32 Removed = Label->ExplicitAssets.RemoveAll([&Objects](const TSoftObjectPtr<UObject>& Soft)
		{
			return Objects.ContainsByPredicate([&Soft](UObject* O) { return Soft.ToSoftObjectPath() == FSoftObjectPath(O); });
		});
		if (Removed > 0)
		{
			Label->Modify();
			Label->MarkPackageDirty();
			ToSave.AddUnique(Label->GetOutermost());
		}
	}

	static void SplitTables(const TArray<UObject*>& All, TArray<UObject*>& OutNew, TArray<UObject*>& OutBase)
	{
		for (UObject* Object : All)
		{
			(IsNewTable(Object) ? OutNew : OutBase).AddUnique(Object);
		}
	}

	UPrimaryAssetLabel* LabelSharedTables(const TArray<UObject*>& Objects, TArray<UPackage*>& ToSave, TArray<FString>* Log)
	{
		const UAEWModdingSettings* S = Settings();
		TArray<UObject*> All;
		for (UObject* Object : Objects)
		{
			CollectWithParents(Object, All);
		}
		const TArray<UObject*> Dropped = PruneUnchangedTables(All, Log);
		RemoveFromLabel(S->NewTablesLabel, Dropped, ToSave);
		RemoveFromLabel(S->BaseTablesLabel, Dropped, ToSave);
		TArray<UObject*> NewTables;
		TArray<UObject*> BaseTables;
		SplitTables(All, NewTables, BaseTables);
		UPrimaryAssetLabel* Last = nullptr;
		if (NewTables.Num() > 0 && !S->NewTablesLabel.IsEmpty())
		{
			Last = UpdateLabel(S->NewTablesLabel, S->NewTablesChunkId, NewTables, false, false, ToSave, Log);
		}
		if (BaseTables.Num() > 0 && !S->BaseTablesLabel.IsEmpty())
		{
			Last = UpdateLabel(S->BaseTablesLabel, S->BaseTablesChunkId, BaseTables, false, false, ToSave, Log);
		}
		return Last;
	}

	static void ReplaceTableEntries(const FString& LabelPackage, int32 ChunkId, const TArray<UObject*>& Tables, TArray<UPackage*>& ToSave, TArray<FString>* Log, TArray<FString>* Cookable)
	{
		if (LabelPackage.IsEmpty())
		{
			return;
		}
		UPrimaryAssetLabel* Label = UpdateLabel(LabelPackage, ChunkId, {}, false, false, ToSave, Log);
		if (!Label)
		{
			return;
		}
		const int32 Before = Label->ExplicitAssets.Num();
		Label->ExplicitAssets.RemoveAll([](const TSoftObjectPtr<UObject>& Soft)
		{
			UObject* Object = Soft.Get();
			if (!Object)
			{
				Object = Soft.LoadSynchronous();
			}
			return Object == nullptr || Object->IsA<UDataTable>();
		});
		for (UObject* Table : Tables)
		{
			Label->ExplicitAssets.AddUnique(TSoftObjectPtr<UObject>(Table));
		}
		Label->MarkPackageDirty();
		if (Log)
		{
			const int32 Diff = Tables.Num() - Before;
			AddSection(*Log, FString::Printf(TEXT("%s, chunk %d: %d table(s)%s"), *Label->GetName(), ChunkId, Tables.Num(),
				Diff == 0 ? TEXT("") : *FString::Printf(TEXT(" (%+d)"), Diff)), ObjectNames(Tables));
			if (Tables.Num() == 0)
			{
				Log->Add(TEXT(""));
				Log->Add(FString::Printf(TEXT("%s, chunk %d: empty"), *Label->GetName(), ChunkId));
			}
		}
		AllowCookingOf(Tables, Cookable);
	}

	int32 RebuildTableLabels(TArray<FString>& Log)
	{
		const UAEWModdingSettings* S = Settings();
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		for (const FDirectoryPath& Folder : S->TableFolders)
		{
			const FString Dir = NormDir(Folder.Path);
			if (!Dir.IsEmpty())
			{
				AR.GetAssetsByPath(FName(*Dir), Assets, true);
			}
		}

		TArray<UDataTable*> Tables;
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClass == UDataTable::StaticClass()->GetFName() || Asset.AssetClass == UCompositeDataTable::StaticClass()->GetFName())
			{
				if (UDataTable* Table = Cast<UDataTable>(Asset.GetAsset()))
				{
					Tables.AddUnique(Table);
				}
			}
		}

		TArray<UObject*> All;
		for (UDataTable* Table : Tables)
		{
			if (IsNewTable(Table))
			{
				All.AddUnique(Table);
			}
			else if (Cast<UCompositeDataTable>(Table) && AEWTables::GetParents(Table).ContainsByPredicate([](UDataTable* P) { return IsNewTable(P); }))
			{
				CollectWithParents(Table, All);
			}
		}

		for (const FSoftObjectPath& Path : S->AlwaysLabelTables)
		{
			if (UDataTable* Table = Cast<UDataTable>(Path.TryLoad()))
			{
				All.AddUnique(Table);
			}
		}

		TArray<FString> Body;
		TArray<FString> Cookable;
		PruneUnchangedTables(All, &Body);
		TArray<FString> Skipped = Body;
		Body.Reset();
		TArray<UObject*> NewTables;
		TArray<UObject*> BaseTables;
		SplitTables(All, NewTables, BaseTables);
		TArray<UPackage*> ToSave;
		ReplaceTableEntries(S->NewTablesLabel, S->NewTablesChunkId, NewTables, ToSave, &Body, &Cookable);
		ReplaceTableEntries(S->BaseTablesLabel, S->BaseTablesChunkId, BaseTables, ToSave, &Body, &Cookable);
		UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);

		Log.Add(FString::Printf(TEXT("Table labels rebuilt: %d of %d table(s) labelled."), NewTables.Num() + BaseTables.Num(), Tables.Num()));
		Log.Append(Body);
		Log.Append(Skipped);
		AddSection(Log, TEXT("Removed from 'Directories to never cook':"), Cookable);
		return NewTables.Num() + BaseTables.Num();
	}

	static bool IsUnder(const FString& Path, const FString& Dir)
	{
		return Path == Dir || Path.StartsWith(Dir + TEXT("/"));
	}

	bool AllowCooking(const FString& PackageName, TArray<FString>* Log)
	{
		UProjectPackagingSettings* PS = GetMutableDefault<UProjectPackagingSettings>();
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		const FString Folder = FPackageName::GetLongPackagePath(PackageName);
		bool bChanged = false;

		auto AddNever = [PS](const FString& Dir)
		{
			if (!PS->DirectoriesToNeverCook.ContainsByPredicate([&Dir](const FDirectoryPath& D) { return NormDir(D.Path) == Dir; }))
			{
				FDirectoryPath Path;
				Path.Path = Dir;
				PS->DirectoriesToNeverCook.Add(Path);
			}
		};

		for (int32 Guard = 0; Guard < 64; ++Guard)
		{
			const int32 Index = PS->DirectoriesToNeverCook.IndexOfByPredicate([&Folder](const FDirectoryPath& D) { return IsUnder(Folder, NormDir(D.Path)); });
			if (Index == INDEX_NONE)
			{
				break;
			}
			FString Current = NormDir(PS->DirectoriesToNeverCook[Index].Path);
			PS->DirectoriesToNeverCook.RemoveAt(Index);
			bChanged = true;
			while (true)
			{
				FString Next;
				if (Current != Folder)
				{
					const FString Rest = Folder.Mid(Current.Len() + 1);
					FString First, Tail;
					Next = Rest.Split(TEXT("/"), &First, &Tail) ? Current + TEXT("/") + First : Current + TEXT("/") + Rest;
				}
				TArray<FString> Subs;
				AR.GetSubPaths(Current, Subs, false);
				for (const FString& Sub : Subs)
				{
					const FString Dir = NormDir(Sub);
					if (Dir != Next)
					{
						AddNever(Dir);
					}
				}
				if (Current == Folder)
				{
					break;
				}
				Current = Next;
			}
		}

		if (bChanged)
		{
			PS->SaveConfig();
			PS->UpdateDefaultConfigFile();
			AddLog(Log, Folder);
		}
		return bChanged;
	}

	struct FKeptPath
	{
		FString Path;
		bool bRecursive = false;
	};

	static bool IsCookedGamePackage(const FString& PackageName)
	{
		FString File;
		if (!FPackageName::DoesPackageExist(PackageName, nullptr, &File))
		{
			return true;
		}
		return IFileManager::Get().FileExists(*FPaths::ChangeExtension(File, TEXT(".uexp")));
	}

	int32 CollectLabelledPackages(TArray<FString>& OutPackages)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Labels;
		AR.GetAssetsByClass(UPrimaryAssetLabel::StaticClass()->GetFName(), Labels, true);
		TSet<FName> Seen;
		auto Add = [&](FName Package)
		{
			if (!Package.IsNone() && !Seen.Contains(Package))
			{
				Seen.Add(Package);
				OutPackages.Add(Package.ToString());
			}
		};
		int32 Count = 0;
		for (const FAssetData& A : Labels)
		{
			UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset());
			if (!Label || Label->Rules.CookRule != EPrimaryAssetCookRule::AlwaysCook)
			{
				continue;
			}
			++Count;
			if (Label->bLabelAssetsInMyDirectory)
			{
				TArray<FAssetData> InDir;
				AR.GetAssetsByPath(FName(*FPackageName::GetLongPackagePath(A.PackageName.ToString())), InDir, true);
				for (const FAssetData& D : InDir)
				{
					if (D.PackageName != A.PackageName && D.AssetClass != UPrimaryAssetLabel::StaticClass()->GetFName())
					{
						Add(D.PackageName);
					}
				}
			}
			for (const TSoftObjectPtr<UObject>& Soft : Label->ExplicitAssets)
			{
				if (!Soft.IsNull())
				{
					Add(FName(*Soft.ToSoftObjectPath().GetLongPackageName()));
				}
			}
		}
		return Count;
	}

	static FString DetachChunksFromChunkZero(const TSet<int32>& InChunks)
	{
		TArray<int32> Chunks = InChunks.Array();
		Chunks.Sort();
		if (Chunks.Num() == 0)
		{
			return FString();
		}
		if (Chunks.Num() == 1)
		{
			Chunks.Add(Chunks[0] + 1);
		}
		UChunkDependencyInfo* Info = GetMutableDefault<UChunkDependencyInfo>();
		auto Key = [](const TArray<FChunkDependency>& Array)
		{
			TArray<FString> Parts;
			for (const FChunkDependency& D : Array)
			{
				Parts.Add(FString::Printf(TEXT("%d>%d"), D.ChunkID, D.ParentChunkID));
			}
			Parts.Sort();
			return FString::Join(Parts, TEXT(","));
		};
		const FString Before = Key(Info->DependencyArray);
		Info->DependencyArray.RemoveAll([&Chunks](const FChunkDependency& D) { return Chunks.Contains(D.ChunkID); });
		for (int32 i = 0; i < Chunks.Num(); ++i)
		{
			FChunkDependency Dep;
			Dep.ChunkID = Chunks[i];
			Dep.ParentChunkID = Chunks[(i + 1) % Chunks.Num()];
			Info->DependencyArray.Add(Dep);
		}
		if (Key(Info->DependencyArray) != Before)
		{
			Info->SaveConfig();
			Info->UpdateDefaultConfigFile();
		}
		TArray<FString> Ids;
		for (int32 Id : Chunks)
		{
			Ids.Add(LexToString(Id));
		}
		return FString::Join(Ids, TEXT(", "));
	}

	int32 CookOnlyLabelledContent(TArray<FString>& Log, int32* OutMissing)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		UProjectPackagingSettings* PS = GetMutableDefault<UProjectPackagingSettings>();
		const FString Root = TEXT("/Game");

		TArray<FKeptPath> Kept;
		TSet<FName> KeptPackages;
		auto KeepPackage = [&](const FString& Package)
		{
			if (IsUnder(Package, Root))
			{
				Kept.Add({ FPackageName::GetLongPackagePath(Package), false });
				KeptPackages.Add(FName(*Package));
			}
		};

		TArray<FAssetData> Labels;
		AR.GetAssetsByClass(UPrimaryAssetLabel::StaticClass()->GetFName(), Labels, true);
		int32 LabelCount = 0;
		TSet<int32> ModChunks;
		TArray<UPackage*> Relaxed;
		TArray<FString> RelaxedNames;
		for (const FAssetData& A : Labels)
		{
			UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset());
			if (Label && Label->Rules.CookRule == EPrimaryAssetCookRule::NeverCook)
			{
				Label->Modify();
				Label->Rules.CookRule = EPrimaryAssetCookRule::DevelopmentCook;
				Label->MarkPackageDirty();
				Relaxed.Add(Label->GetOutermost());
				RelaxedNames.Add(A.AssetName.ToString());
			}
		}
		if (Relaxed.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(Relaxed, false);
			AddSection(Log, TEXT("Unticked labels changed from Never Cook to 'cook only if referenced' (their folders stay excluded):"), RelaxedNames);
		}
		for (const FAssetData& A : Labels)
		{
			UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset());
			if (!Label || Label->Rules.CookRule == EPrimaryAssetCookRule::NeverCook || Label->Rules.CookRule == EPrimaryAssetCookRule::DevelopmentCook)
			{
				continue;
			}
			++LabelCount;
			if (Label->Rules.ChunkId > 0)
			{
				ModChunks.Add(Label->Rules.ChunkId);
			}
			const FString Package = A.PackageName.ToString();
			KeepPackage(Package);
			if (Label->bLabelAssetsInMyDirectory)
			{
				const FString Dir = FPackageName::GetLongPackagePath(Package);
				Kept.Add({ Dir, true });
				TArray<FAssetData> InDir;
				AR.GetAssetsByPath(FName(*Dir), InDir, true);
				for (const FAssetData& D : InDir)
				{
					KeptPackages.Add(D.PackageName);
				}
			}
			for (const TSoftObjectPtr<UObject>& Soft : Label->ExplicitAssets)
			{
				if (!Soft.IsNull())
				{
					KeepPackage(Soft.ToSoftObjectPath().GetLongPackageName());
				}
			}
		}
		for (const FFilePath& Map : PS->MapsToCook)
		{
			KeepPackage(Map.FilePath);
		}
		for (const FDirectoryPath& Dir : PS->DirectoriesToAlwaysCook)
		{
			const FString D = NormDir(Dir.Path);
			if (IsUnder(D, Root))
			{
				Kept.Add({ D, true });
			}
		}

		TArray<FString> Never;
		TFunction<void(const FString&)> Visit = [&](const FString& Dir)
		{
			bool bHasKept = false;
			for (const FKeptPath& K : Kept)
			{
				if (K.bRecursive && IsUnder(Dir, K.Path))
				{
					return;
				}
				if (IsUnder(K.Path, Dir))
				{
					bHasKept = true;
				}
			}
			if (!bHasKept)
			{
				Never.Add(Dir);
				return;
			}
			TArray<FString> Subs;
			AR.GetSubPaths(Dir, Subs, false);
			for (const FString& Sub : Subs)
			{
				Visit(NormDir(Sub));
			}
		};
		Visit(Root);
		Never.Sort();

		TArray<FDirectoryPath> NewList;
		for (const FDirectoryPath& D : PS->DirectoriesToNeverCook)
		{
			if (!IsUnder(NormDir(D.Path), Root))
			{
				NewList.Add(D);
			}
		}
		for (const FString& D : Never)
		{
			FDirectoryPath Path;
			Path.Path = D;
			NewList.Add(Path);
		}
		auto ListKey = [](const TArray<FDirectoryPath>& List)
		{
			TArray<FString> Parts;
			for (const FDirectoryPath& D : List)
			{
				Parts.Add(NormDir(D.Path));
			}
			return FString::Join(Parts, TEXT("|"));
		};
		const bool bWasCookAll = PS->bCookAll;
		TArray<FString> LabelDirs;
		for (const FAssetData& A : Labels)
		{
			UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset());
			if (Label && Label->bLabelAssetsInMyDirectory && A.PackageName.ToString().StartsWith(TEXT("/Game/")))
			{
				LabelDirs.Add(FPackageName::GetLongPackagePath(A.PackageName.ToString()));
			}
		}
		TArray<FString> DroppedMaps;
		const int32 MapsBefore = PS->MapsToCook.Num();
		PS->MapsToCook.RemoveAll([&LabelDirs, &DroppedMaps](const FFilePath& Map)
		{
			FString Package = Map.FilePath;
			Package.Split(TEXT("."), &Package, nullptr);
			bool bLabelled = false;
			for (const FString& Dir : LabelDirs)
			{
				bLabelled = bLabelled || Package.StartsWith(Dir + TEXT("/"));
			}
			if (bLabelled || (Package.StartsWith(TEXT("/Game/")) && !FPackageName::DoesPackageExist(Package)))
			{
				DroppedMaps.Add(Package);
				return true;
			}
			return false;
		});
		if (DroppedMaps.Num() > 0)
		{
			AddSection(Log, TEXT("Taken out of 'List of maps to include' (a label decides whether they cook, or the map no longer exists):"), DroppedMaps);
		}
		const bool bSettingsChanged = bWasCookAll || PS->bCookMapsOnly || ListKey(PS->DirectoriesToNeverCook) != ListKey(NewList) || PS->MapsToCook.Num() != MapsBefore;
		PS->DirectoriesToNeverCook = NewList;
		PS->bCookAll = false;
		PS->bCookMapsOnly = false;
		if (bSettingsChanged)
		{
			PS->SaveConfig();
			PS->UpdateDefaultConfigFile();
		}

		const FString ChunkLine = DetachChunksFromChunkZero(ModChunks);

		auto IsNever = [&Never](const FString& Package)
		{
			const FString Dir = FPackageName::GetLongPackagePath(Package);
			for (const FString& N : Never)
			{
				if (IsUnder(Dir, N))
				{
					return true;
				}
			}
			return false;
		};

		TArray<FName> Queue = KeptPackages.Array();
		TSet<FName> Seen(KeptPackages);
		TArray<FString> Missing;
		while (Queue.Num() > 0)
		{
			const FName Package = Queue.Pop(false);
			TArray<FName> Deps;
			AR.GetDependencies(Package, Deps, UE::AssetRegistry::EDependencyCategory::Package);
			for (const FName Dep : Deps)
			{
				if (Seen.Contains(Dep))
				{
					continue;
				}
				Seen.Add(Dep);
				const FString DepName = Dep.ToString();
				if (!IsUnder(DepName, Root) || IsCookedGamePackage(DepName))
				{
					continue;
				}
				if (IsNever(DepName))
				{
					Missing.Add(FString::Printf(TEXT("%s (used by %s)"), *DepName, *Package.ToString()));
				}
				Queue.Add(Dep);
			}
		}

		Log.Add(Missing.Num() > 0
			? FString::Printf(TEXT("Cook settings updated - %d unlabelled asset(s) need attention."), Missing.Num())
			: TEXT("Cook settings updated - only labelled content will be cooked."));
		Log.Add(TEXT(""));
		Log.Add(FString::Printf(TEXT("Labels: %d"), LabelCount));
		Log.Add(FString::Printf(TEXT("Mod chunks: %s"), ChunkLine.IsEmpty() ? TEXT("none") : *ChunkLine));
		Log.Add(FString::Printf(TEXT("Never-cook folders: %d"), Never.Num()));
		Log.Add(FString::Printf(TEXT("Cook everything: %s"), bWasCookAll ? TEXT("turned off") : TEXT("off")));
		if (Missing.Num() > 0)
		{
			AddSection(Log, TEXT("Used but not labelled, so not cooked (move them into a labelled folder or add them to a label):"), Missing);
		}
		if (OutMissing)
		{
			*OutMissing = Missing.Num();
		}
		return Never.Num();
	}

	void AllowCookingOf(const TArray<UObject*>& Objects, TArray<FString>* Log)
	{
		TSet<FString> Done;
		for (UObject* Object : Objects)
		{
			if (Object)
			{
				const FString Name = Object->GetOutermost()->GetName();
				if (!Done.Contains(Name))
				{
					Done.Add(Name);
					AllowCooking(Name, Log);
				}
			}
		}
	}
}
