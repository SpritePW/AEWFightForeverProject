#include "SChunkNamesPanel.h"
#include "AEWModdingSettings.h"
#include "AEWPackaging.h"
#include "AEWToolUI.h"
#include "AssetRegistryModule.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "AEWChunkNames"

const FName SChunkNamesPanel::TabName(TEXT("AEWChunkNames"));

namespace ChunkNameColumns
{
	static const FName Chunk(TEXT("Chunk"));
	static const FName Name(TEXT("Name"));
	static const FName Zip(TEXT("Zip"));
	static const FName Assets(TEXT("Assets"));
}

static FString EffectiveZip(const FChunkNameItem& I)
{
	if (!I.Zip.TrimStartAndEnd().IsEmpty())
	{
		return AEWPackaging::CleanModName(I.Zip);
	}
	if (!I.Name.TrimStartAndEnd().IsEmpty())
	{
		return AEWPackaging::CleanModName(I.Name);
	}
	return FString::Printf(TEXT("pakchunk%d"), I.Chunk);
}

static FString EffectiveFile(const FChunkNameItem& I)
{
	const FString Name = AEWPackaging::CleanModName(I.Name);
	if (Name.IsEmpty())
	{
		return FString::Printf(TEXT("pakchunk%d-WindowsNoEditor"), I.Chunk);
	}
	return Name.EndsWith(TEXT("_P")) ? Name : Name + TEXT("_P");
}

class SChunkNameRow : public SMultiColumnTableRow<TSharedPtr<FChunkNameItem>>
{
public:
	SLATE_BEGIN_ARGS(SChunkNameRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& Owner, TSharedPtr<FChunkNameItem> InItem)
	{
		Item = InItem;
		SMultiColumnTableRow<TSharedPtr<FChunkNameItem>>::Construct(FSuperRowType::FArguments(), Owner);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
	{
		TSharedPtr<FChunkNameItem> I = Item;
		if (Column == ChunkNameColumns::Chunk)
		{
			return SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("pakchunk%d"), I->Chunk)));
		}
		if (Column == ChunkNameColumns::Name)
		{
			return SNew(STextBlock)
				.Text_Lambda([I]() { return FText::FromString(EffectiveFile(*I)); })
				.ColorAndOpacity_Lambda([I]() { return I->Name.IsEmpty() ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground(); });
		}
		if (Column == ChunkNameColumns::Zip)
		{
			return SNew(STextBlock)
				.Text_Lambda([I]() { return FText::FromString(EffectiveZip(*I) + TEXT(".zip")); });
		}
		return SNew(STextBlock)
			.Text(FText::AsNumber(I->Assets.Num()))
			.ToolTipText(FText::FromString(FString::Join(I->Labels, TEXT("\n"))));
	}

private:
	TSharedPtr<FChunkNameItem> Item;
};

void SChunkNamesPanel::Construct(const FArguments& InArgs)
{
	OutputFolder = AEWPackaging::Settings()->PackagedModsFolder.Path;

	auto HasSelection = [this]() { return Selected.IsValid(); };

	TSharedRef<SWidget> Toolbar = AEWToolUI::MakeToolbar(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
		[
			AEWToolUI::MakePrimaryButton(LOCTEXT("Package", "Package Project"),
				LOCTEXT("PackageTip", "Package for Windows (64-bit) using the project's packaging settings."),
				[this]() { Save(); AEWPackaging::PackageProject(); })
		]

		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNullWidget::NullWidget
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 0, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("Settings", "Settings..."), LOCTEXT("SettingsTip", "Open the plugin settings."),
				[]() { AEWToolUI::OpenSettings(TEXT("AEWModdingSettings")); })
		],
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SChunkNamesPanel::GetStatusText)));

	TSharedRef<SWidget> List = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("Search", "Filter chunks, names and assets"))
			.OnTextChanged_Lambda([this](const FText& Text) { FilterText = Text.ToString(); ApplyFilter(); })
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FChunkNameItem>>)
			.ListItemsSource(&VisibleItems)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SChunkNamesPanel::OnGenerateRow)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FChunkNameItem> Item, ESelectInfo::Type) { Select(Item); })
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ChunkNameColumns::Chunk).DefaultLabel(LOCTEXT("ColChunk", "Chunk")).FillWidth(0.2f)
				+ SHeaderRow::Column(ChunkNameColumns::Name).DefaultLabel(LOCTEXT("ColName", "File name")).FillWidth(0.35f)
				+ SHeaderRow::Column(ChunkNameColumns::Zip).DefaultLabel(LOCTEXT("ColZip", "Zip")).FillWidth(0.33f)
				+ SHeaderRow::Column(ChunkNameColumns::Assets).DefaultLabel(LOCTEXT("ColAssets", "Assets")).FillWidth(0.12f)
			)
		];

	auto Field = [this](FString FChunkNameItem::* Member)
	{
		return SNew(SEditableTextBox)
			.IsEnabled_Lambda([this]() { return Selected.IsValid(); })
			.Text_Lambda([this, Member]() { return Selected.IsValid() ? FText::FromString((*Selected).*Member) : FText::GetEmpty(); })
			.OnTextChanged_Lambda([this, Member](const FText& T) { if (Selected.IsValid()) { (*Selected).*Member = T.ToString(); } })
			.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type) { Save(); });
	};

	TSharedRef<SWidget> Side = AEWToolUI::MakeSidePanel(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("ChunkSection", "Chunk"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Chunk", "Chunk"),
						SNew(STextBlock).Text_Lambda([this]()
						{
							return Selected.IsValid() ? FText::FromString(FString::Printf(TEXT("pakchunk%d"), Selected->Chunk)) : LOCTEXT("PickChunk", "(pick one on the left)");
						}))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("FileName", "File name"), Field(&FChunkNameItem::Name),
						LOCTEXT("FileNameTip", "Name the mod is shipped under. _P is added."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("ZipName", "Zip"), Field(&FChunkNameItem::Zip),
						LOCTEXT("ZipTip", "Chunks with the same zip name share one archive."))
				]
)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("AssetsSection", "Assets in this chunk"),
				SNew(SBox).MinDesiredHeight(120.f).MaxDesiredHeight(320.f)
				[
					SAssignNew(AssetView, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&SelectedAssets)
					.SelectionMode(ESelectionMode::None)
					.OnGenerateRow(this, &SChunkNamesPanel::OnGenerateAssetRow)
				])
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("OutputSection", "Output"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Folder", "Output folder"),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 4, 0)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]() { return FText::FromString(OutputFolder); })
							.OnTextChanged_Lambda([this](const FText& T) { OutputFolder = T.ToString(); })
							.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type) { Save(); })
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							AEWToolUI::MakeButton(LOCTEXT("Browse", "..."), LOCTEXT("BrowseTip", "Choose a folder."), [this]() { BrowseFolder(); })
						])
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					AEWToolUI::MakeRow(LOCTEXT("Auto", "After packaging"),
						SNew(SCheckBox)
						.IsChecked_Lambda([]() { return AEWPackaging::Settings()->bProcessPackagedChunks ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
						{
							UAEWModdingSettings* S = GetMutableDefault<UAEWModdingSettings>();
							S->bProcessPackagedChunks = State == ECheckBoxState::Checked;
							Save();
						})
						[
							SNew(STextBlock).Text(LOCTEXT("AutoText", "Rename and archive automatically"))
						],
						LOCTEXT("AutoTip", "Rename and archive after every package."))
				])
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			AEWToolUI::MakeSection(LOCTEXT("SettingsArea", "Settings"), AEWToolUI::MakeSettingsView(GetMutableDefault<UAEWModdingSettings>()), true)
		]);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Toolbar ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ AEWToolUI::MakeBody(List, Side, &LogText, 0.5f) ]
	];

	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AddedHandle = AR.OnAssetAdded().AddSP(this, &SChunkNamesPanel::OnAssetChanged);
	RemovedHandle = AR.OnAssetRemoved().AddSP(this, &SChunkNamesPanel::OnAssetChanged);
	RenamedHandle = AR.OnAssetRenamed().AddSP(this, &SChunkNamesPanel::OnAssetRenamed);

	Refresh();
}

SChunkNamesPanel::~SChunkNamesPanel()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		IAssetRegistry& AR = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AR.OnAssetAdded().Remove(AddedHandle);
		AR.OnAssetRemoved().Remove(RemovedHandle);
		AR.OnAssetRenamed().Remove(RenamedHandle);
	}
}

void SChunkNamesPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (bRefreshPending && FPlatformTime::Seconds() >= RefreshAt)
	{
		bRefreshPending = false;
		Refresh();
	}
}

void SChunkNamesPanel::QueueRefresh()
{
	bRefreshPending = true;
	RefreshAt = FPlatformTime::Seconds() + 1.0;
}

void SChunkNamesPanel::OnAssetChanged(const FAssetData& Asset)
{
	QueueRefresh();
}

void SChunkNamesPanel::OnAssetRenamed(const FAssetData& Asset, const FString& OldPath)
{
	QueueRefresh();
}

void SChunkNamesPanel::Refresh()
{
	const int32 SelectedChunk = Selected.IsValid() ? Selected->Chunk : INDEX_NONE;
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> Labels;
	AR.GetAssetsByClass(UPrimaryAssetLabel::StaticClass()->GetFName(), Labels, true);

	TMap<int32, TSharedPtr<FChunkNameItem>> ByChunk;
	TMap<int32, TSet<FString>> Assets;
	for (const FAssetData& A : Labels)
	{
		if (!A.PackageName.ToString().StartsWith(TEXT("/Game/")))
		{
			continue;
		}
		UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset());
		if (!Label || Label->Rules.ChunkId <= 0)
		{
			continue;
		}
		const int32 Chunk = Label->Rules.ChunkId;
		TSharedPtr<FChunkNameItem>& Item = ByChunk.FindOrAdd(Chunk);
		if (!Item.IsValid())
		{
			Item = MakeShared<FChunkNameItem>();
			Item->Chunk = Chunk;
		}
		Item->Labels.Add(A.AssetName.ToString());
		TSet<FString>& Set = Assets.FindOrAdd(Chunk);
		if (Label->bLabelAssetsInMyDirectory)
		{
			TArray<FAssetData> InDir;
			AR.GetAssetsByPath(FName(*FPackageName::GetLongPackagePath(A.PackageName.ToString())), InDir, true);
			for (const FAssetData& D : InDir)
			{
				if (D.AssetClass != UPrimaryAssetLabel::StaticClass()->GetFName())
				{
					Set.Add(D.PackageName.ToString());
				}
			}
		}
		for (const TSoftObjectPtr<UObject>& Soft : Label->ExplicitAssets)
		{
			if (!Soft.IsNull())
			{
				Set.Add(Soft.ToSoftObjectPath().GetLongPackageName());
			}
		}
	}

	const UAEWModdingSettings* S = AEWPackaging::Settings();
	for (const FAEWChunkOutput& Out : S->ChunkOutputs)
	{
		if (TSharedPtr<FChunkNameItem>* Item = ByChunk.Find(Out.ChunkId))
		{
			(*Item)->Name = Out.Name;
			(*Item)->Zip = Out.Zip;
		}
	}

	AllItems.Reset();
	for (TPair<int32, TSharedPtr<FChunkNameItem>>& Pair : ByChunk)
	{
		Pair.Value->Assets = Assets.FindRef(Pair.Key).Array();
		Pair.Value->Assets.Sort();
		Pair.Value->Labels.Sort();
		AllItems.Add(Pair.Value);
	}
	AllItems.Sort([](const TSharedPtr<FChunkNameItem>& A, const TSharedPtr<FChunkNameItem>& B) { return A->Chunk < B->Chunk; });
	ApplyFilter();

	TSharedPtr<FChunkNameItem> Keep;
	for (const TSharedPtr<FChunkNameItem>& Item : VisibleItems)
	{
		if (Item->Chunk == SelectedChunk)
		{
			Keep = Item;
		}
	}
	if (Keep.IsValid() && ListView.IsValid())
	{
		ListView->SetSelection(Keep);
	}
	else
	{
		Select(nullptr);
	}
}

void SChunkNamesPanel::ApplyFilter()
{
	VisibleItems.Reset();
	for (const TSharedPtr<FChunkNameItem>& Item : AllItems)
	{
		bool bShow = FilterText.IsEmpty()
			|| FString::Printf(TEXT("pakchunk%d"), Item->Chunk).Contains(FilterText)
			|| Item->Name.Contains(FilterText)
			|| Item->Zip.Contains(FilterText);
		for (int32 i = 0; !bShow && i < Item->Assets.Num(); ++i)
		{
			bShow = Item->Assets[i].Contains(FilterText);
		}
		if (bShow)
		{
			VisibleItems.Add(Item);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

void SChunkNamesPanel::Select(TSharedPtr<FChunkNameItem> Item)
{
	Selected = Item;
	SelectedAssets.Reset();
	if (Item.IsValid())
	{
		for (const FString& Asset : Item->Assets)
		{
			SelectedAssets.Add(MakeShared<FString>(Asset));
		}
	}
	if (AssetView.IsValid())
	{
		AssetView->RequestListRefresh();
	}
}

void SChunkNamesPanel::Save()
{
	UAEWModdingSettings* S = GetMutableDefault<UAEWModdingSettings>();
	TSet<int32> Shown;
	TArray<FAEWChunkOutput> Outputs;
	for (const TSharedPtr<FChunkNameItem>& Item : AllItems)
	{
		Shown.Add(Item->Chunk);
		const FString Name = AEWPackaging::CleanModName(Item->Name);
		const FString Zip = AEWPackaging::CleanModName(Item->Zip);
		if (Name.IsEmpty() && Zip.IsEmpty())
		{
			continue;
		}
		FAEWChunkOutput Out;
		Out.ChunkId = Item->Chunk;
		Out.Name = Name;
		Out.Zip = AEWPackaging::CleanModName(Item->Zip);
		Outputs.Add(Out);
	}
	for (const FAEWChunkOutput& Old : S->ChunkOutputs)
	{
		if (!Shown.Contains(Old.ChunkId))
		{
			Outputs.Add(Old);
		}
	}
	Outputs.Sort([](const FAEWChunkOutput& A, const FAEWChunkOutput& B) { return A.ChunkId < B.ChunkId; });

	const FString Folder = OutputFolder.TrimStartAndEnd();
	S->ChunkOutputs = Outputs;
	S->PackagedModsFolder.Path = Folder;
	S->SaveConfig();
	S->UpdateDefaultConfigFile();
}

void SChunkNamesPanel::BrowseFolder()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		return;
	}
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	FString Start = OutputFolder.TrimStartAndEnd();
	if (Start.IsEmpty() || !FPaths::DirectoryExists(Start))
	{
		Start = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}
	FString Picked;
	if (Desktop->OpenDirectoryDialog(Parent, LOCTEXT("BrowseTitle", "Output folder for packaged mods").ToString(), Start, Picked))
	{
		OutputFolder = FPaths::ConvertRelativePathToFull(Picked);
		Save();
	}
}

void SChunkNamesPanel::AddLog(const FString& Line)
{
	LogText += Line + TEXT("\n");
}

FText SChunkNamesPanel::GetStatusText() const
{
	int32 Named = 0;
	for (const TSharedPtr<FChunkNameItem>& Item : AllItems)
	{
		Named += Item->Name.TrimStartAndEnd().IsEmpty() ? 0 : 1;
	}
	const FString Folder = OutputFolder.TrimStartAndEnd().IsEmpty() ? FString(TEXT("Mods inside the packaging folder")) : OutputFolder.TrimStartAndEnd();
	return FText::FromString(FString::Printf(TEXT("%d chunks, %d named    Output: %s    After packaging: %s"),
		AllItems.Num(), Named, *Folder, AEWPackaging::Settings()->bProcessPackagedChunks ? TEXT("rename and archive") : TEXT("off")));
}

TSharedRef<ITableRow> SChunkNamesPanel::OnGenerateRow(TSharedPtr<FChunkNameItem> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SChunkNameRow, Owner, Item);
}

TSharedRef<ITableRow> SChunkNamesPanel::OnGenerateAssetRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FString>>, Owner)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FPackageName::GetShortName(*Item)))
			.ToolTipText(FText::FromString(*Item))
		];
}

namespace AEWPackaging
{
	void OpenChunkNames()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(SChunkNamesPanel::TabName);
	}
}

#undef LOCTEXT_NAMESPACE
