#include "Modules/ModuleManager.h"
#include "AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "AcbEditorSession.h"
#include "AcbImportBridge.h"
#include "AcbCueDirtyFilter.h"
#include "UObject/Package.h"
#include "AcbEditorSettings.h"
#include "ISettingsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserDataMenuContexts.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SoundAtomCueSheet.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "SAcbEditor.h"

#define LOCTEXT_NAMESPACE "AEWAcbEditor"

static const FName AcbEditorTabName(TEXT("AEWAcbEditor"));

class FAEWAcbEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AcbEditorTabName,
			FOnSpawnTab::CreateRaw(this, &FAEWAcbEditorModule::SpawnTab))
			.SetDisplayName(LOCTEXT("TabTitle", "AEW ACB Editor"))
			.SetTooltipText(LOCTEXT("TabTip", "Edit and import ACB/AWB cue sheets"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAEWAcbEditorModule::RegisterMenus));

		PackageSavedHandle = UPackage::PackageSavedEvent.AddRaw(this, &FAEWAcbEditorModule::OnPackageSaved);

		FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRemovedHandle = Registry.Get().OnAssetRemoved().AddRaw(this, &FAEWAcbEditorModule::OnAssetRemoved);

		FAcbCueDirtyFilter::Get().Startup();

		if (ISettingsModule* SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "AEWAcbEditor",
				LOCTEXT("SettingsName", "AEW ACB Editor"),
				LOCTEXT("SettingsDescription", "AEW ACB Editor options."),
				GetMutableDefault<UAcbEditorSettings>());
		}
	}

	virtual void ShutdownModule() override
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "AEWAcbEditor");
		}
		UPackage::PackageSavedEvent.Remove(PackageSavedHandle);
		FAcbCueDirtyFilter::Get().Shutdown();
		if (FAssetRegistryModule* Registry = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
		{
			Registry->Get().OnAssetRemoved().Remove(AssetRemovedHandle);
		}
		if (RescanTickerHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(RescanTickerHandle);
		}
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AcbEditorTabName);
		}
	}

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
	{
		TSharedRef<SAcbEditor> Panel = SNew(SAcbEditor);
		Panel_ = Panel;
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[Panel];
	}

	void OnAssetRemoved(const FAssetData& Asset)
	{
		static const FName CueClass(TEXT("SoundAtomCue"));
		static const FName SheetClass(TEXT("SoundAtomCueSheet"));
		if (Asset.AssetClass != CueClass && Asset.AssetClass != SheetClass)
		{
			return;
		}
		const FString File = FPaths::ConvertRelativePathToFull(
			FPackageName::LongPackageNameToFilename(Asset.PackageName.ToString(), FPackageName::GetAssetPackageExtension()));
		if (!IFileManager::Get().FileExists(*File))
		{
			return;
		}
		FilesToRescan.AddUnique(File);
		if (!RescanTickerHandle.IsValid())
		{
			RescanTickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAEWAcbEditorModule::RescanLostAssets), 1.0f);
		}
	}

	bool RescanLostAssets(float)
	{
		RescanTickerHandle.Reset();
		TArray<FString> Files;
		for (const FString& File : FilesToRescan)
		{
			if (IFileManager::Get().FileExists(*File))
			{
				Files.Add(File);
			}
		}
		FilesToRescan.Reset();
		if (Files.Num() > 0)
		{
			FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			Registry.Get().ScanFilesSynchronous(Files, true);
			TArray<FString> Names;
			for (const FString& File : Files)
			{
				Names.Add(FPaths::GetBaseFilename(File));
			}
			UE_LOG(LogAEWAcbEditor, Display, TEXT("Re-registered %d cue asset(s) the editor lost track of while saving (their files were still on disk): %s"),
				Files.Num(), *FString::Join(Names, TEXT(", ")));
		}
		return false;
	}

	void OnPackageSaved(const FString&, UObject* Outer)
	{
		UPackage* Package = Cast<UPackage>(Outer);
		if (!Package || IsRunningCommandlet() || bSyncing)
		{
			return;
		}
		const FString Name = Package->GetName();
		if (!FindObject<USoundAtomCueSheet>(Package, *FPackageName::GetShortName(Name)))
		{
			return;
		}
		TGuardValue<bool> Guard(bSyncing, true);
		TArray<FString> Log;
		AcbImportBridge::SyncAwbPakRule(Name, Log);
		for (const FString& Line : Log)
		{
			UE_LOG(LogAEWAcbEditor, Display, TEXT("%s"), *Line);
		}
	}

	void ImportIntoFolder(const FString& ContentFolder)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(AcbEditorTabName);
		if (TSharedPtr<SAcbEditor> Panel = Panel_.Pin())
		{
			Panel->OpenForImportFolder(ContentFolder);
		}
	}

	static FString ToGameFolder(FString Path)
	{
		Path.RemoveFromStart(TEXT("/All"));
		Path.RemoveFromEnd(TEXT("/"));
		return Path.StartsWith(TEXT("/Game")) ? Path : FString();
	}

	void AddImportHereEntry(FToolMenuSection& Section, const FString& Folder)
	{
		Section.AddMenuEntry("AEWAcbImportHere",
			LOCTEXT("ImportHereLabel", "Import cue sheet here (ACB Editor)..."),
			FText::Format(LOCTEXT("ImportHereTip", "Import into {0}."),
				FText::FromString(Folder)),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Folder]() { ImportIntoFolder(Folder); })));
	}

	void OpenWith(const FString& AcbPath, const FString& CueSheetFolder)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(AcbEditorTabName);
		if (TSharedPtr<SAcbEditor> Panel = Panel_.Pin())
		{
			Panel->OpenFile(AcbPath, FString(), CueSheetFolder);
		}
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
		{
			FToolMenuSection& Section = WindowMenu->FindOrAddSection("AEWFightForever");
			Section.Label = LOCTEXT("SectionLabel", "AEW Fight Forever");
			Section.AddMenuEntry("OpenAEWAcbEditor",
				LOCTEXT("OpenLabel", "AEW ACB Editor"),
				LOCTEXT("OpenTip", "Edit and import ACB/AWB cue sheets"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(AcbEditorTabName); })));
		}

		if (UToolMenu* FolderMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu"))
		{
			FToolMenuSection& Section = FolderMenu->FindOrAddSection("AEWAcbEditor");
			Section.Label = LOCTEXT("ContentSectionLabel", "AEW Fight Forever");
			Section.AddDynamicEntry("AEWAcbImportHereFolder", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
			{
				const UContentBrowserDataMenuContext_FolderMenu* Context = InSection.FindContext<UContentBrowserDataMenuContext_FolderMenu>();
				FName PackagePath;
				if (!Context || Context->SelectedItems.Num() != 1 || !Context->SelectedItems[0].Legacy_TryGetPackagePath(PackagePath))
				{
					return;
				}
				const FString Folder = ToGameFolder(PackagePath.ToString());
				if (!Folder.IsEmpty())
				{
					AddImportHereEntry(InSection, Folder);
				}
			}));
		}

		if (UToolMenu* AddNewMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AddNewContextMenu"))
		{
			FToolMenuSection& Section = AddNewMenu->FindOrAddSection("AEWAcbEditor");
			Section.Label = LOCTEXT("ContentSectionLabel", "AEW Fight Forever");
			Section.AddDynamicEntry("AEWAcbImportHereAddNew", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
			{
				const UContentBrowserDataMenuContext_AddNewMenu* Context = InSection.FindContext<UContentBrowserDataMenuContext_AddNewMenu>();
				if (!Context || Context->SelectedPaths.Num() != 1)
				{
					return;
				}
				const FString Folder = ToGameFolder(Context->SelectedPaths[0].ToString());
				if (!Folder.IsEmpty())
				{
					AddImportHereEntry(InSection, Folder);
				}
			}));
		}

		if (UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SoundAtomCueSheet"))
		{
			FToolMenuSection& Section = AssetMenu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry("AEWEditAcb", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
			{
				const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context)
				{
					return;
				}
				FString SourceFile;
				FString SheetFolder;
				FString SheetPackage;
				for (const TWeakObjectPtr<UObject>& Object : Context->SelectedObjects)
				{
					if (USoundAtomCueSheet* Sheet = Cast<USoundAtomCueSheet>(Object.Get()))
					{
						if (Sheet->AssetImportData)
						{
							SourceFile = Sheet->AssetImportData->GetFirstFilename();
						}
						SheetPackage = Sheet->GetOutermost()->GetName();
						SheetFolder = FPackageName::GetLongPackagePath(SheetPackage);
						break;
					}
				}
				if (SheetPackage.IsEmpty())
				{
					return;
				}
				if (SourceFile.IsEmpty())
				{
					return;
				}
				InSection.AddMenuEntry("AEWEditAcb",
					LOCTEXT("EditAcbLabel", "Edit in ACB Editor..."),
					FText::Format(LOCTEXT("EditAcbTip", "Open {0} in the AEW ACB Editor"), FText::FromString(SourceFile)),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([this, SourceFile, SheetFolder]()
					{
						if (FPaths::FileExists(SourceFile))
						{
							OpenWith(SourceFile, SheetFolder);
						}
					})));
			}));
		}
	}

	TWeakPtr<SAcbEditor> Panel_;
	FDelegateHandle PackageSavedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle RescanTickerHandle;
	TArray<FString> FilesToRescan;
	bool bSyncing = false;
};

IMPLEMENT_MODULE(FAEWAcbEditorModule, AEWAcbEditor)

#undef LOCTEXT_NAMESPACE
