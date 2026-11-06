#include "Modules/ModuleManager.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "AEWPackaging.h"
#include "AEWTableUtils.h"
#include "AEWModdingSettings.h"
#include "GameDelegates.h"
#include "AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/PlatformTime.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "AEWModdingCore"

DEFINE_LOG_CATEGORY_STATIC(LogAEWModding, Log, All);

class FAEWModdingCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAEWModdingCoreModule::RegisterMenus));

		FCookModificationDelegate& Cook = FGameDelegates::Get().GetCookModificationDelegate();
		if (Cook.IsBound())
		{
			UE_LOG(LogAEWModding, Warning, TEXT("The cook modification delegate is already bound - labelled assets are not added to the cook."));
		}
		else
		{
			Cook.BindStatic(&FAEWModdingCoreModule::AddLabelledToCook);
			bBoundCook = true;
		}

		if (!IsRunningCommandlet())
		{
			IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
			SavedHandle = UPackage::PackageSavedEvent.AddRaw(this, &FAEWModdingCoreModule::OnPackageSaved);
			RemovedHandle = AR.OnAssetRemoved().AddRaw(this, &FAEWModdingCoreModule::OnAssetRemoved);
			RenamedHandle = AR.OnAssetRenamed().AddRaw(this, &FAEWModdingCoreModule::OnAssetRenamed);
			if (AR.IsLoadingAssets())
			{
				FilesLoadedHandle = AR.OnFilesLoaded().AddRaw(this, &FAEWModdingCoreModule::OnFilesLoaded);
			}
			else
			{
				Queue(false);
			}
		}
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		UPackage::PackageSavedEvent.Remove(SavedHandle);
		if (TickHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(TickHandle);
			TickHandle.Reset();
		}
		if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
		{
			IAssetRegistry& AR = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
			AR.OnAssetRemoved().Remove(RemovedHandle);
			AR.OnAssetRenamed().Remove(RenamedHandle);
			AR.OnFilesLoaded().Remove(FilesLoadedHandle);
		}
		if (bBoundCook)
		{
			FGameDelegates::Get().GetCookModificationDelegate().Unbind();
		}
	}

private:
	bool bBoundCook = false;
	bool bRunning = false;
	bool bTablesPending = false;
	double DueAt = 0.0;
	int32 LastMissing = 0;
	FDelegateHandle SavedHandle;
	FDelegateHandle RemovedHandle;
	FDelegateHandle RenamedHandle;
	FDelegateHandle FilesLoadedHandle;
	FDelegateHandle TickHandle;

	static bool IsTablePath(const FString& PackageName)
	{
		const UAEWModdingSettings* S = AEWPackaging::Settings();
		for (const FDirectoryPath& Folder : S->TableFolders)
		{
			FString Dir = Folder.Path;
			Dir.RemoveFromEnd(TEXT("/"));
			if (!Dir.IsEmpty() && (PackageName == Dir || PackageName.StartsWith(Dir + TEXT("/"))))
			{
				return true;
			}
		}
		for (const FSoftObjectPath& Path : S->AlwaysLabelTables)
		{
			if (Path.GetLongPackageName() == PackageName)
			{
				return true;
			}
		}
		return false;
	}

	void Queue(bool bTables)
	{
		bTablesPending = bTablesPending || bTables;
		DueAt = FPlatformTime::Seconds() + 2.0;
		if (!TickHandle.IsValid())
		{
			TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAEWModdingCoreModule::ProcessQueue), 0.5f);
		}
	}

	void OnFilesLoaded()
	{
		Queue(false);
	}

	void OnPackageSaved(const FString& Filename, UObject* Outer)
	{
		UPackage* Package = Cast<UPackage>(Outer);
		if (bRunning || !Package || Filename.Contains(TEXT("Autosaves")))
		{
			return;
		}
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects, false);
		bool bTable = false;
		bool bLabel = false;
		for (UObject* Object : Objects)
		{
			bTable = bTable || Object->IsA<UDataTable>();
			bLabel = bLabel || Object->IsA<UPrimaryAssetLabel>();
		}
		if (bTable && IsTablePath(Package->GetName()))
		{
			Queue(true);
		}
		else if (bLabel)
		{
			Queue(false);
		}
	}

	void OnAssetChanged(const FAssetData& Asset)
	{
		if (bRunning)
		{
			return;
		}
		UClass* Class = Asset.GetClass();
		if (Class && Class->IsChildOf(UDataTable::StaticClass()) && IsTablePath(Asset.PackageName.ToString()))
		{
			Queue(true);
		}
		else if (Class && Class->IsChildOf(UPrimaryAssetLabel::StaticClass()))
		{
			Queue(false);
		}
	}

	void OnAssetRemoved(const FAssetData& Asset)
	{
		OnAssetChanged(Asset);
	}

	void OnAssetRenamed(const FAssetData& Asset, const FString& OldPath)
	{
		OnAssetChanged(Asset);
	}

	bool ProcessQueue(float)
	{
		if ((GEditor && GEditor->PlayWorld) || GIsSavingPackage || IsGarbageCollecting() || FPlatformTime::Seconds() < DueAt
			|| FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().IsLoadingAssets())
		{
			return true;
		}
		TickHandle.Reset();
		TGuardValue<bool> Running(bRunning, true);
		TArray<FString> Log;
		if (bTablesPending)
		{
			bTablesPending = false;
			AEWPackaging::RebuildTableLabels(Log);
		}
		int32 Missing = 0;
		AEWPackaging::CookOnlyLabelledContent(Log, &Missing);
		for (const FString& Line : Log)
		{
			if (!Line.IsEmpty())
			{
				UE_LOG(LogAEWModding, Log, TEXT("%s"), *Line);
			}
		}
		if (Missing > 0 && Missing != LastMissing)
		{
			FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("%d asset(s) used by your labelled content aren't in a label, so they won't be cooked. See the Output Log (LogAEWModding) for the list."), Missing)));
			Info.ExpireDuration = 10.f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
		LastMissing = Missing;
		return false;
	}

	static void AddLabelledToCook(TArray<FString>& ExtraPackagesToCook)
	{
		const int32 Before = ExtraPackagesToCook.Num();
		const int32 Labels = AEWPackaging::CollectLabelledPackages(ExtraPackagesToCook);
		UE_LOG(LogAEWModding, Display, TEXT("Cook: added %d package(s) from %d Always Cook label(s)."), ExtraPackagesToCook.Num() - Before, Labels);
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		AEWMenus::AddGroupedToolEntry("Cook", LOCTEXT("CookGroup", "Cook"), "AEWCookChooseLabels",
			LOCTEXT("ChooseLabels", "Choose What To Cook..."),
			LOCTEXT("ChooseLabelsTip", "Tick the labels (arenas, edit parts, cue sheets, tables) to cook and package; unticked ones are left out of the cook completely."),
			FUIAction(FExecuteAction::CreateLambda([]() { AEWPackaging::ChooseLabelsToCook(); })));
		AEWMenus::AddGroupedToolEntry("Cook", LOCTEXT("CookGroup", "Cook"), "AEWCookClearUnlabelledChunks",
			LOCTEXT("ClearChunks", "Clear Unlabelled Chunk Assignments"),
			LOCTEXT("ClearChunksTip", "List and clear every chunk assignment that doesn't come from a PrimaryAssetLabel: chunk IDs stored in packages (old 'Assign to Chunk') and chunk IDs on Asset Manager rules. Asks before changing anything."),
			FUIAction(FExecuteAction::CreateStatic(&FAEWModdingCoreModule::ClearChunks)));
	}

	static void ClearChunks()
	{
		TArray<FString> Log;
		AEWPackaging::ClearUnlabelledChunks(true, Log);
		Show(LOCTEXT("ClearChunksTitle", "Clear Unlabelled Chunk Assignments"), Log);
	}

	static void Show(const FText& Title, const TArray<FString>& Log)
	{
		for (const FString& Line : Log)
		{
			if (!Line.IsEmpty())
			{
				UE_LOG(LogAEWModding, Log, TEXT("%s"), *Line);
			}
		}
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Join(Log, TEXT("\n"))), &Title);
	}

};

IMPLEMENT_MODULE(FAEWModdingCoreModule, AEWModdingCore)

#undef LOCTEXT_NAMESPACE
