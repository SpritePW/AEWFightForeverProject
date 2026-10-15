#include "Modules/ModuleManager.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "SEditPartMaker.h"

#define LOCTEXT_NAMESPACE "AEWEditPartMaker"

static const FName EditPartMakerTabName(TEXT("AEWEditPartMaker"));

class FAEWEditPartMakerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EditPartMakerTabName,
			FOnSpawnTab::CreateRaw(this, &FAEWEditPartMakerModule::SpawnTab))
			.SetDisplayName(LOCTEXT("TabTitle", "AEW Edit Part Maker"))
			.SetTooltipText(LOCTEXT("TabTip", "Make new CAW parts"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAEWEditPartMakerModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(EditPartMakerTabName);
		}
	}

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
	{
		TSharedRef<SEditPartMaker> Panel = SNew(SEditPartMaker);
		Panel_ = Panel;
		if (PendingMesh.IsValid())
		{
			Panel->SetMesh(PendingMesh.Get());
			PendingMesh.Reset();
		}
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[Panel];
	}

	void OpenWith(USkeletalMesh* Mesh)
	{
		PendingMesh = Mesh;
		FGlobalTabmanager::Get()->TryInvokeTab(EditPartMakerTabName);
		if (TSharedPtr<SEditPartMaker> Panel = Panel_.Pin())
		{
			if (PendingMesh.IsValid())
			{
				Panel->SetMesh(PendingMesh.Get());
			}
		}
		PendingMesh.Reset();
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
		{
			FToolMenuSection& Section = WindowMenu->FindOrAddSection("AEWFightForever");
			Section.Label = LOCTEXT("SectionLabel", "AEW Fight Forever");
			Section.AddMenuEntry("OpenAEWEditPartMaker",
				LOCTEXT("OpenLabel", "AEW Edit Part Maker"),
				LOCTEXT("OpenTip", "Make new CAW parts"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(EditPartMakerTabName); })));
		}

		if (UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SkeletalMesh"))
		{
			FToolMenuSection& Section = AssetMenu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry("AEWMakePart", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
			{
				const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				if (!Context)
				{
					return;
				}
				TWeakObjectPtr<USkeletalMesh> Mesh;
				for (const TWeakObjectPtr<UObject>& Object : Context->SelectedObjects)
				{
					if (USkeletalMesh* Candidate = Cast<USkeletalMesh>(Object.Get()))
					{
						Mesh = Candidate;
						break;
					}
				}
				if (!Mesh.IsValid())
				{
					return;
				}
				InSection.AddMenuEntry("AEWMakePart",
					LOCTEXT("MakeLabel", "Make AEW edit part..."),
					LOCTEXT("MakeTip", "Open the AEW Edit Part Maker with this mesh"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([this, Mesh]() { OpenWith(Mesh.Get()); })));
			}));
		}
	}

	TWeakPtr<SEditPartMaker> Panel_;
	TWeakObjectPtr<USkeletalMesh> PendingMesh;
};

IMPLEMENT_MODULE(FAEWEditPartMakerModule, AEWEditPartMaker)

#undef LOCTEXT_NAMESPACE
