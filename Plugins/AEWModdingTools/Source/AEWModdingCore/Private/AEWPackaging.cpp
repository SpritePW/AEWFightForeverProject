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
		Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
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
		AddLog(Log, FString::Printf(TEXT("Label %s%s: chunk %d, %d asset(s) added%s."), *Name, bNew ? TEXT(" (new)") : TEXT(""), ChunkId, Added,
			bLabelDirectory ? (bRecursive ? TEXT(", labels its folder and sub-folders") : TEXT(", labels its folder")) : TEXT("")));

		if (Settings()->bAllowCookingLabelledAssets)
		{
			AllowCooking(PackageName, Log);
			AllowCookingOf(Assets, Log);
		}
		return Label;
	}

	static bool IsNewTable(const UObject* Object)
	{
		return Object && Object->IsA<UDataTable>() && !Object->IsA<UCompositeDataTable>() && Object->GetName().EndsWith(TEXT("_New"));
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
		if (!Settings()->bSkipUnchangedTables)
		{
			return Dropped;
		}
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
			TArray<FString> Names;
			for (UObject* Object : Dropped)
			{
				Names.Add(Object->GetName());
			}
			Names.Sort();
			Log->Add(FString::Printf(TEXT("Skipped %d unchanged table(s) (their _New table is empty): %s"), Names.Num(), *FString::Join(Names, TEXT(", "))));
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
		if (!S->bCreateLabels || !S->bLabelSharedTables)
		{
			return nullptr;
		}
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

	static void ReplaceTableEntries(const FString& LabelPackage, int32 ChunkId, const TArray<UObject*>& Tables, TArray<UPackage*>& ToSave, TArray<FString>* Log)
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
		AddLog(Log, FString::Printf(TEXT("%s: %d table(s) (was %d entries, chunk %d)."), *Label->GetName(), Tables.Num(), Before, ChunkId));
		if (Settings()->bAllowCookingLabelledAssets)
		{
			AllowCookingOf(Tables, Log);
		}
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
		if (S->bRebuildAllTables)
		{
			for (UDataTable* Table : Tables)
			{
				All.AddUnique(Table);
			}
		}
		else
		{
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
		}

		PruneUnchangedTables(All, &Log);
		TArray<UObject*> NewTables;
		TArray<UObject*> BaseTables;
		SplitTables(All, NewTables, BaseTables);
		TArray<UPackage*> ToSave;
		ReplaceTableEntries(S->NewTablesLabel, S->NewTablesChunkId, NewTables, ToSave, &Log);
		ReplaceTableEntries(S->BaseTablesLabel, S->BaseTablesChunkId, BaseTables, ToSave, &Log);
		UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		Log.Add(FString::Printf(TEXT("Table labels rebuilt from %d table(s) found: %d _New table(s) in chunk %d, %d other table(s) in chunk %d."),
			Tables.Num(), NewTables.Num(), S->NewTablesChunkId, BaseTables.Num(), S->BaseTablesChunkId));
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
			AddLog(Log, TEXT("Packaging: narrowed 'Directories to never cook' so ") + Folder + TEXT(" can be cooked."));
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
			return TEXT("No label chunks to detach from chunk 0.");
		}
		if (Chunks.Num() == 1)
		{
			Chunks.Add(Chunks[0] + 1);
		}
		UChunkDependencyInfo* Info = GetMutableDefault<UChunkDependencyInfo>();
		Info->DependencyArray.RemoveAll([&Chunks](const FChunkDependency& D) { return Chunks.Contains(D.ChunkID); });
		for (int32 i = 0; i < Chunks.Num(); ++i)
		{
			FChunkDependency Dep;
			Dep.ChunkID = Chunks[i];
			Dep.ParentChunkID = Chunks[(i + 1) % Chunks.Num()];
			Info->DependencyArray.Add(Dep);
		}
		Info->SaveConfig();
		Info->UpdateDefaultConfigFile();
		TArray<FString> Ids;
		for (int32 Id : Chunks)
		{
			Ids.Add(LexToString(Id));
		}
		return FString::Printf(TEXT("Chunks %s no longer count as children of chunk 0, so assets the editor loads at startup (tables, enums) stay in their label's chunk instead of falling back to chunk 0 (DefaultEngine.ini, ChunkDependencyInfo)."), *FString::Join(Ids, TEXT(", ")));
	}

	int32 CookOnlyLabelledContent(TArray<FString>& Log)
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
		for (const FAssetData& A : Labels)
		{
			UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset());
			if (!Label || Label->Rules.CookRule == EPrimaryAssetCookRule::NeverCook)
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
		PS->DirectoriesToNeverCook = NewList;
		const bool bWasCookAll = PS->bCookAll;
		PS->bCookAll = false;
		PS->bCookMapsOnly = false;
		PS->SaveConfig();
		PS->UpdateDefaultConfigFile();

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

		Log.Add(FString::Printf(TEXT("Cook only labelled content: %d label(s), %d folder(s) set to never cook%s."),
			LabelCount, Never.Num(), bWasCookAll ? TEXT(", 'Cook everything' turned off") : TEXT("")));
		Log.Add(TEXT("Only labelled assets, Maps To Cook and Directories To Always Cook are cooked; game assets they use are left to the game's own paks."));
		Log.Add(ChunkLine);
		if (Missing.Num() > 0)
		{
			Missing.Sort();
			Log.Add(FString::Printf(TEXT("warning: %d asset(s) you made or edited are used but not labelled, so they won't be cooked - move them into a labelled folder or add them to a label:"), Missing.Num()));
			for (int32 i = 0; i < Missing.Num() && i < 40; ++i)
			{
				Log.Add(TEXT("  ") + Missing[i]);
			}
			if (Missing.Num() > 40)
			{
				Log.Add(FString::Printf(TEXT("  ... and %d more"), Missing.Num() - 40));
			}
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
