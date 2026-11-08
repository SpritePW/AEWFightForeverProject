#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "AEWArenaTools.h"
#include "AEWArenaMakerSettings.h"
#include "AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/World.h"
#include "AEWTableUtils.h"
#include "SArenaMakerPanel.h"

#define LOCTEXT_NAMESPACE "AEWArenaMaker"

static const FName ArenaTemplateTabName(TEXT("AEWArenaMaker"));

class FAEWArenaMakerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ArenaTemplateTabName,
			FOnSpawnTab::CreateRaw(this, &FAEWArenaMakerModule::SpawnTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Arena Maker"))
			.SetTooltipText(LOCTEXT("TabTip", "Create new arenas from templates"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAEWArenaMakerModule::RegisterMenus));

		FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		RemovedHandle = AssetRegistry.Get().OnAssetRemoved().AddRaw(this, &FAEWArenaMakerModule::OnAssetRemoved);
		SaveHandle = FEditorDelegates::PostSaveWorld.AddRaw(this, &FAEWArenaMakerModule::OnPostSaveWorld);
		OpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &FAEWArenaMakerModule::OnMapOpened);
		if (!IsRunningCommandlet() && !GetDefault<UAEWArenaMakerSettings>()->bExistingArenasConverted)
		{
			PassTickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAEWArenaMakerModule::ProcessPass), 2.f);
		}
	}

	virtual void ShutdownModule() override
	{
		FEditorDelegates::PostSaveWorld.Remove(SaveHandle);
		FEditorDelegates::OnMapOpened.Remove(OpenedHandle);
		if (PassTickHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(PassTickHandle);
			PassTickHandle.Reset();
		}
		if (OwnTickHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(OwnTickHandle);
			OwnTickHandle.Reset();
		}
		if (SwapTickHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(SwapTickHandle);
			SwapTickHandle.Reset();
		}
		if (TickHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(TickHandle);
			TickHandle.Reset();
		}
		if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
		{
			FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().OnAssetRemoved().Remove(RemovedHandle);
		}
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ArenaTemplateTabName);
		}
	}

private:
	void OnAssetRemoved(const FAssetData& Asset)
	{
		if (UAEWArenaTools::IsBusy() || Asset.AssetClass != UWorld::StaticClass()->GetFName() || !GetDefault<UAEWArenaMakerSettings>()->bSyncOnDelete)
		{
			return;
		}
		PendingLevels.Add(Asset.AssetName);
		if (!TickHandle.IsValid())
		{
			TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAEWArenaMakerModule::ProcessRemoved), 0.5f);
		}
	}

	void OnPostSaveWorld(uint32 SaveFlags, UWorld* World, bool bSuccess)
	{
		if (!bSuccess || !World || IsRunningCommandlet() || (SaveFlags & SAVE_FromAutosave) || UAEWArenaTools::IsBusy() || SwapTickHandle.IsValid())
		{
			return;
		}
		if (!GEditor || World != GEditor->GetEditorWorldContext().World())
		{
			return;
		}
		SwapTickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAEWArenaMakerModule::ProcessSwaps), 0.f);
	}

	void OnMapOpened(const FString& Filename, bool bAsTemplate)
	{
		if (bAsTemplate || IsRunningCommandlet() || UAEWArenaTools::IsBusy() || UAEWArenaTools::IsConvertingArenas() || OwnTickHandle.IsValid())
		{
			return;
		}
		OwnTickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAEWArenaMakerModule::ProcessOwn), 0.5f);
	}

	bool ProcessPass(float)
	{
		if (!GEditor || GEditor->PlayWorld || !FSlateApplication::IsInitialized() || UAEWArenaTools::IsBusy()
			|| FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().IsLoadingAssets())
		{
			return true;
		}
		UWorld* World = GEditor->GetEditorWorldContext().World();
		if (World && World->GetOutermost()->IsDirty())
		{
			return true;
		}
		PassTickHandle.Reset();
		UAEWArenaTools::ConvertExistingArenas();
		return false;
	}

	bool ProcessOwn(float)
	{
		if ((GEditor && GEditor->PlayWorld) || FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().IsLoadingAssets())
		{
			return true;
		}
		OwnTickHandle.Reset();
		UAEWArenaTools::MakeSharedSublevelsOwn();
		UAEWArenaTools::SplitBarricadeSets();
		return false;
	}

	bool ProcessSwaps(float)
	{
		SwapTickHandle.Reset();
		if (GEditor && !GEditor->PlayWorld)
		{
			UAEWArenaTools::UpdateEntranceSwaps(false);
		}
		return false;
	}

	bool ProcessRemoved(float)
	{
		if ((GEditor && GEditor->PlayWorld) || FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().IsLoadingAssets())
		{
			return true;
		}
		const TSet<FName> Levels = MoveTemp(PendingLevels);
		PendingLevels.Reset();
		TickHandle.Reset();
		UAEWArenaTools::HandleRemovedLevels(Levels);
		return false;
	}

	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
	{
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[ SNew(SArenaMakerPanel) ];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		AEWMenus::AddToolEntry("OpenAEWArenaMaker",
			LOCTEXT("OpenLabel", "Arena Maker"),
			LOCTEXT("OpenTip", "Copy an imported arena template into a new arena."),
			FUIAction(FExecuteAction::CreateStatic(&UAEWArenaTools::OpenDialog)));
		const FText ArenaLabel = LOCTEXT("ArenaGroup", "Arena");
		const FText OverridesLabel = LOCTEXT("OverridesGroup", "Overrides");
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaStartMarkers",
			LOCTEXT("StartMarkers", "Add Start Markers"),
			LOCTEXT("StartMarkersTip", "Place markers to move the player and referee starts in this arena."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddStartMarkers(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaCameraOverride",
			LOCTEXT("CameraOverride", "Add Camera Override"),
			LOCTEXT("CameraOverrideTip", "Add a camera table override for this arena."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddCameraOverride(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaEntranceOverride",
			LOCTEXT("EntranceOverride", "Add Entrance Override"),
			LOCTEXT("EntranceOverrideTip", "Place markers to move the stage, crowd and ring entrances in this arena."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddEntranceOverride(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaWeaponPoints",
			LOCTEXT("WeaponPoints", "Add Weapon Point Markers"),
			LOCTEXT("WeaponPointsTip", "Place markers to move weapon and ladder/thumbtack points in this arena."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddWeaponPointMarkers(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaRuleOverride",
			LOCTEXT("RuleNodes", "Add Rule Override"),
			LOCTEXT("RuleNodesTip", "Add Level Blueprint nodes that change the match rule in this arena."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddMatchRuleOverrideNodes(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaRemoveOverrides",
			LOCTEXT("RemoveOverrides", "Remove Overrides..."),
			LOCTEXT("RemoveOverridesTip", "Choose overrides to remove from the open arena."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::RemoveOverrides(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaMakeBarricade",
			LOCTEXT("MakeBarricade", "Make Barricade From Selected"),
			LOCTEXT("MakeBarricadeTip", "Turn the selected meshes into barricades."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::MakeBarricadeFromSelected(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaBarricadeSettings",
			LOCTEXT("BarricadeSettings", "Add Barricade Settings To Selected"),
			LOCTEXT("BarricadeSettingsTip", "Add a Settings actor to the selected barricade pieces."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddBarricadeSettingsToSelected(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaRestoreMeshes",
			LOCTEXT("RestoreMeshes", "Restore Meshes From Barricades"),
			LOCTEXT("RestoreMeshesTip", "Remove barricade pieces from the selected meshes (all if none selected)."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::RestoreMeshesFromBarricades(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaFillAudience",
			LOCTEXT("FillAudience", "Fill Audience From Selected"),
			LOCTEXT("FillAudienceTip", "Fill around the selected Empty Actor with crowd, facing its X arrow."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::FillAudienceFromSelected(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaClearAudience",
			LOCTEXT("ClearAudience", "Clear Generated Audience"),
			LOCTEXT("ClearAudienceTip", "Remove generated crowd from the selection (all if none selected)."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::ClearGeneratedAudience(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaCameraPreview",
			LOCTEXT("CameraPreview", "Toggle Camera Preview"),
			LOCTEXT("CameraPreviewTip", "Show where the gameplay camera settles. Run again to remove."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::ToggleCameraPreview(); })));

		UToolMenus* Menus = UToolMenus::Get();
		const FString ArenaMenu = TEXT("LevelEditor.MainMenu.Window.AEWModdingTools.Arena");
		for (const TCHAR* Context : { TEXT("LevelEditor.ActorContextMenu"), TEXT("LevelEditor.EmptySelectionContextMenu") })
		{
			if (UToolMenu* Menu = Menus->ExtendMenu(Context))
			{
				FToolMenuSection& Section = Menu->FindOrAddSection("AEWFightForever");
				Section.Label = LOCTEXT("ContextSection", "AEW Fight Forever");
				if (!Section.FindEntry("AEWArena"))
				{
					Section.AddSubMenu("AEWArena", ArenaLabel, LOCTEXT("ContextArenaTip", "Arena Maker tools for the open arena"), FNewToolMenuChoice());
				}
			}
			const FString Child = FString(Context) + TEXT(".AEWArena");
			if (!Menus->IsMenuRegistered(FName(*Child)))
			{
				Menus->RegisterMenu(FName(*Child), FName(*ArenaMenu));
			}
			if (!Menus->IsMenuRegistered(FName(*(Child + TEXT(".Overrides")))))
			{
				Menus->RegisterMenu(FName(*(Child + TEXT(".Overrides"))), FName(*(ArenaMenu + TEXT(".Overrides"))));
			}
		}
	}

	FDelegateHandle RemovedHandle;
	FDelegateHandle TickHandle;
	FDelegateHandle SaveHandle;
	FDelegateHandle SwapTickHandle;
	FDelegateHandle OpenedHandle;
	FDelegateHandle OwnTickHandle;
	FDelegateHandle PassTickHandle;
	TSet<FName> PendingLevels;
};

IMPLEMENT_MODULE(FAEWArenaMakerModule, AEWArenaMaker)

#undef LOCTEXT_NAMESPACE
