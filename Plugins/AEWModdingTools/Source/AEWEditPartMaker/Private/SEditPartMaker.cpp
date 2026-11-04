#include "SEditPartMaker.h"
#include "EditPartMakerSettings.h"

#include "AssetThumbnail.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "IContentBrowserSingleton.h"
#include "ISettingsModule.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SNullWidget.h"
#include "AEWToolUI.h"
#include "AEWTableUtils.h"

#define LOCTEXT_NAMESPACE "AEWEditPartMaker"

namespace EditPartMakerColumns
{
	static const FName Row(TEXT("Row"));
	static const FName PartsID(TEXT("PartsID"));
	static const FName Slot(TEXT("Slot"));
	static const FName Label(TEXT("Label"));
}

class SEditPartListRow : public SMultiColumnTableRow<TSharedPtr<FEditPartListItem>>
{
public:
	SLATE_BEGIN_ARGS(SEditPartListRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& Owner, TSharedPtr<FEditPartListItem> InItem)
	{
		Item = InItem;
		SMultiColumnTableRow<TSharedPtr<FEditPartListItem>>::Construct(FSuperRowType::FArguments(), Owner);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
	{
		FString Text;
		if (Column == EditPartMakerColumns::Row) Text = Item->Row;
		else if (Column == EditPartMakerColumns::PartsID) Text = Item->PartsID;
		else if (Column == EditPartMakerColumns::Slot) Text = Item->Slot;
		else Text = Item->Label;
		return SNew(SBox).Padding(FMargin(4.f, 1.f))[ SNew(STextBlock).Text(FText::FromString(Text)) ];
	}

private:
	TSharedPtr<FEditPartListItem> Item;
};

void SEditPartMaker::Construct(const FArguments& InArgs)
{
	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(16));
	Request.ThumbnailPath = GetDefault<UEditPartMakerSettings>()->DefaultThumbnail.ToString();
	Request.CompatibleCategory = TEXT("---");
	Request.WACategory = TEXT("None");

	TSharedRef<SWidget> Toolbar = AEWToolUI::MakeToolbar(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("UseSelected", "Use selected mesh"), LOCTEXT("UseSelectedTip", "Use the skeletal mesh selected in the Content Browser"),
				[this]() { UseContentBrowserSelection(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("Reload", "Reload tables"), LOCTEXT("ReloadTip", "Reload the tables named in the settings."),
				[this]() { ReloadTables(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 4, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("Check", "Check"), LOCTEXT("CheckTip", "Validate the fields without changing anything."),
				[this]() { CheckOnly(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
		[
			AEWToolUI::MakePrimaryButton(LOCTEXT("MakePartButton", "Make part"), LOCTEXT("MakePartTip", "Create the WA_ asset, the parts row and a default-unlocked unlock row."),
				[this]() { DoMake(); }, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]() { return Tables.IsUsable() && Request.Mesh.IsValid(); })))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNullWidget::NullWidget
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 0, 0)
		[
			AEWToolUI::MakeButton(LOCTEXT("Settings", "Settings..."), LOCTEXT("SettingsTip", "Project Settings > Plugins > Edit Part Maker"),
				[this]() { OpenSettings(); })
		],
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SEditPartMaker::GetStatusText)));

	TSharedRef<SWidget> List = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("Search", "Filter parts"))
			.OnTextChanged_Lambda([this](const FText& Text) { FilterText = Text.ToString(); ApplyFilter(); })
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FEditPartListItem>>)
			.ListItemsSource(&VisibleParts)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SEditPartMaker::OnGenerateRow)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(EditPartMakerColumns::Row).DefaultLabel(LOCTEXT("ColRow", "Row")).FillWidth(0.35f)
				+ SHeaderRow::Column(EditPartMakerColumns::PartsID).DefaultLabel(LOCTEXT("ColId", "Parts ID")).FillWidth(0.15f)
				+ SHeaderRow::Column(EditPartMakerColumns::Slot).DefaultLabel(LOCTEXT("ColSlot", "Slot")).FillWidth(0.2f)
				+ SHeaderRow::Column(EditPartMakerColumns::Label).DefaultLabel(LOCTEXT("ColLabel", "Label")).FillWidth(0.3f)
			)
		];

	TSharedRef<SWidget> Side = AEWToolUI::MakeSidePanel(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("MeshSection", "Mesh"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Mesh", "Skeletal mesh"),
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(USkeletalMesh::StaticClass())
						.ObjectPath_Lambda([this]() { return Request.Mesh.IsValid() ? Request.Mesh->GetPathName() : FString(); })
						.OnObjectChanged_Lambda([this](const FAssetData& Asset) { SetMesh(Cast<USkeletalMesh>(Asset.GetAsset())); })
						.ThumbnailPool(ThumbnailPool)
						.AllowClear(true)
						.DisplayThumbnail(true))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Thumb", "Thumbnail"),
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(UTexture2D::StaticClass())
						.ObjectPath_Lambda([this]() { return Request.ThumbnailPath; })
						.OnObjectChanged_Lambda([this](const FAssetData& Asset) { Request.ThumbnailPath = Asset.IsValid() ? Asset.ObjectPath.ToString() : FString(); })
						.ThumbnailPool(ThumbnailPool)
						.AllowClear(true))
				]
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("WAName", "Parts asset"), MakeTextField(&Request.WAName, LOCTEXT("WANameHint", "WA_..."))) ])
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("RowSection", "Part"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("RowName", "Row name"), MakeTextField(&Request.RowName, LOCTEXT("RowNameHint", "e.g. MyPart_0001")),
					LOCTEXT("RowNameTip", "DataTable row name (no spaces).")) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("Label", "Menu label"), MakeTextField(&Request.Label, LOCTEXT("LabelHint", "Name shown in the CAW menu"))) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("Slot", "Edit slot"), SAssignNew(SlotCombo, STextComboBox).OptionsSource(&SlotOptions)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.Slot = *V; })) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("Gender", "Gender"), SAssignNew(GenderCombo, STextComboBox).OptionsSource(&GenderOptions)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.Gender = *V; })) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("Face", "Face type"), SAssignNew(FaceCombo, STextComboBox).OptionsSource(&FaceOptions)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.FaceType = *V; }),
					LOCTEXT("FaceTip", "Default for most parts. Heads use one face type each; reuse makes the menu treat them as one entry.")) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("Attire", "Attire"), SAssignNew(AttireCombo, STextComboBox).OptionsSource(&AttireOptions)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.Attire = *V; })) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("WACat", "Category"), SAssignNew(WACategoryCombo, STextComboBox).OptionsSource(&WACategoryOptions)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.WACategory = *V; }),
					LOCTEXT("WACatTip", "DT_PartsCategory row: decides which body sections are masked and which parts are hidden. None for heads.")) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("CompatCat", "Compatible"), SAssignNew(CompatibleCombo, STextComboBox).OptionsSource(&CompatibleOptions)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.CompatibleCategory = *V; }),
					LOCTEXT("CompatTip", "The parts row's CommpatiablePartsCategory. Vanilla leaves --- for most parts.")) ])
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			AEWToolUI::MakeSection(LOCTEXT("IdSection", "IDs"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("PartsId", "Parts ID"),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 4, 0)
						[
							SNew(SNumericEntryBox<int32>)
							.AllowSpin(false)
							.Value_Lambda([this]() { return TOptional<int32>(Request.PartsID); })
							.OnValueChanged_Lambda([this](int32 V) { Request.PartsID = V; })
							.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { Request.PartsID = V; })
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							AEWToolUI::MakeButton(LOCTEXT("SuggestParts", "Next free"), LOCTEXT("SuggestPartsTip", "Lowest PartsID not used by any part"),
								[this]() { SuggestIds(true, false); })
						],
						LOCTEXT("PartsIdTip", "Must be unique across all parts."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("UnlockId", "Unlock ID"),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 4, 0)
						[
							SNew(SNumericEntryBox<int32>)
							.AllowSpin(false)
							.Value_Lambda([this]() { return TOptional<int32>(Request.UnlockID); })
							.OnValueChanged_Lambda([this](int32 V) { Request.UnlockID = V; })
							.OnValueCommitted_Lambda([this](int32 V, ETextCommit::Type) { Request.UnlockID = V; })
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							AEWToolUI::MakeButton(LOCTEXT("SuggestUnlock", "Next free"), LOCTEXT("SuggestUnlockTip", "Lowest unlock ID in the custom range not used by any part or unlock row"),
								[this]() { SuggestIds(false, true); })
						],
						LOCTEXT("UnlockIdTip", "Gets its own default-unlocked row in the unlock table. Never shared with another part."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Overwrite", "Update existing"),
						SNew(SCheckBox)
						.IsChecked_Lambda([this]() { return Request.bOverwrite ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { Request.bOverwrite = (S == ECheckBoxState::Checked); }),
						LOCTEXT("OverwriteTip", "Replace a row this tool (or you) already added to the parts table."))
				])
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			AEWToolUI::MakeSection(LOCTEXT("SettingsArea", "Settings"), AEWToolUI::MakeSettingsView(GetMutableDefault<UEditPartMakerSettings>()), true)
		]);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Toolbar ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ AEWToolUI::MakeBody(List, Side, &LogText, 0.5f) ]
	];

	GetMutableDefault<UEditPartMakerSettings>()->OnSettingChanged().AddSP(this, &SEditPartMaker::OnSettingsChanged);
	ReloadTables();
	AddLog(TEXT("Pick a skeletal mesh (or select one and press Use selected mesh), check the fields, then Make part."));
}

TSharedRef<ITableRow> SEditPartMaker::OnGenerateRow(TSharedPtr<FEditPartListItem> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SEditPartListRow, Owner, Item);
}

void SEditPartMaker::RefreshPartList()
{
	AllParts.Reset();
	if (Tables.Parts && Tables.Parts->GetRowStruct())
	{
		const UScriptStruct* Struct = Tables.Parts->GetRowStruct();
		auto Read = [Struct](const uint8* Row, const TCHAR* Field)
		{
			FProperty* Prop = AEWTables::FindField(Struct, Field);
			if (FTextProperty* Text = CastField<FTextProperty>(Prop))
			{
				return Text->GetPropertyValue_InContainer(Row).ToString();
			}
			FString Value = AEWTables::GetAsText(Struct, Row, Field);
			int32 Colon = INDEX_NONE;
			return Value.FindLastChar(TEXT(':'), Colon) ? Value.Mid(Colon + 1) : Value;
		};
		for (const TPair<FName, uint8*>& Pair : Tables.Parts->GetRowMap())
		{
			TSharedPtr<FEditPartListItem> Item = MakeShared<FEditPartListItem>();
			Item->Row = Pair.Key.ToString();
			Item->PartsID = Read(Pair.Value, TEXT("PartsID"));
			Item->Slot = Read(Pair.Value, TEXT("EditSlot"));
			Item->Label = Read(Pair.Value, TEXT("Label"));
			AllParts.Add(Item);
		}
	}
	ApplyFilter();
}

void SEditPartMaker::ApplyFilter()
{
	VisibleParts.Reset();
	for (const TSharedPtr<FEditPartListItem>& Item : AllParts)
	{
		if (FilterText.IsEmpty() || Item->Row.Contains(FilterText) || Item->Label.Contains(FilterText) || Item->Slot.Contains(FilterText) || Item->PartsID.Contains(FilterText))
		{
			VisibleParts.Add(Item);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

FText SEditPartMaker::GetStatusText() const
{
	if (!Tables.IsUsable())
	{
		return LOCTEXT("NoTables", "Tables not loaded - check Settings.");
	}
	return FText::FromString(FString::Printf(TEXT("Parts table: %s (%d rows)    Unlock table: %s (%d rows)    Mesh: %s"),
		*Tables.Parts->GetName(), Tables.Parts->GetRowMap().Num(), *Tables.Unlock->GetName(), Tables.Unlock->GetRowMap().Num(),
		Request.Mesh.IsValid() ? *Request.Mesh->GetName() : TEXT("(none)")));
}

TSharedRef<SWidget> SEditPartMaker::MakeRow(const FText& Label, TSharedRef<SWidget> Content, const FText& Tooltip)
{
	return AEWToolUI::MakeRow(Label, Content, Tooltip);
}

TSharedRef<SWidget> SEditPartMaker::MakeTextField(FString* Target, const FText& Hint)
{
	return AEWToolUI::MakeTextField(Target, Hint);
}

void SEditPartMaker::FillOptions(TArray<TSharedPtr<FString>>& Out, const TArray<FString>& Values, const TArray<FString>& Leading)
{
	Out.Reset();
	for (const FString& V : Leading) Out.Add(MakeShared<FString>(V));
	for (const FString& V : Values)
	{
		if (!Leading.Contains(V)) Out.Add(MakeShared<FString>(V));
	}
}

void SEditPartMaker::Select(const TSharedPtr<STextComboBox>& Combo, const TArray<TSharedPtr<FString>>& Options, const FString& Value, FString* Target)
{
	if (!Combo.IsValid())
	{
		return;
	}
	for (const TSharedPtr<FString>& Option : Options)
	{
		if (Option.IsValid() && *Option == Value)
		{
			Combo->SetSelectedItem(Option);
			*Target = Value;
			return;
		}
	}
	if (Options.Num() > 0)
	{
		Combo->SetSelectedItem(Options[0]);
		*Target = *Options[0];
	}
}

void SEditPartMaker::ReloadTables()
{
	TArray<FString> Errors;
	Tables = EditPartMaker::LoadTables(Errors);
	LogText.Reset();
	for (const FString& E : Errors) AddLog(TEXT("ERROR: ") + E);

	FillOptions(SlotOptions, EditPartMaker::GetEnumOptions(Tables.Parts, TEXT("EditSlot")));
	FillOptions(GenderOptions, EditPartMaker::GetEnumOptions(Tables.Parts, TEXT("GenderCondition")));
	FillOptions(FaceOptions, EditPartMaker::GetEnumOptions(Tables.Parts, TEXT("FaceTypeCondition")));
	FillOptions(AttireOptions, EditPartMaker::GetEnumOptions(Tables.Parts, TEXT("AttireTypeCondition")));
	const TArray<FString> Categories = EditPartMaker::GetCategoryOptions(Tables.Category);
	FillOptions(WACategoryOptions, Categories, TArray<FString>{ FString(TEXT("None")) });
	FillOptions(CompatibleOptions, Categories, TArray<FString>{ FString(TEXT("---")) });

	for (TSharedPtr<STextComboBox> Combo : { SlotCombo, GenderCombo, FaceCombo, AttireCombo, WACategoryCombo, CompatibleCombo })
	{
		if (Combo.IsValid()) Combo->RefreshOptions();
	}
	Select(SlotCombo, SlotOptions, Request.Slot.IsEmpty() ? FString(TEXT("Suits")) : Request.Slot, &Request.Slot);
	Select(GenderCombo, GenderOptions, Request.Gender.IsEmpty() ? FString(TEXT("Any")) : Request.Gender, &Request.Gender);
	Select(FaceCombo, FaceOptions, Request.FaceType.IsEmpty() ? FString(TEXT("Default")) : Request.FaceType, &Request.FaceType);
	Select(AttireCombo, AttireOptions, Request.Attire.IsEmpty() ? FString(TEXT("Any")) : Request.Attire, &Request.Attire);
	Select(WACategoryCombo, WACategoryOptions, Request.WACategory, &Request.WACategory);
	Select(CompatibleCombo, CompatibleOptions, Request.CompatibleCategory, &Request.CompatibleCategory);

	RefreshPartList();
	if (Tables.IsUsable())
	{
		AddLog(FString::Printf(TEXT("Tables loaded: %s (%d rows), %s (%d rows), %d categories."),
			*Tables.Parts->GetName(), Tables.Parts->GetRowMap().Num(), *Tables.Unlock->GetName(), Tables.Unlock->GetRowMap().Num(), Categories.Num()));
	}
}

void SEditPartMaker::SetMesh(USkeletalMesh* Mesh)
{
	if (!Mesh)
	{
		Request.Mesh.Reset();
		return;
	}
	EditPartMaker::FillDefaultsFromMesh(Mesh, Request);
	SuggestIds();
	AddLog(FString::Printf(TEXT("Mesh: %s"), *Mesh->GetPathName()));
}

void SEditPartMaker::OnSettingsChanged(UObject*, FPropertyChangedEvent&)
{
	ReloadTables();
}

void SEditPartMaker::OpenSettings()
{
	AEWToolUI::OpenSettings(GetDefault<UEditPartMakerSettings>()->GetSectionName());
}

void SEditPartMaker::SuggestIds(bool bPartsID, bool bUnlockID)
{
	if (!Tables.IsUsable())
	{
		return;
	}
	int32 PartsID = 0;
	int32 UnlockID = 0;
	EditPartMaker::SuggestIds(Tables, Request.bOverwrite ? Request.RowName : FString(), PartsID, UnlockID);
	if (bPartsID) Request.PartsID = PartsID;
	if (bUnlockID) Request.UnlockID = UnlockID;
	AddLog(FString::Printf(TEXT("Next free: PartsID %d, unlock ID %d"), Request.PartsID, Request.UnlockID));
}

void SEditPartMaker::UseContentBrowserSelection()
{
	FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> Selected;
	ContentBrowser.Get().GetSelectedAssets(Selected);
	for (const FAssetData& Asset : Selected)
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset()))
		{
			SetMesh(Mesh);
			return;
		}
	}
	AddLog(TEXT("No skeletal mesh selected in the Content Browser."));
}

void SEditPartMaker::CheckOnly()
{
	TArray<FString> Errors;
	TArray<FString> Warnings;
	EditPartMaker::Validate(Tables, Request, Errors, Warnings);
	AddLog(TEXT("---- check"));
	for (const FString& E : Errors) AddLog(TEXT("ERROR: ") + E);
	for (const FString& W : Warnings) AddLog(TEXT("warning: ") + W);
	if (Errors.Num() == 0) AddLog(TEXT("OK - ready to make."));
}

void SEditPartMaker::DoMake()
{
	AddLog(TEXT("---- make ") + Request.RowName);
	TArray<FString> Log;
	const bool bOk = EditPartMaker::MakePart(Tables, Request, Log);
	for (const FString& Line : Log) AddLog(Line);
	AddLog(bOk ? TEXT("Done.") : TEXT("Failed - see errors above."));
	if (bOk)
	{
		Request.bOverwrite = true;
		RefreshPartList();
	}
}

void SEditPartMaker::AddLog(const FString& Line)
{
	UE_LOG(LogAEWEditPartMaker, Log, TEXT("%s"), *Line);
	LogText += Line + TEXT("\n");
}

#undef LOCTEXT_NAMESPACE
