#include "AEWPackaging.h"
#include "AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/PrimaryAssetLabel.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Settings/ProjectPackagingSettings.h"
#include "UObject/Package.h"

namespace AEWChunkCleanup
{
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

	static bool IsUnder(const FString& Path, const FString& Dir)
	{
		return Path == Dir || Path.StartsWith(Dir + TEXT("/"));
	}

	static bool IsCookedGamePackage(const FString& PackageName)
	{
		FString File;
		if (!FPackageName::DoesPackageExist(PackageName, nullptr, &File))
		{
			return true;
		}
		return IFileManager::Get().FileExists(*FPaths::ChangeExtension(File, TEXT(".uexp")));
	}

	static int32 Run(bool bAsk, TArray<FString>& Log)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		const UProjectPackagingSettings* PS = GetDefault<UProjectPackagingSettings>();

		TArray<FString> LabelLines;
		TArray<FAssetData> Labels;
		AR.GetAssetsByClass(UPrimaryAssetLabel::StaticClass()->GetFName(), Labels, true);
		for (const FAssetData& A : Labels)
		{
			if (UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset()))
			{
				LabelLines.Add(FString::Printf(TEXT("  %s: chunk %d"), *A.PackageName.ToString(), Label->Rules.ChunkId));
			}
		}
		LabelLines.Sort();

		auto IsNeverCooked = [PS](const FString& Package)
		{
			for (const FDirectoryPath& D : PS->DirectoriesToNeverCook)
			{
				if (IsUnder(Package, NormDir(D.Path)))
				{
					return true;
				}
			}
			return false;
		};

		TMap<FName, TArray<int32>> PackageChunks;
		TArray<FAssetData> All;
		AR.GetAssetsByPath(FName(TEXT("/Game")), All, true);
		for (const FAssetData& A : All)
		{
			if (A.ChunkIDs.Num() > 0)
			{
				PackageChunks.FindOrAdd(A.PackageName) = A.ChunkIDs;
			}
		}
		TArray<FName> ToClear;
		TArray<FString> SkippedCooked;
		TArray<FString> PackageLines;
		for (const TPair<FName, TArray<int32>>& P : PackageChunks)
		{
			const FString Name = P.Key.ToString();
			TArray<FString> Ids;
			for (int32 Id : P.Value)
			{
				Ids.Add(LexToString(Id));
			}
			if (IsCookedGamePackage(Name) && IsNeverCooked(Name))
			{
				SkippedCooked.Add(Name);
				continue;
			}
			ToClear.Add(P.Key);
			PackageLines.Add(FString::Printf(TEXT("  %s: chunk %s"), *Name, *FString::Join(Ids, TEXT(","))));
		}
		PackageLines.Sort();

		UAssetManagerSettings* AMS = GetMutableDefault<UAssetManagerSettings>();
		TArray<FString> RuleLines;
		int32 RuleCount = 0;
		{
			for (const FPrimaryAssetTypeInfo& T : AMS->PrimaryAssetTypesToScan)
			{
				if (T.Rules.ChunkId != INDEX_NONE)
				{
					RuleLines.Add(FString::Printf(TEXT("  Asset Manager rule type %s: chunk %d"), *T.PrimaryAssetType.ToString(), T.Rules.ChunkId));
				}
			}
			for (const FPrimaryAssetRulesOverride& O : AMS->PrimaryAssetRules)
			{
				if (O.Rules.ChunkId != INDEX_NONE)
				{
					RuleLines.Add(FString::Printf(TEXT("  Asset Manager rule %s: chunk %d"), *O.PrimaryAssetId.ToString(), O.Rules.ChunkId));
				}
			}
			for (const FPrimaryAssetRulesCustomOverride& O : AMS->CustomPrimaryAssetRules)
			{
				if (O.Rules.ChunkId != INDEX_NONE)
				{
					RuleLines.Add(FString::Printf(TEXT("  Asset Manager custom rule %s: chunk %d"), *O.PrimaryAssetType.ToString(), O.Rules.ChunkId));
				}
			}
			RuleCount = RuleLines.Num();
		}

		Log.Add(FString::Printf(TEXT("Labels (kept, %d):"), LabelLines.Num()));
		Log.Append(LabelLines);
		if (ToClear.Num() + RuleCount == 0)
		{
			Log.Add(TEXT("No chunk assignments outside the labels were found."));
			if (SkippedCooked.Num() > 0)
			{
				Log.Add(FString::Printf(TEXT("(%d never-cooked game package(s) left as they are.)"), SkippedCooked.Num()));
			}
			return 0;
		}

		TArray<FString> Report;
		Report.Add(FString::Printf(TEXT("Chunk IDs outside labels (%d package(s), %d Asset Manager rule(s)):"), ToClear.Num(), RuleCount));
		for (int32 i = 0; i < PackageLines.Num() && i < 30; ++i)
		{
			Report.Add(PackageLines[i]);
		}
		if (PackageLines.Num() > 30)
		{
			Report.Add(FString::Printf(TEXT("  ... and %d more package(s)"), PackageLines.Num() - 30));
		}
		Report.Append(RuleLines);

		if (bAsk)
		{
			const FString Question = FString::Join(Log, TEXT("\n")) + TEXT("\n\n") + FString::Join(Report, TEXT("\n")) +
				TEXT("\n\nClear them?");
			if (FMessageDialog::Open(EAppMsgType::OkCancel, FText::FromString(Question)) != EAppReturnType::Ok)
			{
				Log.Add(TEXT("Cancelled."));
				return 0;
			}
		}
		Log.Append(Report);

		TArray<UPackage*> ToSave;
		TArray<FString> Failed;
		for (const FName& Name : ToClear)
		{
			UPackage* Package = LoadPackage(nullptr, *Name.ToString(), LOAD_None);
			if (!Package)
			{
				Failed.Add(Name.ToString());
				continue;
			}
			Package->SetChunkIDs(TArray<int32>());
			Package->MarkPackageDirty();
			ToSave.Add(Package);
		}
		if (ToSave.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
			for (UPackage* Package : ToSave)
			{
				if (Package->IsDirty())
				{
					Failed.Add(Package->GetName());
					continue;
				}
				if (!Package->HasAnyPackageFlags(PKG_FilterEditorOnly))
				{
					FString File;
					if (FPackageName::DoesPackageExist(Package->GetName(), nullptr, &File))
					{
						const FString Uexp = FPaths::ChangeExtension(File, TEXT(".uexp"));
						if (IFileManager::Get().FileExists(*Uexp))
						{
							IFileManager::Get().Delete(*Uexp, false, true, true);
						}
					}
				}
			}
		}

		if (RuleCount > 0)
		{
			for (FPrimaryAssetTypeInfo& T : AMS->PrimaryAssetTypesToScan)
			{
				T.Rules.ChunkId = INDEX_NONE;
			}
			for (FPrimaryAssetRulesOverride& O : AMS->PrimaryAssetRules)
			{
				O.Rules.ChunkId = INDEX_NONE;
			}
			for (FPrimaryAssetRulesCustomOverride& O : AMS->CustomPrimaryAssetRules)
			{
				O.Rules.ChunkId = INDEX_NONE;
			}
			AMS->SaveConfig();
			AMS->UpdateDefaultConfigFile();
			if (UAssetManager::IsValid())
			{
				UAssetManager::Get().ReinitializeFromConfig();
			}
		}

		Log.Add(FString::Printf(TEXT("Cleared chunk IDs from %d package(s) and %d rule(s)."), ToSave.Num() - Failed.Num(), RuleCount));
		if (Failed.Num() > 0)
		{
			Log.Add(FString::Printf(TEXT("warning: %d package(s) couldn't be re-saved:"), Failed.Num()));
			for (int32 i = 0; i < Failed.Num() && i < 20; ++i)
			{
				Log.Add(TEXT("  ") + Failed[i]);
			}
		}
		return ToSave.Num() - Failed.Num() + RuleCount;
	}
}

int32 AEWPackaging::ClearUnlabelledChunks(bool bAsk, TArray<FString>& Log)
{
	return AEWChunkCleanup::Run(bAsk, Log);
}
