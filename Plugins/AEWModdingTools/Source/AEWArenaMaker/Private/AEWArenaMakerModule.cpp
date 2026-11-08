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
			.SetTooltipText(LOCTEXT("TabTip", "Create new arenas from imported templates"))
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
			LOCTEXT("OpenTip", "Copy an imported arena template (map + its own sublevels) into a new arena and add it to the game's arena list."),
			FUIAction(FExecuteAction::CreateStatic(&UAEWArenaTools::OpenDialog)));
		const FText ArenaLabel = LOCTEXT("ArenaGroup", "Arena");
		const FText OverridesLabel = LOCTEXT("OverridesGroup", "Overrides");
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaStartMarkers",
			LOCTEXT("StartMarkers", "Add Start Markers"),
			LOCTEXT("StartMarkersTip", "Create BP_StartMarker in the open arena's folder and place one on every player and referee start of ArenaMatch. Move the ones you want to change and delete the rest; in game they move ArenaMatch's starts for this arena only."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddStartMarkers(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaCameraOverride",
			LOCTEXT("CameraOverride", "Add Camera Override"),
			LOCTEXT("CameraOverrideTip", "Create BP_CameraOverride and a copy of the gameplay camera table in the open arena's folder and place the override in the map. In game the camera uses that table in this arena only; tune its rows with Toggle Camera Preview."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddCameraOverride(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaEntranceOverride",
			LOCTEXT("EntranceOverride", "Add Entrance Override"),
			LOCTEXT("EntranceOverrideTip", "Create BP_EntranceOverride in the open arena's folder and place EntranceOrigins for the stage, crowd and ring entrances. Move them to your set; in game the entrances in this arena move with them. Force Stage Entrances is written to the game's entrance conversion table every time you save the map."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddEntranceOverride(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaWeaponPoints",
			LOCTEXT("WeaponPoints", "Add Weapon Point Markers"),
			LOCTEXT("WeaponPointsTip", "Create BP_WeaponPointMarker in the open arena's folder and place a marker on every Lights Out weapon spawn point and every ladder / thumbtack placement point. Move the ones you want to change; in game they move those points for this arena only."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddWeaponPointMarkers(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaRuleOverride",
			LOCTEXT("RuleNodes", "Add Rule Override"),
			LOCTEXT("RuleNodesTip", "Add nodes to the open arena's Level Blueprint that change the match rule for matches in this arena: on BeginPlay they read the game's match rule, pass it through 'Set members in FMatchRule' (tick the fields you want to change) and set it back; on EndPlay they restore the original."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddMatchRuleOverrideNodes(); })));
		AEWMenus::AddSubGroupedToolEntry("Arena", ArenaLabel, "Overrides", OverridesLabel, "AEWArenaRemoveOverrides",
			LOCTEXT("RemoveOverrides", "Remove Overrides..."),
			LOCTEXT("RemoveOverridesTip", "Choose which overrides to take out of the open arena: their placed actors, their blueprints and camera table in the arena's folder, the rule override nodes and the arena's entrance swaps."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::RemoveOverrides(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaMakeBarricade",
			LOCTEXT("MakeBarricade", "Make Barricade From Selected"),
			LOCTEXT("MakeBarricadeTip", "Turn the selected mesh actors into barricades: each mesh stays as it is and invisible game barricade pieces (one per barricade length) are placed along it, facing the ring, with their reaction, hit and weapon boxes fitted. A <mesh>_Barricade_Settings actor is added next to it: its Details (Arena Override section) has Can Be Climbed, Allow Barricade Moves, Can Be Knocked Down and Starts Down."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::MakeBarricadeFromSelected(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaBarricadeSettings",
			LOCTEXT("BarricadeSettings", "Add Barricade Settings To Selected"),
			LOCTEXT("BarricadeSettingsTip", "Give the selected barricade pieces (e.g. a split game barricade set) a Settings actor with Can Be Climbed, Allow Barricade Moves, Can Be Knocked Down and Starts Down. Barricade sets split from now on get one automatically."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddBarricadeSettingsToSelected(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaRestoreMeshes",
			LOCTEXT("RestoreMeshes", "Restore Meshes From Barricades"),
			LOCTEXT("RestoreMeshesTip", "Undo Make Barricade From Selected: remove the barricade pieces made for the selected meshes (or for all of them in the level if nothing is selected) and leave the plain meshes. Barricades made by older versions get their meshes rebuilt."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::RestoreMeshesFromBarricades(); })));
		AEWMenus::AddGroupedToolEntry("Arena", ArenaLabel, "AEWArenaCameraPreview",
			LOCTEXT("CameraPreview", "Toggle Camera Preview"),
			LOCTEXT("CameraPreviewTip", "Show where the gameplay camera settles for this arena's camera table (Camera Preview Row in the settings), using the game's own camera maths, with a GameplayCameraPreview camera you can look through. Uses the selected actors as wrestlers, or the 1-on-1 starts if nothing is selected. Run it again to remove the preview."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::ToggleCameraPreview(); })));
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
