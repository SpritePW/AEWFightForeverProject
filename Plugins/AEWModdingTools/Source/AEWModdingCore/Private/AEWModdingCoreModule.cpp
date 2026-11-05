#include "Modules/ModuleManager.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "AEWPackaging.h"
#include "AEWTableUtils.h"
#include "AEWModdingSettings.h"
#include "GameDelegates.h"

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
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (bBoundCook)
		{
			FGameDelegates::Get().GetCookModificationDelegate().Unbind();
		}
	}

private:
	bool bBoundCook = false;

	static void AddLabelledToCook(TArray<FString>& ExtraPackagesToCook)
	{
		if (!GetDefault<UAEWModdingSettings>()->bCookLabelledAssets)
		{
			return;
		}
		const int32 Before = ExtraPackagesToCook.Num();
		const int32 Labels = AEWPackaging::CollectLabelledPackages(ExtraPackagesToCook);
		UE_LOG(LogAEWModding, Display, TEXT("Cook: added %d package(s) from %d Always Cook label(s)."), ExtraPackagesToCook.Num() - Before, Labels);
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		AEWMenus::AddGroupedToolEntry("Cook", LOCTEXT("CookGroup", "Cook"), "RebuildAEWTableLabels",
			LOCTEXT("RebuildLabels", "Rebuild Table Labels"),
			LOCTEXT("RebuildLabelsTip", "Put every _New DataTable in the New Tables label and the composites/other tables in the Base Tables label (Project Settings > Plugins > AEW Modding Tools)."),
			FUIAction(FExecuteAction::CreateStatic(&FAEWModdingCoreModule::Rebuild)));
		AEWMenus::AddGroupedToolEntry("Cook", LOCTEXT("CookGroup", "Cook"), "AEWCookOnlyLabelled",
			LOCTEXT("CookOnlyLabelled", "Cook Only Labelled Content"),
			LOCTEXT("CookOnlyLabelledTip", "Turn off 'Cook everything' and set 'Directories to never cook' to every content folder that holds nothing a label ships, so packaging only cooks your labelled chunks. Run again after adding labels in new folders."),
			FUIAction(FExecuteAction::CreateStatic(&FAEWModdingCoreModule::CookOnly)));
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

	static void CookOnly()
	{
		TArray<FString> Log;
		AEWPackaging::CookOnlyLabelledContent(Log);
		Show(LOCTEXT("CookOnlyLabelledContentTitle", "Cook Only Labelled Content"), Log);
	}

	static void Rebuild()
	{
		TArray<FString> Log;
		AEWPackaging::RebuildTableLabels(Log);
		Show(LOCTEXT("RebuildTableLabelsTitle", "Rebuild Table Labels"), Log);
	}
};

IMPLEMENT_MODULE(FAEWModdingCoreModule, AEWModdingCore)

#undef LOCTEXT_NAMESPACE
