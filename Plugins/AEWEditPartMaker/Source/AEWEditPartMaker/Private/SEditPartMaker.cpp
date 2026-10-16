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

#define LOCTEXT_NAMESPACE "AEWEditPartMaker"

void SEditPartMaker::Construct(const FArguments& InArgs)
{
	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(16));
	Request.ThumbnailPath = GetDefault<UEditPartMakerSettings>()->DefaultThumbnail.ToString();
	Request.CompatibleCategory = TEXT("---");
	Request.WACategory = TEXT("None");

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(8.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT("Intro", "Pick an imported skeletal mesh, check the fields, then Make part. Creates the WA_ asset, the parts row and a unique default-unlocked unlock row."))
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeRow(LOCTEXT("Mesh", "Skeletal mesh"),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(USkeletalMesh::StaticClass())
						.ObjectPath_Lambda([this]() { return Request.Mesh.IsValid() ? Request.Mesh->GetPathName() : FString(); })
						.OnObjectChanged_Lambda([this](const FAssetData& Asset) { SetMesh(Cast<USkeletalMesh>(Asset.GetAsset())); })
						.ThumbnailPool(ThumbnailPool)
						.AllowClear(true)
						.DisplayThumbnail(true)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Center)
					[
						SNew(SButton)
						.Text(LOCTEXT("UseSelected", "Use selected"))
						.ToolTipText(LOCTEXT("UseSelectedTip", "Use the skeletal mesh selected in the Content Browser"))
						.OnClicked_Lambda([this]() { UseContentBrowserSelection(); return FReply::Handled(); })
					])
			]

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
			+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("Attire", "Attire condition"), SAssignNew(AttireCombo, STextComboBox).OptionsSource(&AttireOptions)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.Attire = *V; })) ]
			+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("WACat", "Parts category"), SAssignNew(WACategoryCombo, STextComboBox).OptionsSource(&WACategoryOptions)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.WACategory = *V; }),
				LOCTEXT("WACatTip", "DT_PartsCategory row: decides which body sections are masked and which parts are hidden. None for heads.")) ]
			+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("CompatCat", "Compatible category"), SAssignNew(CompatibleCombo, STextComboBox).OptionsSource(&CompatibleOptions)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V.IsValid()) Request.CompatibleCategory = *V; }),
				LOCTEXT("CompatTip", "The parts row's CommpatiablePartsCategory. Vanilla leaves --- for most parts.")) ]

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
			+ SVerticalBox::Slot().AutoHeight()[ MakeRow(LOCTEXT("WAName", "Parts name"), MakeTextField(&Request.WAName, LOCTEXT("WANameHint", "WA_..."))) ]

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
						SNew(SButton)
						.Text(LOCTEXT("SuggestParts", "Next free"))
						.ToolTipText(LOCTEXT("SuggestPartsTip", "Lowest PartsID not used by any part"))
						.OnClicked_Lambda([this]() { SuggestIds(true, false); return FReply::Handled(); })
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
						SNew(SButton)
						.Text(LOCTEXT("SuggestUnlock", "Next free"))
						.ToolTipText(LOCTEXT("SuggestUnlockTip", "Lowest unlock ID in the custom range not used by any part or unlock row"))
						.OnClicked_Lambda([this]() { SuggestIds(false, true); return FReply::Handled(); })
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
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					SNew(SButton).Text(LOCTEXT("Check", "Check"))
					.OnClicked_Lambda([this]() { CheckOnly(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					SNew(SButton).Text(LOCTEXT("MakePartButton", "Make part"))
					.ButtonStyle(&FEditorStyle::Get(), "FlatButton.Success")
					.ForegroundColor(FLinearColor::White)
					.OnClicked_Lambda([this]() { DoMake(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					SNew(SButton).Text(LOCTEXT("Reload", "Reload tables"))
					.OnClicked_Lambda([this]() { ReloadTables(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("Settings", "Tables & settings..."))
					.ToolTipText(LOCTEXT("SettingsTip", "Project Settings > Plugins > AEW Edit Part Maker"))
					.OnClicked_Lambda([this]() { OpenSettings(); return FReply::Handled(); })
				]
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).MinDesiredHeight(160.f)
				[
					SNew(SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.AutoWrapText(true)
					.Text_Lambda([this]() { return FText::FromString(LogText); })
				]
			]
		]
	];

	GetMutableDefault<UEditPartMakerSettings>()->OnSettingChanged().AddSP(this, &SEditPartMaker::OnSettingsChanged);
	ReloadTables();
}

TSharedRef<SWidget> SEditPartMaker::MakeRow(const FText& Label, TSharedRef<SWidget> Content, const FText& Tooltip)
{
	return SNew(SHorizontalBox)
		.ToolTipText(Tooltip)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2, 8, 2)
		[
			SNew(SBox).WidthOverride(150.f)
			[
				SNew(STextBlock).Text(Label)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0, 2)
		[
			Content
		];
}

TSharedRef<SWidget> SEditPartMaker::MakeTextField(FString* Target, const FText& Hint)
{
	return SNew(SEditableTextBox)
		.HintText(Hint)
		.Text_Lambda([Target]() { return FText::FromString(*Target); })
		.OnTextChanged_Lambda([Target](const FText& T) { *Target = T.ToString(); });
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
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Plugins", GetDefault<UEditPartMakerSettings>()->GetSectionName());
	}
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
	}
}

void SEditPartMaker::AddLog(const FString& Line)
{
	UE_LOG(LogAEWEditPartMaker, Log, TEXT("%s"), *Line);
	LogText += Line + TEXT("\n");
}

#undef LOCTEXT_NAMESPACE
