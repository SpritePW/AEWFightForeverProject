#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "AEWArenaTools.h"
#include "AEWArenaTemplateSettings.h"
#include "AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/World.h"
#include "AEWTableUtils.h"
#include "SArenaTemplatePanel.h"

#define LOCTEXT_NAMESPACE "AEWArenaTemplate"

static const FName ArenaTemplateTabName(TEXT("AEWArenaTemplate"));

class FAEWArenaTemplateModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ArenaTemplateTabName,
			FOnSpawnTab::CreateRaw(this, &FAEWArenaTemplateModule::SpawnTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Arena Maker"))
			.SetTooltipText(LOCTEXT("TabTip", "Create new arenas from imported templates"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAEWArenaTemplateModule::RegisterMenus));

		FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		RemovedHandle = AssetRegistry.Get().OnAssetRemoved().AddRaw(this, &FAEWArenaTemplateModule::OnAssetRemoved);
	}

	virtual void ShutdownModule() override
	{
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
		if (UAEWArenaTools::IsBusy() || Asset.AssetClass != UWorld::StaticClass()->GetFName() || !GetDefault<UAEWArenaTemplateSettings>()->bSyncOnDelete)
		{
			return;
		}
		PendingLevels.Add(Asset.AssetName);
		if (!TickHandle.IsValid())
		{
			TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAEWArenaTemplateModule::ProcessRemoved), 0.5f);
		}
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
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[ SNew(SArenaTemplatePanel) ];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		AEWMenus::AddToolEntry("OpenAEWArenaTemplate",
			LOCTEXT("OpenLabel", "Arena Maker"),
			LOCTEXT("OpenTip", "Copy an imported arena template (map + its own sublevels) into a new arena and add it to the game's arena list."),
			FUIAction(FExecuteAction::CreateStatic(&UAEWArenaTools::OpenDialog)));
		AEWMenus::AddGroupedToolEntry("Arena", LOCTEXT("ArenaGroup", "Arena"), "AEWArenaStartMarkers",
			LOCTEXT("StartMarkers", "Add Start Markers"),
			LOCTEXT("StartMarkersTip", "Create BP_StartMarker in the open arena's folder and place one on every player and referee start of ArenaMatch. Move the ones you want to change and delete the rest; in game they move ArenaMatch's starts for this arena only."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddStartMarkers(); })));
		AEWMenus::AddGroupedToolEntry("Arena", LOCTEXT("ArenaGroup", "Arena"), "AEWArenaCameraOverride",
			LOCTEXT("CameraOverride", "Add Camera Override"),
			LOCTEXT("CameraOverrideTip", "Create BP_CameraOverride and a zoomed-out copy of the gameplay camera table in the open arena's folder and place the override in the map. In game the camera uses that table in this arena only."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddCameraOverride(); })));
		AEWMenus::AddGroupedToolEntry("Arena", LOCTEXT("ArenaGroup", "Arena"), "AEWArenaRuleOverride",
			LOCTEXT("RuleNodes", "Add Rule Override"),
			LOCTEXT("RuleNodesTip", "Add nodes to the open arena's Level Blueprint that change the match rule for matches in this arena: on BeginPlay they read the game's match rule, pass it through 'Set members in FMatchRule' (tick the fields you want to change) and set it back; on EndPlay they restore the original."),
			FUIAction(FExecuteAction::CreateLambda([]() { UAEWArenaTools::AddMatchRuleOverrideNodes(); })));
	}

	FDelegateHandle RemovedHandle;
	FDelegateHandle TickHandle;
	TSet<FName> PendingLevels;
};

IMPLEMENT_MODULE(FAEWArenaTemplateModule, AEWArenaTemplate)

#undef LOCTEXT_NAMESPACE
