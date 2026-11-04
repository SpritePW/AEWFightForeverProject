#include "SArenaTemplatePanel.h"
#include "AEWArenaTemplateSettings.h"
#include "AEWToolUI.h"
#include "AEWPackaging.h"
#include "AEWModdingSettings.h"

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

#define LOCTEXT_NAMESPACE "AEWArenaTemplate"

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

void SArenaTemplatePanel::Construct(const FArguments& InArgs)
{
	const UAEWArenaTemplateSettings* S = GetDefault<UAEWArenaTemplateSettings>();
	ParentFolder = S->DefaultParentFolder.Path;
	ChunkId = AEWPackaging::Settings()->ChunkId;

	LogCapture = MakeShared<FArenaLogCapture>([this](const FString& Line) { LogText += Line + TEXT("\n"); });
	GLog->AddOutputDevice(LogCapture.Get());

	auto CanCreate = [this]() { return Selected.IsValid() && !IdProblem() && !Name.TrimStartAndEnd().IsEmpty(); };

	TSharedRef<SWidget> Toolbar = AEWToolUI::MakeToolbar(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("Refresh", "Refresh"), LOCTEXT("RefreshTip", "Reload the templates and the arena list."),
				[this]() { Refresh(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("Sync", "Sync arena list"),
				LOCTEXT("SyncTip", "Remove mod arenas whose map was deleted from the arena table, the enum and the unlock table, then renumber the mod arenas without gaps (renaming their <ID>_ maps and folders)."),
				[this]() { UAEWArenaTools::SyncArenaList(GetDefault<UAEWArenaTemplateSettings>()->bRenumberOnDelete, true); Refresh(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 8, 0)
		[
			AEWToolUI::MakePrimaryButton(LOCTEXT("Create", "Create arena"),
				LOCTEXT("CreateTip", "Copy the selected template into a new arena, re-point its sublevels, label it and (if ticked) add it to the arena list."),
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
			AEWToolUI::MakeButton(LOCTEXT("Settings", "Settings..."), LOCTEXT("SettingsTip", "Project Settings > Plugins > Arena Template"),
				[]() { UAEWArenaTools::OpenSettings(); })
		],
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SArenaTemplatePanel::GetStatusText)));

	TSharedRef<SWidget> List = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("Search", "Filter templates"))
			.OnTextChanged_Lambda([this](const FText& Text) { FilterText = Text.ToString(); ApplyFilter(); })
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FArenaTemplateItem>>)
			.ListItemsSource(&VisibleTemplates)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SArenaTemplatePanel::OnGenerateRow)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FArenaTemplateItem> Item, ESelectInfo::Type) { Selected = Item; })
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ArenaTemplateColumns::Name).DefaultLabel(LOCTEXT("ColName", "Template")).FillWidth(0.45f)
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
						LOCTEXT("ArenaIdTip", "EArenaID entry and DT_ArenaOriginal row of the new arena. The map is saved as <ID>_<name>."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Name", "Name"), AEWToolUI::MakeTextField(&Name, LOCTEXT("NameHint", "No spaces")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(FText::GetEmpty(),
						SNew(STextBlock)
						.Text(this, &SArenaTemplatePanel::GetPreviewText)
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
						LOCTEXT("RegisterTip", "Add an EArenaID entry, a DT_ArenaOriginal row and a default-unlocked unlock row."))
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
						LOCTEXT("LabelTip", "Create a PrimaryAssetLabel for the arena's maps in this chunk."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Maps", "Maps to cook"), Check(&bAddMaps),
						LOCTEXT("MapsTip", "Add the maps to Project Settings > Packaging > List of maps to include."))
				])
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("ArenaListSection", "Arena list"),
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.Text(this, &SArenaTemplatePanel::GetArenaListText),
				true)
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			AEWToolUI::MakeSection(LOCTEXT("SettingsArea", "Settings"), AEWToolUI::MakeSettingsView(GetMutableDefault<UAEWArenaTemplateSettings>()), true)
		]);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Toolbar ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ AEWToolUI::MakeBody(List, Side, &LogText, 0.45f) ]
	];

	Refresh();
	AddLog(TEXT("Pick a template, set the ID and name, then Create arena."));
}

SArenaTemplatePanel::~SArenaTemplatePanel()
{
	if (LogCapture.IsValid() && GLog)
	{
		GLog->RemoveOutputDevice(LogCapture.Get());
	}
}

void SArenaTemplatePanel::Refresh()
{
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
	ApplyFilter();
	AddLog(FString::Printf(TEXT("%d templates, %d arenas, next free arena ID %d."), AllTemplates.Num(), Arenas.Num(), NextId));
}

void SArenaTemplatePanel::ApplyFilter()
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

void SArenaTemplatePanel::Create()
{
	if (!Selected.IsValid())
	{
		return;
	}
	AddLog(TEXT("---- create ") + MapName() + TEXT(" from ") + Selected->Name);
	const FString Result = UAEWArenaTools::CreateArenaFromTemplate(Selected->Package, ParentFolder.TrimStartAndEnd(), Name,
		bCreateLabel, ChunkId, bAddMaps, true, bRegister, DisplayName.TrimStartAndEnd(), bRegister ? ArenaId : -1);
	AddLog(Result.IsEmpty() ? TEXT("Failed - see errors above.") : TEXT("Done: ") + Result);
	Refresh();
}

void SArenaTemplatePanel::AddLog(const FString& Line)
{
	LogText += Line + TEXT("\n");
}

bool SArenaTemplatePanel::IsRegistering() const
{
	return bRegister;
}

bool SArenaTemplatePanel::IdProblem() const
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

FString SArenaTemplatePanel::MapName() const
{
	return bRegister ? UAEWArenaTools::MakeArenaMapName(ArenaId, Name) : Name.TrimStartAndEnd();
}

FText SArenaTemplatePanel::GetPreviewText() const
{
	if (IdProblem())
	{
		return FText::Format(LOCTEXT("IdUsed", "Arena ID {0} is already used"), FText::AsNumber(ArenaId));
	}
	return FText::Format(LOCTEXT("MapName", "Map name: {0}"), FText::FromString(MapName()));
}

FText SArenaTemplatePanel::GetStatusText() const
{
	const UAEWArenaTemplateSettings* S = GetDefault<UAEWArenaTemplateSettings>();
	return FText::FromString(FString::Printf(TEXT("Templates: %d    Arena table: %s (%d rows)    Next free arena ID: %d"),
		AllTemplates.Num(), *S->ArenaTable.GetAssetName(), Arenas.Num(), NextId));
}

FText SArenaTemplatePanel::GetArenaListText() const
{
	FString Out;
	for (const FAEWArenaListEntry& A : Arenas)
	{
		Out += FString::Printf(TEXT("%4s  %-28s %s\n"), *A.Id, *A.LevelName, A.bSelectable ? TEXT("") : TEXT("(hidden)"));
	}
	return FText::FromString(Out.TrimEnd());
}

TSharedRef<ITableRow> SArenaTemplatePanel::OnGenerateRow(TSharedPtr<FArenaTemplateItem> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SArenaTemplateRow, Owner, Item);
}

#undef LOCTEXT_NAMESPACE
