#include "Modules/ModuleManager.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "EditorStyleSet.h"
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
#include "AEWModdingSettings.h"
#include "SChunkNamesPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "AEWModdingCore"

DEFINE_LOG_CATEGORY_STATIC(LogAEWModding, Log, All);

class FAEWModdingCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAEWModdingCoreModule::RegisterMenus));
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SChunkNamesPanel::TabName,
			FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) { return SNew(SDockTab).TabRole(ETabRole::NomadTab)[ SNew(SChunkNamesPanel) ]; }))
			.SetDisplayName(LOCTEXT("ChunkNamesTab", "Package Project"))
			.SetTooltipText(LOCTEXT("ChunkNamesTabTip", "Package the project and archive its chunks."))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		FCookModificationDelegate& Cook = FGameDelegates::Get().GetCookModificationDelegate();
		if (Cook.IsBound())
		{
			UE_LOG(LogAEWModding, Warning, TEXT("Cook delegate already bound; labelled assets not added to the cook."));
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
			LastUatLog = IFileManager::Get().GetTimeStamp(*AEWPackaging::UatLogPath());
			WatchHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAEWModdingCoreModule::WatchPackaging), 3.f);
		}
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SChunkNamesPanel::TabName);
		}
		UPackage::PackageSavedEvent.Remove(SavedHandle);
		if (WatchHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(WatchHandle);
			WatchHandle.Reset();
		}
		if (MenuRetryHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(MenuRetryHandle);
			MenuRetryHandle.Reset();
		}
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
	FDelegateHandle WatchHandle;
	FDelegateHandle MenuRetryHandle;
	int32 MenuTries = 0;

	bool TakeOverPackageMenu()
	{
		UToolMenu* File = UToolMenus::Get()->ExtendMenu("MainFrame.MainTabMenu.File");
		FToolMenuSection* Section = File ? File->FindSection("FileProject") : nullptr;
		FToolMenuEntry* Original = Section ? Section->FindEntry("PackageProject") : nullptr;
		if (!Original)
		{
			return false;
		}
		FToolMenuOwnerScoped OwnerScoped(this);
		if (!Section->FindEntry("PackageProjectEngine"))
		{
			FToolMenuEntry Engine = *Original;
			Engine.Name = "PackageProjectEngine";
			Engine.Label = LOCTEXT("EnginePackage", "Package Project (Unreal)");
			Section->AddEntry(Engine);
		}
		Section->AddEntry(FToolMenuEntry::InitMenuEntry("PackageProject",
			LOCTEXT("PackageProjectFile", "Package Project"),
			LOCTEXT("PackageProjectFileTip", "Package the project and archive its chunks."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.PackageProject"),
			FUIAction(FExecuteAction::CreateStatic(&AEWPackaging::OpenChunkNames))));
		return true;
	}
	FDateTime LastUatLog;

	bool WatchPackaging(float)
	{
		const FString UatLog = AEWPackaging::UatLogPath();
		const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*UatLog);
		if (Stamp == FDateTime::MinValue() || Stamp <= LastUatLog)
		{
			return true;
		}
		if (!AEWPackaging::UatLogShowsSuccess(UatLog))
		{
			return true;
		}
		LastUatLog = Stamp;
		if (!AEWPackaging::Settings()->bRenamePackagedChunks && !AEWPackaging::Settings()->bArchivePackagedChunks)
		{
			return true;
		}
		TArray<FString> Log;
		const int32 Count = AEWPackaging::ProcessLastPackage(Log);
		if (Count < 0)
		{
			return true;
		}
		for (const FString& Line : Log)
		{
			UE_LOG(LogAEWModding, Display, TEXT("%s"), *Line);
		}
		FNotificationInfo Info(FText::FromString(Log.Num() > 0 ? Log[0] : FString()));
		Info.ExpireDuration = 10.f;
		Info.bUseLargeFont = false;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(Count > 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_None);
		}
		return true;
	}

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
			FNotificationInfo Info(FText::FromString(FString::Printf(TEXT("%d asset(s) used by labelled content aren't labelled and won't be cooked. See LogAEWModding."), Missing)));
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
		if (!TakeOverPackageMenu())
		{
			MenuRetryHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
			{
				if (TakeOverPackageMenu() || ++MenuTries > 60)
				{
					MenuRetryHandle.Reset();
					return false;
				}
				return true;
			}), 0.5f);
		}
		FToolMenuOwnerScoped OwnerScoped(this);
		AEWMenus::AddGroupedToolEntry("Cook", LOCTEXT("CookGroup", "Cook"), "AEWCookClearUnlabelledChunks",
			LOCTEXT("ClearChunks", "Clear Unlabelled Chunk Assignments"),
			LOCTEXT("ClearChunksTip", "Find and clear chunk IDs not set by a label. Asks first."),
			FUIAction(FExecuteAction::CreateStatic(&FAEWModdingCoreModule::ClearChunks)));
		AEWMenus::AddGroupedToolEntry("Cook", LOCTEXT("CookGroup", "Cook"), "AEWCookNameChunks",
			LOCTEXT("NameChunks", "Package Project"),
			LOCTEXT("NameChunksTip", "Package the project and archive its chunks."),
			FUIAction(FExecuteAction::CreateStatic(&AEWPackaging::OpenChunkNames)));
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
