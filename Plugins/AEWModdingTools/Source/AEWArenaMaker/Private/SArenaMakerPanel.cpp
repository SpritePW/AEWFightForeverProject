#include "SArenaMakerPanel.h"
#include "AEWArenaMakerSettings.h"
#include "AEWToolUI.h"
#include "AEWPackaging.h"
#include "AEWModdingSettings.h"
#include "AssetRegistryModule.h"
#include "HAL/PlatformTime.h"

#include "Misc/OutputDevice.h"
#include "Styling/CoreStyle.h"
#include "Misc/PackageName.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "AEWArenaMaker"

class FArenaLogCapture : public FOutputDevice
{
public:
	explicit FArenaLogCapture(TFunction<void(const FString&)> InSink) : Sink(MoveTemp(InSink)) {}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		static const FName ArenaCategory(TEXT("LogAEWArena"));
		if (Category == ArenaCategory && IsInGameThread() && Sink)
		{
			FString Line(V);
			if (Verbosity == ELogVerbosity::Error)
			{
				Line = TEXT("ERROR: ") + Line;
			}
			else if (Verbosity == ELogVerbosity::Warning)
			{
				Line = TEXT("warning: ") + Line;
			}
			Sink(Line);
		}
	}

private:
	TFunction<void(const FString&)> Sink;
};

namespace ArenaTemplateColumns
{
	static const FName Name(TEXT("Name"));
	static const FName Folder(TEXT("Folder"));
}

class SArenaTemplateRow : public SMultiColumnTableRow<TSharedPtr<FArenaTemplateItem>>
{
public:
	SLATE_BEGIN_ARGS(SArenaTemplateRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& Owner, TSharedPtr<FArenaTemplateItem> InItem)
	{
		Item = InItem;
		SMultiColumnTableRow<TSharedPtr<FArenaTemplateItem>>::Construct(FSuperRowType::FArguments(), Owner);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
	{
		return SNew(SBox).Padding(FMargin(4.f, 1.f))
			[
				SNew(STextBlock).Text(FText::FromString(Column == ArenaTemplateColumns::Name ? Item->Name : Item->Folder))
			];
	}

private:
	TSharedPtr<FArenaTemplateItem> Item;
};

void SArenaMakerPanel::Construct(const FArguments& InArgs)
{
	const UAEWArenaMakerSettings* S = GetDefault<UAEWArenaMakerSettings>();
	ParentFolder = S->DefaultParentFolder.Path;
	ChunkId = AEWPackaging::NextFreeChunkId();

	LogCapture = MakeShared<FArenaLogCapture>([this](const FString& Line) { LogText += Line + TEXT("\n"); });
	GLog->AddOutputDevice(LogCapture.Get());

	auto CanCreate = [this]() { return Selected.IsValid() && !IdProblem() && !Name.TrimStartAndEnd().IsEmpty(); };

	TSharedRef<SWidget> Toolbar = AEWToolUI::MakeToolbar(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("Sync", "Sync arena list"),
				LOCTEXT("SyncTip", "Remove deleted mod arenas and renumber the rest."),
				[this]() { UAEWArenaTools::SyncArenaList(GetDefault<UAEWArenaMakerSettings>()->bRenumberOnDelete, true); Refresh(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 8, 0)
		[
			AEWToolUI::MakePrimaryButton(LOCTEXT("Create", "Create arena"),
				LOCTEXT("CreateTip", "Copy the selected arena into a new arena."),
				[this]() { Create(); }, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(CanCreate)))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return FText::FromString(TEXT("as: ") + MapName()); })
			.ColorAndOpacity_Lambda([this]() { return IdProblem() ? FSlateColor(FLinearColor::Red) : FSlateColor::UseForeground(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 0, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("Settings", "Settings..."), LOCTEXT("SettingsTip", "Project Settings > Plugins > Arena Maker"),
				[]() { UAEWArenaTools::OpenSettings(); })
		],
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SArenaMakerPanel::GetStatusText)));

	TSharedRef<SWidget> List = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("Search", "Filter templates and arenas"))
			.OnTextChanged_Lambda([this](const FText& Text) { FilterText = Text.ToString(); ApplyFilter(); })
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FArenaTemplateItem>>)
			.ListItemsSource(&VisibleTemplates)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SArenaMakerPanel::OnGenerateRow)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FArenaTemplateItem> Item, ESelectInfo::Type) { Selected = Item; })
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ArenaTemplateColumns::Name).DefaultLabel(LOCTEXT("ColName", "Template or arena")).FillWidth(0.45f)
				+ SHeaderRow::Column(ArenaTemplateColumns::Folder).DefaultLabel(LOCTEXT("ColFolder", "Folder")).FillWidth(0.55f)
			)
		];

	auto Check = [](bool* Flag)
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([Flag]() { return *Flag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Flag](ECheckBoxState State) { *Flag = State == ECheckBoxState::Checked; });
	};

	TSharedRef<SWidget> Side = AEWToolUI::MakeSidePanel(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("NewSection", "New arena"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Template", "Template"),
						SNew(STextBlock).Text_Lambda([this]()
						{
							return Selected.IsValid() ? FText::FromString(Selected->Name) : LOCTEXT("PickTemplate", "(pick one on the left)");
						}))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("ArenaId", "Arena ID"),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 4, 0)
						[
							SNew(SNumericEntryBox<int32>)
							.AllowSpin(false)
							.MinValue(0).MaxValue(254)
							.IsEnabled_Lambda([this]() { return bRegister; })
							.Value_Lambda([this]() { return TOptional<int32>(ArenaId); })
							.OnValueChanged_Lambda([this](int32 V) { ArenaId = FMath::Clamp(V, 0, 254); })
							.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { ArenaId = FMath::Clamp(V, 0, 254); })
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							AEWToolUI::MakeButton(LOCTEXT("NextFreeId", "Next free"), LOCTEXT("NextFreeIdTip", "Lowest arena ID after the highest one in use"),
								[this]() { NextId = UAEWArenaTools::GetNextArenaId(); if (NextId >= 0) { ArenaId = NextId; } },
								TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]() { return bRegister; })))
						],
						LOCTEXT("ArenaIdTip", "EArenaID entry and arena row. Map is saved as <ID>_<name>."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Name", "Name"), AEWToolUI::MakeTextField(&Name, LOCTEXT("NameHint", "No spaces")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(FText::GetEmpty(),
						SNew(STextBlock)
						.Text(this, &SArenaMakerPanel::GetPreviewText)
						.ColorAndOpacity_Lambda([this]() { return IdProblem() ? FSlateColor(FLinearColor::Red) : FSlateColor::UseSubduedForeground(); }))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Display", "Display name"), AEWToolUI::MakeTextField(&DisplayName, LOCTEXT("DisplayHint", "Optional, defaults to the name")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Folder", "Parent folder"), AEWToolUI::MakeTextField(&ParentFolder, LOCTEXT("FolderHint", "/Game/...")),
						LOCTEXT("FolderTip", "A sub-folder named after the map is created here."))
				])
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("OptionsSection", "Options"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Register", "Register"), Check(&bRegister),
						LOCTEXT("RegisterTip", "Add the enum entry, arena row and unlock row."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Label", "Asset label"),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ Check(&bCreateLabel) ]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)[ SNew(STextBlock).Text(LOCTEXT("Chunk", "chunk")) ]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SNumericEntryBox<int32>)
							.AllowSpin(false)
							.MinValue(1).MaxValue(9999)
							.IsEnabled_Lambda([this]() { return bCreateLabel; })
							.Value_Lambda([this]() { return TOptional<int32>(ChunkId); })
							.OnValueChanged_Lambda([this](int32 V) { ChunkId = FMath::Clamp(V, 1, 9999); })
							.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { ChunkId = FMath::Clamp(V, 1, 9999); })
						],
						LOCTEXT("LabelTip", "Label the arena's maps in this chunk."))
				]
				)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("ArenaListSection", "Arena list"),
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.Text(this, &SArenaMakerPanel::GetArenaListText),
				true)
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			AEWToolUI::MakeSection(LOCTEXT("SettingsArea", "Settings"), AEWToolUI::MakeSettingsView(GetMutableDefault<UAEWArenaMakerSettings>()), true)
		]);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Toolbar ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ AEWToolUI::MakeBody(List, Side, &LogText, 0.45f) ]
	];

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AddedHandle = AR.OnAssetAdded().AddSP(this, &SArenaMakerPanel::OnAssetChanged);
	RemovedHandle = AR.OnAssetRemoved().AddSP(this, &SArenaMakerPanel::OnAssetChanged);
	RenamedHandle = AR.OnAssetRenamed().AddSP(this, &SArenaMakerPanel::OnAssetRenamed);

	Refresh();
	AddLog(TEXT("Pick a template, set the ID and name, then Create arena."));
}

void SArenaMakerPanel::OnAssetChanged(const FAssetData&)
{
	QueueRefresh();
}

void SArenaMakerPanel::OnAssetRenamed(const FAssetData&, const FString&)
{
	QueueRefresh();
}

void SArenaMakerPanel::QueueRefresh()
{
	bRefreshPending = true;
	RefreshAt = FPlatformTime::Seconds() + 1.0;
}

void SArenaMakerPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (!bRefreshPending || FPlatformTime::Seconds() < RefreshAt || UAEWArenaTools::IsBusy())
	{
		return;
	}
	if (FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().IsLoadingAssets())
	{
		return;
	}
	bRefreshPending = false;
	Refresh(true);
}

SArenaMakerPanel::~SArenaMakerPanel()
{
	if (LogCapture.IsValid() && GLog)
	{
		GLog->RemoveOutputDevice(LogCapture.Get());
	}
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		IAssetRegistry& AR = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AR.OnAssetAdded().Remove(AddedHandle);
		AR.OnAssetRemoved().Remove(RemovedHandle);
		AR.OnAssetRenamed().Remove(RenamedHandle);
	}
}

void SArenaMakerPanel::Refresh(bool bQuiet)
{
	const int32 OldTemplates = AllTemplates.Num();
	const int32 OldArenas = Arenas.Num();
	const int32 OldNext = NextId;
	const FString SelectedPackage = Selected.IsValid() ? Selected->Package : FString();
	AllTemplates.Reset();
	for (const FString& Pkg : UAEWArenaTools::FindTemplates())
	{
		TSharedPtr<FArenaTemplateItem> Item = MakeShared<FArenaTemplateItem>();
		Item->Package = Pkg;
		Item->Name = FPackageName::GetShortName(Pkg);
		Item->Folder = FPackageName::GetLongPackagePath(Pkg);
		AllTemplates.Add(Item);
	}
	UAEWArenaTools::GetArenaList(Arenas);
	NextId = UAEWArenaTools::GetNextArenaId();
	if (NextId >= 0 && (!UAEWArenaTools::IsArenaIdFree(ArenaId) || ArenaId < NextId))
	{
		ArenaId = NextId;
	}
	CheckedId = -1;
	Selected.Reset();
	for (const TSharedPtr<FArenaTemplateItem>& Item : AllTemplates)
	{
		if (Item->Package == SelectedPackage)
		{
			Selected = Item;
		}
	}
	ApplyFilter();
	if (ListView.IsValid())
	{
		ListView->ClearSelection();
		if (Selected.IsValid())
		{
			ListView->SetSelection(Selected);
		}
	}
	if (!bQuiet || OldTemplates != AllTemplates.Num() || OldArenas != Arenas.Num() || OldNext != NextId)
	{
		AddLog(FString::Printf(TEXT("%d templates, %d arenas, next free arena ID %d."), AllTemplates.Num(), Arenas.Num(), NextId));
	}
}

void SArenaMakerPanel::ApplyFilter()
{
	VisibleTemplates.Reset();
	for (const TSharedPtr<FArenaTemplateItem>& Item : AllTemplates)
	{
		if (FilterText.IsEmpty() || Item->Name.Contains(FilterText) || Item->Folder.Contains(FilterText))
		{
			VisibleTemplates.Add(Item);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

void SArenaMakerPanel::Create()
{
	if (!Selected.IsValid())
	{
		return;
	}
	AddLog(TEXT("---- create ") + MapName() + TEXT(" from ") + Selected->Name);
	const FString Result = UAEWArenaTools::CreateArenaFromTemplate(Selected->Package, ParentFolder.TrimStartAndEnd(), Name,
		bCreateLabel, ChunkId, false, true, bRegister, DisplayName.TrimStartAndEnd(), bRegister ? ArenaId : -1);
	AddLog(Result.IsEmpty() ? TEXT("Failed - see errors above.") : TEXT("Done: ") + Result);
	if (!Result.IsEmpty())
	{
		ChunkId = AEWPackaging::NextFreeChunkId();
	}
	Refresh();
}

void SArenaMakerPanel::AddLog(const FString& Line)
{
	LogText += Line + TEXT("\n");
}

bool SArenaMakerPanel::IsRegistering() const
{
	return bRegister;
}

bool SArenaMakerPanel::IdProblem() const
{
	if (!bRegister)
	{
		return false;
	}
	if (ArenaId != CheckedId)
	{
		CheckedId = ArenaId;
		bCheckedFree = UAEWArenaTools::IsArenaIdFree(ArenaId);
	}
	return !bCheckedFree;
}

FString SArenaMakerPanel::MapName() const
{
	return bRegister ? UAEWArenaTools::MakeArenaMapName(ArenaId, Name) : Name.TrimStartAndEnd();
}

FText SArenaMakerPanel::GetPreviewText() const
{
	if (IdProblem())
	{
		return FText::Format(LOCTEXT("IdUsed", "Arena ID {0} is already used"), FText::AsNumber(ArenaId));
	}
	return FText::Format(LOCTEXT("MapName", "Map name: {0}"), FText::FromString(MapName()));
}

FText SArenaMakerPanel::GetStatusText() const
{
	const UAEWArenaMakerSettings* S = GetDefault<UAEWArenaMakerSettings>();
	return FText::FromString(FString::Printf(TEXT("Templates: %d    Arena table: %s (%d rows)    Next ID: %d"),
		AllTemplates.Num(), *S->ArenaTable.GetAssetName(), Arenas.Num(), NextId));
}

FText SArenaMakerPanel::GetArenaListText() const
{
	FString Out;
	for (const FAEWArenaListEntry& A : Arenas)
	{
		Out += FString::Printf(TEXT("%4s  %-28s %s\n"), *A.Id, *A.LevelName, A.bSelectable ? TEXT("") : TEXT("(hidden)"));
	}
	return FText::FromString(Out.TrimEnd());
}

TSharedRef<ITableRow> SArenaMakerPanel::OnGenerateRow(TSharedPtr<FArenaTemplateItem> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SArenaTemplateRow, Owner, Item);
}

#undef LOCTEXT_NAMESPACE
