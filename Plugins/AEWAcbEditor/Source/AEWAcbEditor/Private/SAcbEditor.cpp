#include "SAcbEditor.h"
#include "AcbDocument.h"
#include "AcbImportBridge.h"
#include "AcbPreviewPlayer.h"

#include "DesktopPlatformModule.h"
#include "Dialogs/DlgPickPath.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "ISettingsModule.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "AEWAcbEditor"

namespace AcbEditorColumns
{
	static const FName Id(TEXT("Id"));
	static const FName Name(TEXT("Name"));
	static const FName Categories(TEXT("Categories"));
	static const FName Waves(TEXT("Waves"));
	static const FName PlayMode(TEXT("PlayMode"));
	static const FName Length(TEXT("Length"));
	static const FName Audio(TEXT("Audio"));
}

namespace AcbEditorText
{
	static const TCHAR* DefaultCategory = TEXT("(template / default)");
	static const TCHAR* AudioFilter = TEXT("Audio (*.hca;*.adx;*.wav)|*.hca;*.adx;*.wav");
}

class SAcbCueTableRow : public SMultiColumnTableRow<TSharedPtr<FAcbCueRow>>
{
public:
	SLATE_BEGIN_ARGS(SAcbCueTableRow) {}
	SLATE_ARGUMENT(TSharedPtr<FAcbCueRow>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& Owner)
	{
		Item = InArgs._Item;
		SMultiColumnTableRow<TSharedPtr<FAcbCueRow>>::Construct(FSuperRowType::FArguments(), Owner);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
	{
		FString Text;
		if (Column == AcbEditorColumns::Id) Text = FString::Printf(TEXT("%u"), Item->Id);
		else if (Column == AcbEditorColumns::Name) Text = Item->Name;
		else if (Column == AcbEditorColumns::Categories) Text = Item->Categories;
		else if (Column == AcbEditorColumns::Waves) Text = FString::FromInt(Item->NumWaveforms);
		else if (Column == AcbEditorColumns::PlayMode) Text = Item->PlayMode;
		else if (Column == AcbEditorColumns::Length) Text = Item->Length;
		else if (Column == AcbEditorColumns::Audio) Text = Item->Audio + (Item->bLoop ? TEXT(" (loop)") : TEXT(""));
		return SNew(SBox).Padding(FMargin(4.f, 1.f))
		[
			SNew(STextBlock).Text(FText::FromString(Text))
		];
	}

private:
	TSharedPtr<FAcbCueRow> Item;
};

void SAcbEditor::Construct(const FArguments& InArgs)
{
	const UAcbEditorSettings* Settings = GetDefault<UAcbEditorSettings>();
	bLoopNewTrack = Settings->bLoopNewTracks;

	for (uint8 Mode : { uint8(4), uint8(3), uint8(1), uint8(0) })
	{
		PlayModeValues.Add(Mode);
		PlayModeOptions.Add(MakeShared<FString>(UTF8_TO_TCHAR(AcbCore::SequenceTypeName(Mode))));
	}
	CategoryOptions.Add(MakeShared<FString>(AcbEditorText::DefaultCategory));
	RefreshPresets();

	auto IsOpen = [this]() { return Session.IsOpen(); };
	auto HasSelection = [this]() { return Session.IsOpen() && GetFirstSelected().IsValid(); };

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(6.f, 4.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
					[
						SNew(SButton).Text(LOCTEXT("Open", "Open..."))
						.ToolTipText(LOCTEXT("OpenTip", "Open an .acb (with its .awb) or a cue sheet .uasset/.uexp."))
						.OnClicked_Lambda([this]() { OnOpenClicked(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
					[
						SNew(SButton).Text(LOCTEXT("Reload", "Reload"))
						.ToolTipText(LOCTEXT("ReloadTip", "Discard changes and reload."))
						.IsEnabled_Lambda(IsOpen)
						.OnClicked_Lambda([this]() { OnReloadClicked(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 8, 0)
					[
						SNew(SButton).Text(LOCTEXT("Save", "Save"))
						.ToolTipText_Lambda([]()
						{
							return GetDefault<UAcbEditorSettings>()->bImportAfterSave
								? LOCTEXT("SaveImportTip", "Write the .acb (and .awb) to the output folder, then import into the cue sheet folder: cue assets, DataTables and deleted cue assets are updated. (Settings > Import after save)")
								: LOCTEXT("SaveOnlyTip", "Write the .acb (and .awb) to the output folder. Import after save is off in Settings, so nothing in the project changes.");
						})
						.ButtonStyle(&FEditorStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FLinearColor::White)
						.IsEnabled_Lambda(IsOpen)
						.OnClicked_Lambda([this]() { OnSaveClicked(GetDefault<UAcbEditorSettings>()->bImportAfterSave); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ToolTipText(LOCTEXT("ImportFolderTip", "Where Save imports the cue sheet. Also set by right-clicking a Content Browser folder."))
						.ColorAndOpacity_Lambda([this]() { return ImportFolder.IsEmpty() ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground(); })
						.Text_Lambda([this]()
						{
							return ImportFolder.IsEmpty()
								? LOCTEXT("ImportFolderNone", "into: (ask)")
								: FText::Format(LOCTEXT("ImportFolderSet", "into: {0}"), FText::FromString(ImportFolder));
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
					[
						SNew(SButton).Text(LOCTEXT("ChangeImportFolder", "Change..."))
						.ToolTipText(LOCTEXT("ChangeImportFolderTip", "Pick the import folder."))
						.IsEnabled_Lambda(IsOpen)
						.OnClicked_Lambda([this]()
						{
							FString Picked;
							if (PickCueSheetFolder(Picked))
							{
								AddLog(TEXT("Import folder: ") + Picked);
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 0, 0)
					[
						SNew(SButton).Text(LOCTEXT("Settings", "Settings..."))
						.ToolTipText(LOCTEXT("SettingsTip", "Project Settings > Plugins > AEW ACB Editor"))
						.OnClicked_Lambda([this]() { OpenSettings(); return FReply::Handled(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text_Lambda([this]() { return FText::FromString(Session.Describe()); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
					[
						SNew(STextBlock)
						.ToolTipText(LOCTEXT("PendingTip", "DataTable changes made on the next Import."))
						.Text(this, &SAcbEditor::GetPendingText)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SAcbEditor::GetSizeText)
						.ColorAndOpacity(this, &SAcbEditor::GetSizeColor)
					]
				]
			]
		]

		+ SVerticalBox::Slot().FillHeight(1.f).Padding(6.f, 6.f, 6.f, 6.f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+ SSplitter::Slot().Value(0.82f)
			[
				SNew(SSplitter)
				+ SSplitter::Slot().Value(0.58f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
					[
						SNew(SSearchBox)
						.HintText(LOCTEXT("Search", "Filter cues"))
						.OnTextChanged_Lambda([this](const FText& Text) { FilterText = Text.ToString(); ApplyFilter(); })
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(ListView, SListView<TSharedPtr<FAcbCueRow>>)
						.ListItemsSource(&VisibleRows)
						.SelectionMode(ESelectionMode::Multi)
						.OnGenerateRow(this, &SAcbEditor::OnGenerateRow)
						.OnSelectionChanged(this, &SAcbEditor::OnSelectionChanged)
						.HeaderRow
						(
							SNew(SHeaderRow)
							+ SHeaderRow::Column(AcbEditorColumns::Id).DefaultLabel(LOCTEXT("ColId", "Id")).FillWidth(0.07f)
							+ SHeaderRow::Column(AcbEditorColumns::Name).DefaultLabel(LOCTEXT("ColName", "Cue name")).FillWidth(0.33f)
							+ SHeaderRow::Column(AcbEditorColumns::Categories).DefaultLabel(LOCTEXT("ColCat", "Category")).FillWidth(0.15f)
							+ SHeaderRow::Column(AcbEditorColumns::Waves).DefaultLabel(LOCTEXT("ColWaves", "Waves")).FillWidth(0.06f)
							+ SHeaderRow::Column(AcbEditorColumns::PlayMode).DefaultLabel(LOCTEXT("ColMode", "Play mode")).FillWidth(0.13f)
							+ SHeaderRow::Column(AcbEditorColumns::Length).DefaultLabel(LOCTEXT("ColLength", "Length")).FillWidth(0.07f)
							+ SHeaderRow::Column(AcbEditorColumns::Audio).DefaultLabel(LOCTEXT("ColAudio", "Audio")).FillWidth(0.19f)
						)
					]
				]
				+ SSplitter::Slot().Value(0.42f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot().Padding(6.f, 0.f, 2.f, 0.f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[
							MakeSection(LOCTEXT("SelectedSection", "Selected cue"),
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
									[
										SNew(STextBlock).AutoWrapText(true)
										.Text_Lambda([this]()
										{
											const TArray<TSharedPtr<FAcbCueRow>> Selection = GetSelection();
											if (Selection.Num() == 0) return LOCTEXT("NoSelection", "No cue selected");
											if (Selection.Num() > 1) return FText::Format(LOCTEXT("MultiSelection", "{0} cues selected"), Selection.Num());
											const FAcbCueRow& Row = *Selection[0];
											return FText::FromString(FString::Printf(TEXT("%s  (id %u, %d wave(s)%s%s)"), *Row.Name, Row.Id, Row.NumWaveforms,
												Row.PlayMode.IsEmpty() ? TEXT("") : TEXT(", "), *Row.PlayMode));
										})
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
									[
										SNew(SButton).Text(LOCTEXT("Delete", "Delete"))
										.ToolTipText(LOCTEXT("DeleteTip", "Delete the selected cues."))
										.ButtonStyle(&FEditorStyle::Get(), "FlatButton.Danger")
										.ForegroundColor(FLinearColor::White)
										.IsEnabled_Lambda(HasSelection)
										.OnClicked_Lambda([this]() { DeleteSelected(); return FReply::Handled(); })
									]
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("Rename", "Rename"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeTextField(&RenameTo, LOCTEXT("RenameHint", "New name")) ]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("RenameButton", "Rename"))
											.IsEnabled_Lambda(HasSelection)
											.OnClicked_Lambda([this]() { RenameSelected(); return FReply::Handled(); })
										])
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("Copy", "Copy"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeTextField(&CopyTo, LOCTEXT("CopyHint", "New name")) ]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("CopyButton", "Copy"))
											.ToolTipText(LOCTEXT("CopyTip", "Duplicate this cue under a new name."))
											.IsEnabled_Lambda(HasSelection)
											.OnClicked_Lambda([this]() { CopySelected(); return FReply::Handled(); })
										])
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("SelectedCategory", "Category"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f)[ (SelectedCategoryCombo = MakeCategoryCombo(&SelectedCategory)).ToSharedRef() ]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("SetCategory", "Set"))
											.ToolTipText(LOCTEXT("SetCategoryTip", "Give the selected cues this category. Their other sound settings stay."))
											.IsEnabled_Lambda(HasSelection)
											.OnClicked_Lambda([this]() { SetCategoryOfSelected(); return FReply::Handled(); })
										])
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("CueVolume", "Volume"), MakeVolumeSlider(false),
										LOCTEXT("CueVolumeRowTip", "Volume multiplier for the whole cue, on top of each track's volume (1.0 = unchanged, 2.0 = double). Drag, or double-click to type; it is set when you let go. Vanilla cues use up to about 3.2; the game clamps the final output, so very high values can distort. x removes it."))
								])
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[
							MakeSection(LOCTEXT("VariationsLabel", "Tracks"),
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("Slot", "Track"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f)
										[
											SAssignNew(SlotCombo, STextComboBox)
											.OptionsSource(&SlotOptions)
											.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type)
											{
												SelectedSlot = FMath::Max(0, SlotOptions.IndexOfByKey(V));
												LoadTiming(false);
												LoadVolumes();
											})
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("AddVariation", "Add..."))
											.ToolTipText(LOCTEXT("AddVariationTip", "Add files as tracks."))
											.IsEnabled_Lambda(HasSelection)
											.OnClicked_Lambda([this]() { AddVariations(); return FReply::Handled(); })
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("ReplaceVariation", "Replace..."))
											.ToolTipText(LOCTEXT("ReplaceVariationTip", "Replace this track's audio (this cue only)."))
											.IsEnabled_Lambda(HasSelection)
											.OnClicked_Lambda([this]() { ReplaceVariation(); return FReply::Handled(); })
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("RemoveVariation", "Remove"))
											.IsEnabled_Lambda(HasSelection)
											.OnClicked_Lambda([this]() { RemoveVariation(); return FReply::Handled(); })
										])
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("PlayMode", "Play mode"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f)[ MakePlayModeCombo(&SelectedPlayMode) ]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("ApplyPlayMode", "Set"))
											.IsEnabled_Lambda(HasSelection)
											.OnClicked_Lambda([this]() { ApplyPlayMode(); return FReply::Handled(); })
										])
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("SlotVolume", "Volume"), MakeVolumeSlider(true),
										LOCTEXT("SlotVolumeRowTip", "Volume multiplier for the chosen track, this cue only (1.0 = unchanged). Drag, or double-click to type; it is set when you let go. Vanilla music tracks sit at 0.25-0.56, so raising a quiet track toward 1.0 boosts it. x removes it."))
								])
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[
							MakeSection(LOCTEXT("TimingLabel", "Start and loop"),
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
								[
									SNew(SAcbTimeline)
									.ToolTipText(LOCTEXT("TimelineTip", "Click or drag to play from a point. Drag S to move the start point, [ and ] to move the loop start and end (Loop must be on). Moved markers fill the fields below; Apply (or Play) sets them."))
									.IsEnabled_Lambda([this]() { return CachedTiming.bValid; })
									.NumFrames_Lambda([this]() { return CachedTiming.bValid ? CachedTiming.NumSamples : int64(0); })
									.SampleRate_Lambda([this]() { return CachedTiming.SamplingRate; })
									.Position_Lambda([this]() { return GetTimelinePosition(); })
									.Start_Lambda([this]() { return ParseTimingField(TimingStart); })
									.LoopStart_Lambda([this]() { return ParseTimingField(TimingLoopStart); })
									.LoopEnd_Lambda([this]() { return ParseTimingField(TimingLoopEnd); })
									.Loop_Lambda([this]() { return bTimingLoop; })
									.CanSetStart_Lambda([this]() { return CachedTiming.bCanSetStart; })
									.Peaks(&Peaks)
									.OnSeek_Lambda([this](int64 Frame) { SeekPreview(Frame); })
									.OnMarkerMoved_Lambda([this](EAcbTimelineMarker Marker, int64 Frame) { SetTimingMarker(Marker, Frame); })
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
									[
										SNew(SButton).Text(LOCTEXT("PreviewPlay", "Play"))
										.ToolTipText(LOCTEXT("PreviewPlayTip", "Play the chosen track from the cue's start point, looping as the cue does, through Unreal's audio (decoded with VGAudioCli). Typed or dragged start/loop values are applied first."))
										.IsEnabled_Lambda(HasSelection)
										.OnClicked_Lambda([this]() { PreviewCue(false); return FReply::Handled(); })
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
									[
										SNew(SButton)
										.Text_Lambda([this]() { return Preview.IsValid() && Preview->IsPaused() ? LOCTEXT("PreviewResume", "Resume") : LOCTEXT("PreviewPause", "Pause"); })
										.IsEnabled_Lambda([this]() { return Preview.IsValid() && Preview->IsPlaying(); })
										.OnClicked_Lambda([this]() { Preview->SetPaused(!Preview->IsPaused()); return FReply::Handled(); })
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
									[
										SNew(SButton).Text(LOCTEXT("PreviewStop", "Stop"))
										.IsEnabled_Lambda([this]() { return Preview.IsValid() && Preview->IsPlaying(); })
										.OnClicked_Lambda([this]() { StopPreview(); return FReply::Handled(); })
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
									[
										SNew(SButton).Text(LOCTEXT("PreviewLoop", "Play loop end"))
										.ToolTipText(LOCTEXT("PreviewLoopTip", "Start 4 s before the loop end, to hear the jump back to the loop start."))
										.IsEnabled_Lambda([this, HasSelection]() { return HasSelection() && bTimingLoop; })
										.OnClicked_Lambda([this]() { PreviewCue(true); return FReply::Handled(); })
									]
									+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
									[
										SNew(STextBlock).Text(this, &SAcbEditor::GetPreviewText)
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
								[
									MakeRow(LOCTEXT("SetHere", "Set at playhead"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
										[
											SNew(SButton).Text(LOCTEXT("SetStartHere", "Start"))
											.IsEnabled_Lambda([this]() { return GetTimelinePosition() >= 0 && CachedTiming.bCanSetStart; })
											.OnClicked_Lambda([this]() { SetTimingMarker(EAcbTimelineMarker::Start, GetTimelinePosition()); return FReply::Handled(); })
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
										[
											SNew(SButton).Text(LOCTEXT("SetLoopStartHere", "Loop start"))
											.IsEnabled_Lambda([this]() { return GetTimelinePosition() >= 0; })
											.OnClicked_Lambda([this]() { SetTimingMarker(EAcbTimelineMarker::LoopStart, GetTimelinePosition()); return FReply::Handled(); })
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(SButton).Text(LOCTEXT("SetLoopEndHere", "Loop end"))
											.IsEnabled_Lambda([this]() { return GetTimelinePosition() >= 0; })
											.OnClicked_Lambda([this]() { SetTimingMarker(EAcbTimelineMarker::LoopEnd, GetTimelinePosition()); return FReply::Handled(); })
										],
										LOCTEXT("SetHereTip", "While the track plays (or is paused), put the start point or a loop point where the playhead is. Apply sets them."))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("StartAt", "Start at"), MakeTextField(&TimingStart, LOCTEXT("StartAtHint", "0")),
										LOCTEXT("StartAtTip", "Where playback starts (ms precision)."))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("LoopRow", "Loop"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
										[
											SNew(SCheckBox)
											.IsChecked_Lambda([this]() { return bTimingLoop ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bTimingLoop = S == ECheckBoxState::Checked; })
										]
										+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 4, 0)[ MakeTextField(&TimingLoopStart, LOCTEXT("LoopStartHint", "Loop start")) ]
										+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeTextField(&TimingLoopEnd, LOCTEXT("LoopEndHint", "Loop end")) ],
										LOCTEXT("LoopRowTip", "Off plays to the end. On loops from end back to start."))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
									[
										SNew(SCheckBox)
										.ToolTipText(LOCTEXT("SamplesTip", "Enter samples instead of seconds."))
										.IsChecked_Lambda([this]() { return bTimingSamples ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
										.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { SetTimingUnits(S == ECheckBoxState::Checked); })
										[
											SNew(STextBlock).Text(LOCTEXT("Samples", "Samples"))
										]
									]
									+ SHorizontalBox::Slot().FillWidth(1.f)
									+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
									[
										SNew(SButton).Text(LOCTEXT("ResetTiming", "Reload"))
										.ToolTipText(LOCTEXT("ResetTimingTip", "Show current values."))
										.IsEnabled_Lambda(HasSelection)
										.OnClicked_Lambda([this]() { LoadTiming(true); return FReply::Handled(); })
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
									[
										SNew(SButton).Text(LOCTEXT("ApplyTiming", "Apply"))
										.IsEnabled_Lambda([this]() { return CachedTiming.bValid; })
										.OnClicked_Lambda([this]() { ApplyTiming(); return FReply::Handled(); })
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
								[
									SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
									.Text_Lambda([this]()
									{
										const FAcbSlotTiming& T = CachedTiming;
										if (!T.bValid) return FText::GetEmpty();
										FString Line = FString::Printf(TEXT("Audio %.3fs = %lld samples at %d Hz."), T.SamplingRate ? double(T.NumSamples) / T.SamplingRate : 0.0,
											T.NumSamples, T.SamplingRate);
										if (!T.bCanSetStart) Line += TEXT(" No track, so no start point.");
										if (T.SharedBy > 1) Line += FString::Printf(TEXT(" Shared by %d cues; a new loop region gives this cue its own copy."), T.SharedBy);
										if (T.bWaveformLoops && !T.bLoop) Line += TEXT(" Loop points off for this cue.");
										return FText::FromString(Line);
									})
								])
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[
							MakeSection(LOCTEXT("AddTrackSection", "Add audio"),
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("AudioFile", "Audio"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
										[
											SNew(STextBlock)
											.Text_Lambda([this]()
											{
												return NewAudioFile.IsEmpty() ? LOCTEXT("NoFile", "(none)") : FText::FromString(FPaths::GetCleanFilename(NewAudioFile));
											})
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("Browse", "Browse..."))
											.ToolTipText(LOCTEXT("BrowseTip", "Pick an .hca, .adx or .wav file."))
											.OnClicked_Lambda([this]()
											{
												TArray<FString> Files;
												if (PickFiles(TEXT("Audio file"), AcbEditorText::AudioFilter, false, GetDefault<UAcbEditorSettings>()->LastAudioFolder, Files) && Files.Num() > 0)
												{
													NewAudioFile = Files[0];
													RememberFolder(Files[0]);
												}
												return FReply::Handled();
											})
										])
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("Template", "Copy from"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeTextField(&TemplateCueName, LOCTEXT("TemplateHint", "Cue")) ]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("UseSelected", "Use selected"))
											.OnClicked_Lambda([this]()
											{
												if (const TSharedPtr<FAcbCueRow> Row = GetFirstSelected()) { TemplateCueName = Row->Name; }
												return FReply::Handled();
											})
										],
										LOCTEXT("TemplateTip", "Copy sound settings from this cue. Empty = ACB defaults."))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("Preset", "Preset"),
										SAssignNew(PresetCombo, STextComboBox)
										.OptionsSource(&PresetOptions)
										.InitiallySelectedItem(PresetOptions[0])
										.OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type)
										{
											if (!bRefreshingPresets)
											{
												ApplyPreset(PresetOptions.IndexOfByKey(V));
											}
										}),
										LOCTEXT("PresetTip", "Fills the rows below. Any row's name, category and copy from can then be changed. Every cue plays the same audio."))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
								[
									SNew(SBorder)
									.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
									.Padding(4.f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().FillWidth(0.44f)
											[
												SNew(STextBlock).Text(LOCTEXT("ColCueName", "Cue name")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
											]
											+ SHorizontalBox::Slot().FillWidth(0.28f).Padding(4, 0, 0, 0)
											[
												SNew(STextBlock).Text(LOCTEXT("ColCueCategory", "Category")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
											]
											+ SHorizontalBox::Slot().FillWidth(0.28f).Padding(4, 0, 0, 0)
											[
												SNew(STextBlock).Text(LOCTEXT("ColCueCopyFrom", "Copy from")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
											]
											+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
											[
												SNew(SBox).WidthOverride(22.f)
											]
										]
										+ SVerticalBox::Slot().AutoHeight()
										[
											SAssignNew(CueRowsBox, SVerticalBox)
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth()
											[
												SNew(SButton).Text(LOCTEXT("AddCueRow", "+ cue"))
												.ToolTipText(LOCTEXT("AddCueRowTip", "Add a cue that plays the same audio."))
												.OnClicked_Lambda([this]()
												{
													CueTemplates.Add(MakeShared<FAcbCueTemplate>());
													RebuildCueRows();
													return FReply::Handled();
												})
											]
											+ SHorizontalBox::Slot().FillWidth(1.f)
											+ SHorizontalBox::Slot().AutoWidth()
											[
												SNew(SButton).Text(LOCTEXT("ResetCueRows", "Reset"))
												.ToolTipText(LOCTEXT("ResetCueRowsTip", "Undo your changes to the rows (back to the preset)."))
												.OnClicked_Lambda([this]() { ApplyPreset(CurrentPreset); return FReply::Handled(); })
											]
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("Loop", "Loop"),
										SNew(SCheckBox)
										.IsChecked_Lambda([this]() { return bLoopNewTrack ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
										.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bLoopNewTrack = S == ECheckBoxState::Checked; }),
										LOCTEXT("LoopTip", "Uses the HCA's loop points, else the whole file."))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("TableRows", "DataTables"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
										[
											SNew(STextBlock)
											.AutoWrapText(true)
											.ColorAndOpacity(FSlateColor::UseSubduedForeground())
											.Text_Lambda([this]()
											{
												return TableCandidates.Num() == 0
													? LOCTEXT("NoTableRows", "No Cue Table uses the copy-from cues.")
													: LOCTEXT("SomeTableRows", "Rows copied on Save:");
											})
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("FindTableRows", "Find"))
											.ToolTipText(LOCTEXT("FindTableRowsTip", "Look again for rows that use the copy-from cues."))
											.IsEnabled_Lambda(IsOpen)
											.OnClicked_Lambda([this]() { FindTableRows(); return FReply::Handled(); })
										],
										LOCTEXT("TableRowsTip", "Rows in the Cue Tables (Settings > Tables) that use the copy-from cues. On Save each is copied into its New Table with the new cues."))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SAssignNew(TableRowsBox, SVerticalBox)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(1.f)
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(SButton).Text(LOCTEXT("AddTrack", "Add"))
										.ButtonStyle(&FEditorStyle::Get(), "FlatButton.Success")
										.ForegroundColor(FLinearColor::White)
										.IsEnabled_Lambda(IsOpen)
										.OnClicked_Lambda([this]() { AddTrack(); return FReply::Handled(); })
									]
								])
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[
							MakeSection(LOCTEXT("SharingLabel", "Share audio"),
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("Source", "Source"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f)[ MakeTextField(&AudioSourceName, LOCTEXT("SourceHint", "Cue")) ]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("SourceFromSelection", "Use selected"))
											.OnClicked_Lambda([this]() { SetAudioSourceFromSelection(); return FReply::Handled(); })
										])
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									MakeRow(LOCTEXT("NewFromSource", "New cue"),
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(0.55f)[ MakeTextField(&FromSourceName, LOCTEXT("NewFromSourceHint", "Name")) ]
										+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(4, 0, 0, 0)[ MakeCategoryCombo(&FromSourceCategory) ]
										+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
										[
											SNew(SButton).Text(LOCTEXT("AddFromSource", "Add"))
											.ToolTipText(LOCTEXT("AddFromSourceTip", "Create a cue that plays the source's audio."))
											.IsEnabled_Lambda(IsOpen)
											.OnClicked_Lambda([this]() { AddCueFromSource(); return FReply::Handled(); })
										],
										LOCTEXT("NewFromSourceTip", "Reuse audio without storing it twice."))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0).HAlign(HAlign_Right)
								[
									SNew(SButton).Text(LOCTEXT("PointAtSource", "Apply to selected"))
									.ToolTipText(LOCTEXT("PointAtSourceTip", "Selected cues play the source's audio, keeping their own settings."))
									.IsEnabled_Lambda(HasSelection)
									.OnClicked_Lambda([this]() { PointSelectedAtSource(); return FReply::Handled(); })
								],
								true)
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
						[
							MakeSection(LOCTEXT("AwbSection", "AWB"),
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
									.Text(LOCTEXT("AwbHelp", "Saves in the same layout as opened. Merging puts all audio in the .acb."))
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
								[
									SNew(SButton).Text(LOCTEXT("Merge", "Merge into ACB"))
									.IsEnabled_Lambda([this]() { return Session.IsOpen() && Session.Doc().StreamAwb() != nullptr; })
									.OnClicked_Lambda([this]() { MergeNow(); return FReply::Handled(); })
								],
								true)
						]

						+ SVerticalBox::Slot().AutoHeight()
						[
							MakeSection(LOCTEXT("SettingsArea", "Settings"),
								SNew(SBox).MaxDesiredHeight(500.f)
								[
									MakeSettingsView()
								],
								true)
						]
					]
				]
			]

			+ SSplitter::Slot().Value(0.18f)
			[
				SNew(SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.AutoWrapText(true)
				.Text_Lambda([this]() { return FText::FromString(LogText); })
			]
		]
	];

	ApplyPreset(0);
	GetMutableDefault<UAcbEditorSettings>()->OnSettingChanged().AddSP(this, &SAcbEditor::OnSettingsChanged);

	AddLog(TEXT("Open an .acb or cue sheet .uasset to start."));
}

TSharedRef<SWidget> SAcbEditor::MakeRow(const FText& Label, TSharedRef<SWidget> Content, const FText& Tooltip)
{
	return SNew(SHorizontalBox)
		.ToolTipText(Tooltip)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2, 8, 2)
		[
			SNew(SBox).WidthOverride(100.f)
			[
				SNew(STextBlock).Text(Label)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0, 2)
		[
			Content
		];
}

TSharedRef<SWidget> SAcbEditor::MakeSection(const FText& Title, TSharedRef<SWidget> Content, bool bCollapsed)
{
	return SNew(SExpandableArea)
		.InitiallyCollapsed(bCollapsed)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.AreaTitle(Title)
		.AreaTitleFont(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		.Padding(FMargin(8.f, 4.f, 8.f, 8.f))
		.BodyContent()
		[
			Content
		];
}

TSharedRef<SWidget> SAcbEditor::MakeSettingsView()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch = true;
	Args.bHideSelectionTip = true;
	Args.bShowOptions = false;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	TSharedRef<IDetailsView> View = PropertyEditor.CreateDetailView(Args);
	View->SetObject(GetMutableDefault<UAcbEditorSettings>());
	return View;
}

TSharedRef<SWidget> SAcbEditor::MakeTextField(FString* Target, const FText& Hint)
{
	return SNew(SEditableTextBox)
		.HintText(Hint)
		.Text_Lambda([Target]() { return FText::FromString(*Target); })
		.OnTextChanged_Lambda([Target](const FText& T) { *Target = T.ToString(); });
}

TSharedRef<STextComboBox> SAcbEditor::MakeCategoryCombo(FString* Target, TSharedPtr<FAcbCueTemplate> Owner)
{
	TSharedPtr<FString> Initial = CategoryOptions[0];
	for (const TSharedPtr<FString>& Option : CategoryOptions)
	{
		if (*Option == *Target)
		{
			Initial = Option;
		}
	}
	TSharedRef<STextComboBox> Combo = SNew(STextComboBox)
		.OptionsSource(&CategoryOptions)
		.InitiallySelectedItem(Initial)
		.ToolTipText(LOCTEXT("CategoryTip", "Category. Empty = the template cue's."))
		.OnSelectionChanged_Lambda([Target, Owner](TSharedPtr<FString> V, ESelectInfo::Type Info)
		{
			if (Owner.IsValid() && Info == ESelectInfo::Direct)
			{
				return;
			}
			*Target = (V.IsValid() && *V != AcbEditorText::DefaultCategory) ? *V : FString();
		});
	CategoryCombos.Add(Combo);
	return Combo;
}

TSharedRef<STextComboBox> SAcbEditor::MakePlayModeCombo(uint8* Target)
{
	const int32 Index = PlayModeValues.IndexOfByKey(*Target);
	return SNew(STextComboBox)
		.OptionsSource(&PlayModeOptions)
		.InitiallySelectedItem(PlayModeOptions[Index == INDEX_NONE ? 0 : Index])
		.OnSelectionChanged_Lambda([this, Target](TSharedPtr<FString> V, ESelectInfo::Type)
		{
			const int32 Picked = PlayModeOptions.IndexOfByKey(V);
			if (Picked != INDEX_NONE)
			{
				*Target = PlayModeValues[Picked];
			}
		});
}

void SAcbEditor::ApplyPreset(int32 PresetIndex)
{
	CurrentPreset = (PresetIndex >= 1 && PresetIndex - 1 < VisiblePresets.Num()) ? PresetIndex : 0;
	CueTemplates.Reset();
	if (PresetIndex >= 1 && PresetIndex - 1 < VisiblePresets.Num())
	{
		for (const FAcbCueTemplate& Cue : VisiblePresets[PresetIndex - 1].Cues)
		{
			FAcbCueTemplate Row = Cue;
			Row.Pattern = Row.Pattern.Replace(TEXT("{name}"), TEXT(""), ESearchCase::IgnoreCase);
			CueTemplates.Add(MakeShared<FAcbCueTemplate>(Row));
		}
	}
	if (CueTemplates.Num() == 0)
	{
		CueTemplates.Add(MakeShared<FAcbCueTemplate>());
	}
	RebuildCueRows();
	FindTableRows();
}

void SAcbEditor::ResetCueNames()
{
	const TArray<FAcbCueTemplate>* Preset = CurrentPreset >= 1 && CurrentPreset - 1 < VisiblePresets.Num() ? &VisiblePresets[CurrentPreset - 1].Cues : nullptr;
	for (int32 Index = 0; Index < CueTemplates.Num(); ++Index)
	{
		CueTemplates[Index]->Pattern = Preset && Preset->IsValidIndex(Index)
			? (*Preset)[Index].Pattern.Replace(TEXT("{name}"), TEXT(""), ESearchCase::IgnoreCase)
			: FString();
	}
}

void SAcbEditor::RefreshPresets()
{
	const FString Selected = PresetOptions.IsValidIndex(CurrentPreset) ? *PresetOptions[CurrentPreset] : FString();
	VisiblePresets = GetDefault<UAcbEditorSettings>()->GetPresetsFor(Session.IsOpen() ? Session.GetCueSheetName() : FString());
	PresetOptions.Reset();
	PresetOptions.Add(MakeShared<FString>(TEXT("(single cue)")));
	for (const FAcbCuePreset& Preset : VisiblePresets)
	{
		PresetOptions.Add(MakeShared<FString>(Preset.Name));
	}
	const int32 Same = PresetOptions.IndexOfByPredicate([&Selected](const TSharedPtr<FString>& Option) { return *Option == Selected; });
	if (PresetCombo.IsValid())
	{
		TGuardValue<bool> Guard(bRefreshingPresets, true);
		PresetCombo->RefreshOptions();
		PresetCombo->SetSelectedItem(PresetOptions[Same == INDEX_NONE ? 0 : Same]);
	}
	if (Same == INDEX_NONE)
	{
		ApplyPreset(0);
	}
	else
	{
		CurrentPreset = Same;
	}
}

void SAcbEditor::RequestCueRowsRebuild()
{
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		RebuildCueRows();
		return EActiveTimerReturnType::Stop;
	}));
}

void SAcbEditor::RebuildCueRows()
{
	if (!CueRowsBox.IsValid())
	{
		return;
	}
	CueRowsBox->ClearChildren();
	for (const TSharedPtr<FAcbCueTemplate>& Template : CueTemplates)
	{
		CueRowsBox->AddSlot().AutoHeight().Padding(0, 1)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.44f)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("PatternHint", "cue name (empty = file name)"))
				.ToolTipText(LOCTEXT("PatternTip", "Cue name, e.g. mus_ent_AdamPage. Empty or ending in _ adds the audio file name."))
				.Text_Lambda([Template]() { return FText::FromString(Template->Pattern); })
				.OnTextChanged_Lambda([Template](const FText& T) { Template->Pattern = T.ToString(); })
			]
			+ SHorizontalBox::Slot().FillWidth(0.28f).Padding(4, 0, 0, 0)
			[
				MakeCategoryCombo(&Template->Category, Template)
			]
			+ SHorizontalBox::Slot().FillWidth(0.28f).Padding(4, 0, 0, 0)
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("RowTemplateHint", "copy from"))
				.ToolTipText(LOCTEXT("RowTemplateTip", "Cue to copy settings from."))
				.Text_Lambda([Template]() { return FText::FromString(Template->Template); })
				.OnTextChanged_Lambda([Template](const FText& T) { Template->Template = T.ToString(); })
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type) { FindTableRows(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
			[
				SNew(SButton).Text(LOCTEXT("RemoveCueRow", "x"))
				.ToolTipText(LOCTEXT("RemoveCueRowTip", "Remove"))
				.OnClicked_Lambda([this, Template]()
				{
					CueTemplates.Remove(Template);
					if (CueTemplates.Num() == 0)
					{
						CueTemplates.Add(MakeShared<FAcbCueTemplate>());
					}
					RequestCueRowsRebuild();
					return FReply::Handled();
				})
			]
		];
	}
}

int32 SAcbEditor::GetTemplateCueIndex() const
{
	return TemplateCueName.IsEmpty() ? INDEX_NONE : Session.FindCue(TemplateCueName.TrimStartAndEnd());
}

TArray<FString> SAcbEditor::GetTemplateCueNames() const
{
	TArray<FString> Names;
	for (const TSharedPtr<FAcbCueTemplate>& Row : CueTemplates)
	{
		FString Name = Row->Template.TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			Name = TemplateCueName.TrimStartAndEnd();
		}
		if (!Name.IsEmpty())
		{
			Names.AddUnique(Name);
		}
	}
	return Names;
}

void SAcbEditor::FindTableRows()
{
	TableCandidates.Reset();
	if (Session.IsOpen() && GetDefault<UAcbEditorSettings>()->bUpdateDataTables)
	{
		TArray<FString> Log;
		for (AcbTableRows::FPlannedRow& Row : AcbTableRows::Find(GetTemplateCueNames(), Log))
		{
			TableCandidates.Add(MakeShared<AcbTableRows::FPlannedRow>(MoveTemp(Row)));
		}
		for (const FString& Line : Log)
		{
			AddLog(Line);
		}
	}
	RebuildTableRows();
}

void SAcbEditor::RebuildTableRows()
{
	if (!TableRowsBox.IsValid())
	{
		return;
	}
	TableRowsBox->ClearChildren();
	for (const TSharedPtr<AcbTableRows::FPlannedRow>& Candidate : TableCandidates)
	{
		const bool bHasNewTable = Candidate->NewTable.IsValid();
		const FString NewTableName = bHasNewTable ? Candidate->NewTable.GetAssetName() : FString(TEXT("no New Table set"));
		TSharedRef<SVerticalBox> Fields = SNew(SVerticalBox);
		Fields->AddSlot().AutoHeight()
		[
			MakeRow(LOCTEXT("TableRowName", "Row name"),
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("TableRowNameHint", "automatic"))
				.Text_Lambda([Candidate]() { return FText::FromString(Candidate->RowName); })
				.OnTextChanged_Lambda([Candidate](const FText& T) { Candidate->RowName = T.ToString(); }),
				LOCTEXT("TableRowNameTip", "Empty = the next free number in the table's ID range, or the copied row's name with the new name in it."))
		];
		for (const AcbTableRows::FFieldInfo& Field : Candidate->Fields)
		{
			const FName Key = Field.Field;
			Fields->AddSlot().AutoHeight()
			[
				MakeRow(FText::FromString(Field.Label),
					SNew(SEditableTextBox)
					.HintText(FText::Format(LOCTEXT("TableFieldHint", "auto (copied: {0})"), FText::FromString(Field.TemplateValue.IsEmpty() ? TEXT("empty") : Field.TemplateValue)))
					.Text_Lambda([Candidate, Key]()
					{
						const FString* Value = Candidate->Values.Find(Key);
						return FText::FromString(Value ? *Value : FString());
					})
					.OnTextChanged_Lambda([Candidate, Key](const FText& T) { Candidate->Values.Add(Key, T.ToString()); }),
					LOCTEXT("TableFieldTip", "Empty = the copied value with the template's name part swapped for the new one (texts with no name part get the new name)."))
			];
		}
		if (!Candidate->UnlockTemplateRow.IsNone())
		{
			Fields->AddSlot().AutoHeight()
			[
				MakeRow(LOCTEXT("TableUnlock", "Unlock item"),
					SNew(SCheckBox)
					.IsEnabled(Candidate->UnlockNewTable.IsValid())
					.IsChecked_Lambda([Candidate]() { return Candidate->bAddUnlock ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([Candidate](ECheckBoxState S) { Candidate->bAddUnlock = S == ECheckBoxState::Checked; })
					[
						SNew(STextBlock).Text(Candidate->UnlockNewTable.IsValid()
							? FText::Format(LOCTEXT("TableUnlockLabel", "Add one (copy of {0}) into {1}"), FText::FromName(Candidate->UnlockTemplateRow), FText::FromString(Candidate->UnlockNewTable.GetAssetName()))
							: FText::Format(LOCTEXT("TableUnlockNone", "Shares {0} (no Unlock New Table set)"), FText::FromName(Candidate->UnlockTemplateRow)))
					],
					LOCTEXT("TableUnlockTip", "Copy the template row's unlock item into the Cue Table's Unlock New Table (Settings > Tables) with a free unlock ID. Off = the new row shares the template's unlock item."))
			];
		}

		TableRowsBox->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(4.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SCheckBox)
					.IsEnabled(bHasNewTable)
					.IsChecked_Lambda([Candidate]() { return Candidate->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([Candidate](ECheckBoxState S) { Candidate->bEnabled = S == ECheckBoxState::Checked; })
					[
						SNew(STextBlock).Text(FText::Format(LOCTEXT("TableRowLabel", "{0}: copy row {1} into {2}"),
							FText::FromString(Candidate->TableName), FText::FromName(Candidate->TemplateRow), FText::FromString(NewTableName)))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Visibility(Candidate->CoveredCueFields < Candidate->CueFields ? EVisibility::Visible : EVisibility::Collapsed)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text(FText::Format(LOCTEXT("TablePartial", "The row uses {0} cues and this add replaces {1}. The others switch to the new name's cues if they exist, else stay empty."),
						FText::AsNumber(Candidate->CueFields), FText::AsNumber(Candidate->CoveredCueFields)))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(true)
					.AreaTitle(LOCTEXT("TableValues", "Values"))
					.IsEnabled_Lambda([Candidate]() { return Candidate->bEnabled; })
					.BodyContent()
					[
						Fields
					]
				]
			]
		];
	}
}

bool SAcbEditor::ConfirmDiscard()
{
	if (!Session.IsOpen() || !Session.Doc().IsModified())
	{
		return true;
	}
	return FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Discard", "Discard unsaved changes?")) == EAppReturnType::Yes;
}

bool SAcbEditor::PickFiles(const FString& Title, const FString& Types, bool bMultiple, const FString& DefaultFolder, TArray<FString>& OutFiles)
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		return false;
	}
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	return Desktop->OpenFileDialog(Parent, Title, DefaultFolder, FString(), Types,
		bMultiple ? EFileDialogFlags::Multiple : EFileDialogFlags::None, OutFiles);
}

bool SAcbEditor::PickFolder(const FString& Title, const FString& DefaultFolder, FString& OutFolder)
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		return false;
	}
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	return Desktop->OpenDirectoryDialog(Parent, Title, DefaultFolder, OutFolder);
}

void SAcbEditor::RememberFolder(const FString& Path)
{
	UAcbEditorSettings* Settings = GetMutableDefault<UAcbEditorSettings>();
	Settings->LastAudioFolder = FPaths::GetPath(Path);
	Settings->SaveConfig();
}

void SAcbEditor::OpenFile(const FString& AcbPath, const FString& AwbPath, const FString& CueSheetFolder)
{
	if (!ConfirmDiscard())
	{
		return;
	}
	StopPreview();
	ClearPending();
	if (!(Session.IsOpen() && FPaths::IsSamePath(Session.GetAcbPath(), AcbPath)) || !CueSheetFolder.IsEmpty())
	{
		ImportFolder = CueSheetFolder;
	}
	FString Awb = AwbPath.IsEmpty() ? FAcbEditorSession::FindSiblingAwb(AcbPath) : AwbPath;
	FString Error;
	if (!Session.Open(AcbPath, Awb, Error))
	{
		AddLog(TEXT("ERROR: ") + Error);
		Refresh();
		return;
	}
	if (Awb.IsEmpty() && Session.Doc().UsesStreamingAwb())
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("PickAwb",
			"This ACB streams audio from an .awb, but none was found next to it. Pick the .awb now?\n\n"
			"Without it you can still edit cues and add audio embedded in the ACB, but not merge or add streaming audio.")) == EAppReturnType::Yes)
		{
			TArray<FString> Files;
			if (PickFiles(TEXT("Streaming AWB"), TEXT("AWB (*.awb)|*.awb"), false, FPaths::GetPath(AcbPath), Files) && Files.Num() > 0)
			{
				Awb = Files[0];
				if (!Session.Open(AcbPath, Awb, Error))
				{
					AddLog(TEXT("ERROR: ") + Error);
				}
			}
		}
	}
	AddLog(TEXT("Opened ") + AcbPath + (Session.GetAwbPath().IsEmpty() ? FString() : TEXT(" + ") + Session.GetAwbPath()));
	TemplateCueName.Reset();
	AudioSourceName.Reset();
	RefreshPresets();
	RefreshCategories();
	Refresh();
}

void SAcbEditor::OnOpenClicked()
{
	const FString Current = Session.IsOpen() ? FPaths::GetPath(Session.GetAcbPath()) : GetDefault<UAcbEditorSettings>()->LastAcbFolder;
	TArray<FString> Files;
	if (PickFiles(TEXT("Open cue sheet"), TEXT("Cue sheet (*.acb;*.uasset;*.uexp)|*.acb;*.uasset;*.uexp"), false, Current, Files) && Files.Num() > 0)
	{
		UAcbEditorSettings* Settings = GetMutableDefault<UAcbEditorSettings>();
		Settings->LastAcbFolder = FPaths::GetPath(Files[0]);
		Settings->SaveConfig();
		OpenFile(Files[0]);
	}
}

void SAcbEditor::OnReloadClicked()
{
	if (Session.IsOpen())
	{
		const FString Acb = Session.GetAcbPath();
		const FString Awb = Session.GetAwbPath();
		OpenFile(Acb, Awb);
	}
}

void SAcbEditor::OpenSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Plugins", "AEWAcbEditor");
	}
}

void SAcbEditor::OpenForImportFolder(const FString& ContentFolder)
{
	if (Session.IsOpen() && FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("UseOpenSheet",
		"Import the open cue sheet ({0}) into {1}?\n\nNo: open another .acb to import there."),
		FText::FromString(Session.GetCueSheetName()), FText::FromString(ContentFolder))) == EAppReturnType::Yes)
	{
		SetImportFolder(ContentFolder);
		return;
	}
	const FString Current = Session.IsOpen() ? FPaths::GetPath(Session.GetAcbPath()) : GetDefault<UAcbEditorSettings>()->LastAcbFolder;
	TArray<FString> Files;
	if (PickFiles(TEXT("Open cue sheet to import into ") + ContentFolder, TEXT("Cue sheet (*.acb;*.uasset;*.uexp)|*.acb;*.uasset;*.uexp"), false, Current, Files) && Files.Num() > 0)
	{
		UAcbEditorSettings* Settings = GetMutableDefault<UAcbEditorSettings>();
		Settings->LastAcbFolder = FPaths::GetPath(Files[0]);
		Settings->SaveConfig();
		OpenFile(Files[0], FString(), ContentFolder);
	}
}

void SAcbEditor::SetImportFolder(const FString& ContentFolder)
{
	ImportFolder = ContentFolder;
	AddLog(TEXT("Import folder: ") + ContentFolder);
}

bool SAcbEditor::PickCueSheetFolder(FString& OutFolder)
{
	FString Default = ImportFolder;
	if (Default.IsEmpty())
	{
		Default = AcbImportBridge::FindCueSheetFolder(Session.GetCueSheetName());
	}
	if (Default.IsEmpty())
	{
		Default = GetDefault<UAcbEditorSettings>()->LastCueSheetFolder;
	}
	if (Default.IsEmpty())
	{
		Default = TEXT("/Game");
	}
	TSharedRef<SDlgPickPath> Dialog = SNew(SDlgPickPath)
		.Title(FText::Format(LOCTEXT("PickCueSheetFolder", "Import {0} into folder"), FText::FromString(Session.GetCueSheetName())))
		.DefaultPath(FText::FromString(Default));
	if (Dialog->ShowModal() != EAppReturnType::Ok)
	{
		return false;
	}
	OutFolder = Dialog->GetPath().ToString();
	UAcbEditorSettings* Settings = GetMutableDefault<UAcbEditorSettings>();
	Settings->LastCueSheetFolder = OutFolder;
	Settings->SaveConfig();
	ImportFolder = OutFolder;
	return true;
}

void SAcbEditor::OnSaveClicked(bool bImport)
{
	if (!Session.IsOpen())
	{
		return;
	}
	StopPreview();
	const UAcbEditorSettings* Settings = GetDefault<UAcbEditorSettings>();
	const uint64 Estimate = Session.EstimateOutputAcbSize(*Settings);
	FString CueSheetFolder;
	if (bImport && Estimate > FAcbEditorSession::CriImportLimit)
	{
		const FText Message = FText::Format(LOCTEXT("TooBig",
			"The saved .acb will be about {0} GiB, but the CriWare importer can only read files under 2 GiB.\n\n"
			"Save without importing?"),
			FText::AsNumber(double(Estimate) / double(1ull << 30)));
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message) != EAppReturnType::Yes)
		{
			return;
		}
		bImport = false;
	}
	if (bImport)
	{
		if (ImportFolder.IsEmpty() && !PickCueSheetFolder(ImportFolder))
		{
			AddLog(TEXT("Import cancelled."));
			return;
		}
		CueSheetFolder = ImportFolder;
		AddLog(TEXT("Importing into ") + CueSheetFolder);
	}

	AddLog(TEXT("---- save"));
	const bool bStreamsWithoutAwb = Session.Doc().UsesStreamingAwb() && Session.Doc().StreamAwb() == nullptr;
	const FAcbSaveResult Result = Session.Save(*Settings);
	if (!Result.bOk)
	{
		AddLog(TEXT("ERROR: ") + Result.Error);
		Refresh();
		return;
	}
	if (!Result.Cleanup.IsEmpty())
	{
		AddLog(Result.Cleanup);
	}
	AddLog(TEXT("Saved ") + Result.AcbPath + (Result.AwbPath.IsEmpty() ? FString() : TEXT(" + ") + Result.AwbPath));
	if (bStreamsWithoutAwb)
	{
		AddLog(TEXT("warning: this cue sheet streams from an .awb that wasn't opened, so none was written or copied. ")
			TEXT("Open it together with its .awb (same name, same folder) to save and import the pair."));
	}
	Refresh();

	bool bAwbCopied = false;
	if (!bImport && !Result.AwbPath.IsEmpty() && Settings->bCopyAwbToProject)
	{
		TArray<FString> CopyLog;
		bAwbCopied = AcbImportBridge::CopyAwbToProject(Result.AwbPath, Settings->AwbDirectory, CopyLog);
		for (const FString& Line : CopyLog)
		{
			AddLog(Line);
		}
	}

	if (bImport)
	{
		AcbImportBridge::FRequest Request;
		Request.AcbFile = Result.AcbPath;
		Request.PackagePath = CueSheetFolder;
		Request.bHasStreamingAwb = !Result.AwbPath.IsEmpty();
		Request.AwbDirectory = Settings->AwbDirectory;
		Request.bStageAwbForPackaging = Settings->bAddAwbFolderToPackaging;
		Request.bSaveAssets = Settings->bSaveAssetsAfterImport;
		Request.bAwbAlreadyCopied = bAwbCopied;
		Request.bAssignSheetChunkToCues = Settings->bAssignSheetChunkToCues;
		Request.bUpdateDataTables = Settings->bUpdateDataTables;
		Request.bDeleteRemovedCueAssets = Settings->bDeleteRemovedCueAssets;
		Request.bDeleteRowsOfDeletedCues = Settings->bDeleteRowsOfDeletedCues;
		Request.RenamedCues = RenamedCues;
		Request.DeletedCues = DeletedCues;
		Request.TableRows = PendingRows;
		for (const TSharedPtr<FAcbCueRow>& Row : AllRows)
		{
			Request.CueNames.Add(Row->Name);
		}
		TArray<FString> ImportLog;
		FString Error;
		const bool bImported = AcbImportBridge::Import(Request, ImportLog, Error);
		for (const FString& Line : ImportLog)
		{
			AddLog(Line);
		}
		AddLog(bImported ? FString(TEXT("Import done.")) : FString(TEXT("ERROR: ")) + Error);
		if (bImported && Settings->bUpdateDataTables)
		{
			ClearPending();
		}
	}
}

bool SAcbEditor::Report(bool bOk, const FString& Message)
{
	AddLog(bOk ? Message : TEXT("ERROR: ") + Message);
	if (bOk)
	{
		TimingCueId = MAX_uint32;
	}
	Refresh();
	RefreshSlots();
	return bOk;
}

void SAcbEditor::AddTrack()
{
	if (NewAudioFile.IsEmpty())
	{
		AddLog(TEXT("ERROR: pick the audio file first."));
		return;
	}
	const int32 Template = GetTemplateCueIndex();
	if (!TemplateCueName.IsEmpty() && Template == INDEX_NONE)
	{
		AddLog(TEXT("ERROR: no cue named '") + TemplateCueName + TEXT("' (Copy from)."));
		return;
	}
	TArray<FAcbCueTemplate> Cues;
	for (const TSharedPtr<FAcbCueTemplate>& Row : CueTemplates)
	{
		Cues.Add(*Row);
	}
	FString Message;
	TArray<FString> Names;
	if (Report(Session.AddTrack(NewAudioFile, Cues, Template, bLoopNewTrack, Message, &Names), Message))
	{
		TMap<FString, FString> CueMap;
		for (int32 Index = 0; Index < CueTemplates.Num() && Index < Names.Num(); ++Index)
		{
			FString From = CueTemplates[Index]->Template.TrimStartAndEnd();
			if (From.IsEmpty())
			{
				From = TemplateCueName.TrimStartAndEnd();
			}
			if (!From.IsEmpty())
			{
				CueMap.Add(From, Names[Index]);
			}
		}
		for (const TSharedPtr<AcbTableRows::FPlannedRow>& Candidate : TableCandidates)
		{
			if (!Candidate->bEnabled || !Candidate->NewTable.IsValid())
			{
				continue;
			}
			AcbTableRows::FPlannedRow Row = *Candidate;
			AcbTableRows::SetCues(Row, CueMap);
			AcbTableRows::Merge(PendingRows, Row);
			AddLog(FString::Printf(TEXT("%s: a copy of row %s (%s -> %s) is added on Save."), *Row.TableName, *Row.TemplateRow.ToString(), *Row.From, *Row.To));
		}
		for (const TSharedPtr<AcbTableRows::FPlannedRow>& Candidate : TableCandidates)
		{
			Candidate->Values.Reset();
			Candidate->RowName.Reset();
		}
		NewAudioFile.Reset();
		ResetCueNames();
	}
}

void SAcbEditor::RenameSelected()
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (!Row.IsValid() || RenameTo.IsEmpty())
	{
		return;
	}
	const std::string NewName = FAcbEditorSession::ToUtf8(RenameTo.TrimStartAndEnd());
	const size_t Index = size_t(Row->Index);
	FString Error;
	const bool bOk = Session.Edit([&](AcbCore::AcbDocument& Doc) { Doc.RenameCue(Index, NewName); }, Error);
	if (bOk)
	{
		NoteRename(Row->Name, RenameTo.TrimStartAndEnd());
	}
	Report(bOk, bOk ? FString::Printf(TEXT("Renamed %s -> %s"), *Row->Name, *RenameTo) : Error);
}

void SAcbEditor::NoteRename(const FString& OldName, const FString& NewName)
{
	FString Original = OldName;
	for (const TPair<FString, FString>& Pair : RenamedCues)
	{
		if (Pair.Value == OldName)
		{
			Original = Pair.Key;
		}
	}
	RenamedCues.Remove(Original);
	if (Original != NewName)
	{
		RenamedCues.Add(Original, NewName);
	}
	AcbTableRows::RenameCue(PendingRows, OldName, NewName);
}

void SAcbEditor::NoteDelete(const FString& Name)
{
	FString Original = Name;
	for (const TPair<FString, FString>& Pair : RenamedCues)
	{
		if (Pair.Value == Name)
		{
			Original = Pair.Key;
		}
	}
	RenamedCues.Remove(Original);
	DeletedCues.AddUnique(Original);
	AcbTableRows::RenameCue(PendingRows, Name, FString());
}

void SAcbEditor::ClearPending()
{
	RenamedCues.Reset();
	DeletedCues.Reset();
	PendingRows.Reset();
}

FText SAcbEditor::GetPendingText() const
{
	TArray<FString> Parts;
	if (PendingRows.Num() > 0) Parts.Add(FString::Printf(TEXT("%d DataTable row%s"), PendingRows.Num(), PendingRows.Num() == 1 ? TEXT("") : TEXT("s")));
	if (RenamedCues.Num() > 0) Parts.Add(FString::Printf(TEXT("%d rename%s"), RenamedCues.Num(), RenamedCues.Num() == 1 ? TEXT("") : TEXT("s")));
	if (DeletedCues.Num() > 0) Parts.Add(FString::Printf(TEXT("%d deletion%s"), DeletedCues.Num(), DeletedCues.Num() == 1 ? TEXT("") : TEXT("s")));
	return Parts.Num() == 0 || !GetDefault<UAcbEditorSettings>()->bUpdateDataTables
		? FText::GetEmpty()
		: FText::FromString(TEXT("On Save: ") + FString::Join(Parts, TEXT(", ")));
}

void SAcbEditor::SetCategoryOfSelected()
{
	const TArray<TSharedPtr<FAcbCueRow>> Selection = GetSelection();
	if (Selection.Num() == 0)
	{
		return;
	}
	if (SelectedCategory.IsEmpty())
	{
		AddLog(TEXT("ERROR: pick a category."));
		return;
	}
	const std::string Category = FAcbEditorSession::ToUtf8(SelectedCategory);
	TArray<size_t> Indices;
	for (const TSharedPtr<FAcbCueRow>& Row : Selection)
	{
		Indices.Add(size_t(Row->Index));
	}
	FString Error;
	const bool bOk = Session.Edit([&](AcbCore::AcbDocument& Doc)
	{
		for (size_t Index : Indices)
		{
			Doc.SetCueCategory(Index, Category);
		}
	}, Error);
	Report(bOk, bOk ? FString::Printf(TEXT("Category of %d cue(s): %s"), Selection.Num(), *SelectedCategory) : Error);
}

namespace AcbVolume
{
	constexpr float SliderMax = 4.f;
	constexpr float Max = 655.35f;

	uint16_t ToRaw(float Value)
	{
		return uint16_t(FMath::Clamp(FMath::RoundToInt(Value * 100.f), 0, 65535));
	}

	FString Format(const std::optional<uint16_t>& Volume)
	{
		return Volume ? FString::Printf(TEXT("%.2f"), double(*Volume) / 100.0) : FString(TEXT("none"));
	}
}

TSharedRef<SWidget> SAcbEditor::MakeVolumeSlider(bool bSlot)
{
	auto HasSelection = [this, bSlot]() { return GetSelection().Num() > 0 && (!bSlot || (GetSelection().Num() == 1 && SlotOptions.Num() > 0)); };
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(SSpinBox<float>)
			.MinValue(0.f)
			.MaxValue(AcbVolume::Max)
			.MinSliderValue(0.f)
			.MaxSliderValue(AcbVolume::SliderMax)
			.Delta(0.01f)
			.MinFractionalDigits(2)
			.MaxFractionalDigits(2)
			.IsEnabled_Lambda(HasSelection)
			.Value_Lambda([this, bSlot]() { return bSlot ? SlotVolume : CueVolume; })
			.OnValueChanged_Lambda([this, bSlot](float V)
			{
				(bSlot ? SlotVolume : CueVolume) = V;
				UpdatePreviewGain();
			})
			.OnValueCommitted_Lambda([this, bSlot](float V, ETextCommit::Type)
			{
				(bSlot ? SlotVolume : CueVolume) = V;
				CommitVolume(bSlot);
			})
			.OnEndSliderMovement_Lambda([this, bSlot](float V)
			{
				(bSlot ? SlotVolume : CueVolume) = V;
				CommitVolume(bSlot);
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text_Lambda([this, bSlot]() { return (bSlot ? bHasSlotVolume : bHasCueVolume) ? FText::GetEmpty() : LOCTEXT("VolumeNone", "(none)"); })
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
		[
			SNew(SButton).Text(LOCTEXT("ClearVolume", "x"))
			.ToolTipText(bSlot ? LOCTEXT("ClearSlotVolumeTip", "Remove this track's volume.") : LOCTEXT("ClearCueVolumeTip", "Remove the selected cues' volume."))
			.IsEnabled_Lambda([this, bSlot, HasSelection]() { return HasSelection() && (bSlot ? bHasSlotVolume : bHasCueVolume); })
			.OnClicked_Lambda([this, bSlot]() { SetVolume(bSlot, TOptional<float>()); return FReply::Handled(); })
		];
}

void SAcbEditor::CommitVolume(bool bSlot)
{
	const float Value = bSlot ? SlotVolume : CueVolume;
	const bool bHas = bSlot ? bHasSlotVolume : bHasCueVolume;
	const double Now = FPlatformTime::Seconds();
	const uint32 Key = (bSlot ? 0x80000000u : 0u) | AcbVolume::ToRaw(Value);
	if (Key == LastVolumeCommit && Now - LastVolumeCommitTime < 0.5)
	{
		return;
	}
	LastVolumeCommit = Key;
	LastVolumeCommitTime = Now;
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (!Row.IsValid() || !Session.IsOpen())
	{
		return;
	}
	if (GetSelection().Num() == 1)
	{
		try
		{
			const AcbCore::AcbDocument& Doc = Session.Doc();
			const std::optional<uint16_t> Current = bSlot ? Doc.SlotVolume(size_t(Row->Index), size_t(SelectedSlot)) : Doc.CueVolume(size_t(Row->Index));
			if (Current ? *Current == AcbVolume::ToRaw(Value) : (!bHas && AcbVolume::ToRaw(Value) == 100))
			{
				return;
			}
		}
		catch (const std::exception&)
		{
		}
	}
	SetVolume(bSlot, Value);
}

void SAcbEditor::SetVolume(bool bSlot, TOptional<float> Value)
{
	const TArray<TSharedPtr<FAcbCueRow>> Selection = GetSelection();
	if (Selection.Num() == 0)
	{
		return;
	}
	if (bSlot && Selection.Num() != 1)
	{
		AddLog(TEXT("ERROR: select one cue to set a track's volume."));
		return;
	}
	std::optional<uint16_t> Volume;
	if (Value.IsSet())
	{
		Volume = AcbVolume::ToRaw(Value.GetValue());
	}
	TArray<size_t> Indices;
	for (const TSharedPtr<FAcbCueRow>& Row : Selection)
	{
		Indices.Add(size_t(Row->Index));
	}
	const size_t Slot = size_t(SelectedSlot);
	FString Error;
	const bool bOk = Session.Edit([&](AcbCore::AcbDocument& Doc)
	{
		for (size_t Index : Indices)
		{
			if (bSlot)
			{
				Doc.SetSlotVolume(Index, Slot, Volume);
			}
			else
			{
				Doc.SetCueVolume(Index, Volume);
			}
		}
	}, Error);
	const FString Shown = AcbVolume::Format(Volume);
	Report(bOk, bOk
		? (bSlot ? FString::Printf(TEXT("Volume of %s, track %d: %s"), *Selection[0]->Name, SelectedSlot, *Shown)
			: FString::Printf(TEXT("Volume of %d cue(s): %s"), Selection.Num(), *Shown))
		: Error);
	LoadVolumes();
	UpdatePreviewGain();
}

void SAcbEditor::LoadVolumes()
{
	CueVolume = 1.f;
	SlotVolume = 1.f;
	bHasCueVolume = false;
	bHasSlotVolume = false;
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (!Row.IsValid() || !Session.IsOpen())
	{
		return;
	}
	const AcbCore::AcbDocument& Doc = Session.Doc();
	if (size_t(Row->Index) >= Doc.CueCount())
	{
		return;
	}
	try
	{
		if (const std::optional<uint16_t> Cue = Doc.CueVolume(size_t(Row->Index)))
		{
			CueVolume = float(*Cue) / 100.f;
			bHasCueVolume = true;
		}
		if (SlotOptions.Num() > 0)
		{
			if (const std::optional<uint16_t> Track = Doc.SlotVolume(size_t(Row->Index), size_t(SelectedSlot)))
			{
				SlotVolume = float(*Track) / 100.f;
				bHasSlotVolume = true;
			}
		}
	}
	catch (const std::exception&)
	{
	}
}

void SAcbEditor::UpdatePreviewGain()
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (Preview.IsValid() && Preview->IsPlaying() && Row.IsValid() && Row->Index == PreviewCueIndex && SelectedSlot == PreviewSlot)
	{
		Preview->SetGain(CueVolume * SlotVolume);
	}
}

float SAcbEditor::GetPreviewGain(int32 CueIndex, int32 Slot) const
{
	try
	{
		const AcbCore::AcbDocument& Doc = Session.Doc();
		const std::optional<uint16_t> Cue = Doc.CueVolume(size_t(CueIndex));
		const std::optional<uint16_t> Track = Doc.SlotVolume(size_t(CueIndex), size_t(Slot));
		return (Cue ? float(*Cue) / 100.f : 1.f) * (Track ? float(*Track) / 100.f : 1.f);
	}
	catch (const std::exception&)
	{
		return 1.f;
	}
}

void SAcbEditor::CopySelected()
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (!Row.IsValid())
	{
		return;
	}
	const std::string NewName = FAcbEditorSession::ToUtf8(CopyTo.TrimStartAndEnd());
	const size_t Index = size_t(Row->Index);
	uint32_t NewId = 0;
	FString Error;
	const bool bOk = Session.Edit([&](AcbCore::AcbDocument& Doc) { NewId = Doc.CopyCue(Index, NewName); }, Error);
	Report(bOk, bOk ? FString::Printf(TEXT("Copied %s (new cue id %u)"), *Row->Name, NewId) : Error);
}

void SAcbEditor::DeleteSelected()
{
	const TArray<TSharedPtr<FAcbCueRow>> Selection = GetSelection();
	if (Selection.Num() == 0)
	{
		return;
	}
	const FText Message = FText::Format(LOCTEXT("ConfirmDelete",
		"Delete {0} cue(s)? Audio no other cue uses is removed on save (Remove unused audio). When you Save, their cue assets are deleted, and DataTable rows left with no cues are removed."), Selection.Num());
	if (FMessageDialog::Open(EAppMsgType::YesNo, Message) != EAppReturnType::Yes)
	{
		return;
	}
	std::vector<size_t> Indices;
	TArray<FString> Names;
	for (const TSharedPtr<FAcbCueRow>& Row : Selection)
	{
		Indices.push_back(size_t(Row->Index));
		Names.Add(Row->Name);
	}
	FString Error;
	const bool bOk = Session.Edit([&](AcbCore::AcbDocument& Doc) { Doc.DeleteCues(Indices); }, Error);
	if (bOk)
	{
		for (const FString& Name : Names)
		{
			NoteDelete(Name);
		}
	}
	Report(bOk, bOk ? TEXT("Deleted ") + FString::Join(Names, TEXT(", ")) : Error);
}

void SAcbEditor::SetAudioSourceFromSelection()
{
	if (const TSharedPtr<FAcbCueRow> Row = GetFirstSelected())
	{
		AudioSourceName = Row->Name;
		if (FromSourceName.IsEmpty())
		{
			FromSourceName = Row->Name + TEXT("_2");
		}
	}
}

void SAcbEditor::AddCueFromSource()
{
	const int32 Source = Session.FindCue(AudioSourceName.TrimStartAndEnd());
	if (Source == INDEX_NONE)
	{
		AddLog(TEXT("ERROR: set the audio source to an existing cue first."));
		return;
	}
	FString Message;
	if (Report(Session.AddCueFromAudio(Source, FromSourceName, FromSourceCategory, Message), Message))
	{
		FromSourceName.Reset();
	}
}

void SAcbEditor::PointSelectedAtSource()
{
	const int32 Source = Session.FindCue(AudioSourceName.TrimStartAndEnd());
	if (Source == INDEX_NONE)
	{
		AddLog(TEXT("ERROR: set the audio source to an existing cue first."));
		return;
	}
	TArray<int32> Targets;
	for (const TSharedPtr<FAcbCueRow>& Row : GetSelection())
	{
		Targets.Add(Row->Index);
	}
	FString Message;
	Report(Session.PointCuesAtAudio(Targets, Source, Message), Message);
}

void SAcbEditor::AddVariations()
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	TArray<FString> Files;
	if (!Row.IsValid() || !PickFiles(TEXT("Track audio"), AcbEditorText::AudioFilter, true,
		GetDefault<UAcbEditorSettings>()->LastAudioFolder, Files) || Files.Num() == 0)
	{
		return;
	}
	RememberFolder(Files[0]);
	const int32 PlayMode = Row->NumWaveforms <= 1 ? int32(SelectedPlayMode) : INDEX_NONE;
	FString Message;
	Report(Session.AddVariations(Row->Index, Files, PlayMode, Message), Message);
}

void SAcbEditor::ReplaceVariation()
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	TArray<FString> Files;
	if (!Row.IsValid() || !PickFiles(TEXT("Replacement audio"), AcbEditorText::AudioFilter, false,
		GetDefault<UAcbEditorSettings>()->LastAudioFolder, Files) || Files.Num() == 0)
	{
		return;
	}
	RememberFolder(Files[0]);
	FString Message;
	Report(Session.ReplaceVariation(Row->Index, SelectedSlot, Files[0], Message), Message);
}

void SAcbEditor::RemoveVariation()
{
	if (const TSharedPtr<FAcbCueRow> Row = GetFirstSelected())
	{
		FString Message;
		Report(Session.RemoveVariation(Row->Index, SelectedSlot, Message), Message);
	}
}

void SAcbEditor::ApplyPlayMode()
{
	if (const TSharedPtr<FAcbCueRow> Row = GetFirstSelected())
	{
		FString Message;
		Report(Session.SetPlayMode(Row->Index, SelectedPlayMode, Message), Message);
	}
}

void SAcbEditor::MergeNow()
{
	FString Error;
	const bool bOk = Session.Edit([](AcbCore::AcbDocument& Doc) { Doc.MergeAwbIntoAcb(); }, Error);
	Report(bOk, bOk ? FString(TEXT("Merged the streaming AWB into the ACB (Save to write it).")) : Error);
}

void SAcbEditor::RefreshCategories()
{
	CategoryOptions.Reset();
	CategoryOptions.Add(MakeShared<FString>(AcbEditorText::DefaultCategory));
	for (const FString& Category : Session.GetCategories())
	{
		CategoryOptions.Add(MakeShared<FString>(Category));
	}
	FromSourceCategory.Reset();
	for (const TWeakPtr<STextComboBox>& Weak : CategoryCombos)
	{
		if (const TSharedPtr<STextComboBox> Combo = Weak.Pin())
		{
			Combo->RefreshOptions();
			Combo->SetSelectedItem(CategoryOptions[0]);
		}
	}
	CategoryCombos.RemoveAll([](const TWeakPtr<STextComboBox>& Weak) { return !Weak.IsValid(); });
	for (const TSharedPtr<FAcbCueTemplate>& Template : CueTemplates)
	{
		if (!Session.GetCategories().Contains(Template->Category))
		{
			Template->Category.Reset();
		}
	}
	RebuildCueRows();
}

void SAcbEditor::RefreshSlots()
{
	SlotOptions.Reset();
	if (const TSharedPtr<FAcbCueRow> Row = GetFirstSelected())
	{
		for (const FString& Line : Session.DescribeSlots(Row->Index))
		{
			SlotOptions.Add(MakeShared<FString>(Line));
		}
	}
	SelectedSlot = FMath::Clamp(SelectedSlot, 0, FMath::Max(0, SlotOptions.Num() - 1));
	if (SlotCombo.IsValid())
	{
		SlotCombo->RefreshOptions();
		if (SlotOptions.Num() > 0)
		{
			SlotCombo->SetSelectedItem(SlotOptions[SelectedSlot]);
		}
		else
		{
			SlotCombo->ClearSelection();
		}
	}
	LoadTiming(false);
	LoadVolumes();
}

FString SAcbEditor::FormatTimingValue(double Samples) const
{
	if (bTimingSamples || CachedTiming.SamplingRate <= 0)
	{
		return FString::Printf(TEXT("%lld"), int64(FMath::RoundToDouble(Samples)));
	}
	return FString::Printf(TEXT("%.3f"), Samples / double(CachedTiming.SamplingRate));
}

bool SAcbEditor::ParseTimingValue(const FString& InText, double& OutSamples) const
{
	const FString Text = InText.TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return false;
	}
	if (bTimingSamples)
	{
		if (!Text.IsNumeric() || Text.Contains(TEXT(".")) || Text.StartsWith(TEXT("-")))
		{
			return false;
		}
		OutSamples = double(FCString::Atoi64(*Text));
		return true;
	}
	double Seconds = 0.0;
	TArray<FString> Parts;
	Text.ParseIntoArray(Parts, TEXT(":"), false);
	if (Parts.Num() < 1 || Parts.Num() > 3)
	{
		return false;
	}
	for (const FString& Part : Parts)
	{
		if (Part.IsEmpty() || !Part.IsNumeric() || Part.StartsWith(TEXT("-")))
		{
			return false;
		}
		Seconds = Seconds * 60.0 + FCString::Atod(*Part);
	}
	OutSamples = Seconds * double(FMath::Max(CachedTiming.SamplingRate, 1));
	return true;
}

void SAcbEditor::LoadTiming(bool bForce)
{
	const TSharedPtr<FAcbCueRow> Row = GetSelection().Num() == 1 ? GetFirstSelected() : nullptr;
	const uint32 CueId = Row.IsValid() ? Row->Id : MAX_uint32;
	if (!bForce && CueId == TimingCueId && SelectedSlot == TimingSlot)
	{
		return;
	}
	TimingCueId = CueId;
	TimingSlot = SelectedSlot;
	CachedTiming = Row.IsValid() ? Session.GetSlotTiming(Row->Index, SelectedSlot) : FAcbSlotTiming();
	const FAcbSlotTiming& T = CachedTiming;
	if (!T.bValid)
	{
		TimingStart.Reset();
		TimingLoopStart.Reset();
		TimingLoopEnd.Reset();
		bTimingLoop = false;
		LoadPeaks();
		return;
	}
	const double Rate = double(FMath::Max(T.SamplingRate, 1));
	TimingStart = T.bHasStart ? FormatTimingValue(double(T.StartMs) * Rate / 1000.0) : FString();
	if (!bTimingSamples && T.bHasStart)
	{
		TimingStart = FString::Printf(TEXT("%.3f"), double(T.StartMs) / 1000.0);
	}
	bTimingLoop = T.bLoop;
	TimingLoopStart = FormatTimingValue(double(T.bWaveformLoops ? T.LoopStart : 0));
	TimingLoopEnd = FormatTimingValue(double(T.bWaveformLoops ? T.LoopEnd : T.NumSamples));
	LoadPeaks();
}

void SAcbEditor::SetTimingUnits(bool bSamples)
{
	if (bSamples == bTimingSamples)
	{
		return;
	}
	auto Convert = [this, bSamples](FString& Field)
	{
		double Samples = 0.0;
		const bool bParsed = ParseTimingValue(Field, Samples);
		if (bParsed)
		{
			const bool bOld = bTimingSamples;
			bTimingSamples = bSamples;
			Field = FormatTimingValue(Samples);
			bTimingSamples = bOld;
		}
	};
	Convert(TimingStart);
	Convert(TimingLoopStart);
	Convert(TimingLoopEnd);
	bTimingSamples = bSamples;
}

bool SAcbEditor::ParseTimingFields(TOptional<int64>& OutStartMs, int64& OutLoopStart, int64& OutLoopEnd)
{
	const FAcbSlotTiming& T = CachedTiming;
	const double Rate = double(FMath::Max(T.SamplingRate, 1));
	OutStartMs.Reset();
	if (!TimingStart.TrimStartAndEnd().IsEmpty())
	{
		double Samples = 0.0;
		if (!ParseTimingValue(TimingStart, Samples))
		{
			AddLog(TEXT("ERROR: Start at: '") + TimingStart + (bTimingSamples ? TEXT("' is not a sample count.") : TEXT("' is not a time (seconds or m:ss.sss).")));
			return false;
		}
		OutStartMs = int64(FMath::RoundToDouble(Samples * 1000.0 / Rate));
	}
	OutLoopStart = 0;
	OutLoopEnd = 0;
	if (bTimingLoop)
	{
		double Start = 0.0;
		double End = 0.0;
		if (!ParseTimingValue(TimingLoopStart, Start) || !ParseTimingValue(TimingLoopEnd, End))
		{
			AddLog(TEXT("ERROR: loop start and end must both be set (") + FString(bTimingSamples ? TEXT("samples") : TEXT("seconds or m:ss.sss")) + TEXT(")."));
			return false;
		}
		OutLoopStart = int64(FMath::RoundToDouble(Start));
		OutLoopEnd = int64(FMath::RoundToDouble(End));
	}
	return true;
}

void SAcbEditor::ApplyTiming()
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (!Row.IsValid() || !CachedTiming.bValid || GetSelection().Num() != 1)
	{
		AddLog(TEXT("ERROR: select one cue and a track."));
		return;
	}
	TOptional<int64> StartMs;
	int64 LoopStart = 0;
	int64 LoopEnd = 0;
	if (!ParseTimingFields(StartMs, LoopStart, LoopEnd))
	{
		return;
	}
	FString Message;
	Report(Session.SetSlotTiming(Row->Index, SelectedSlot, StartMs, bTimingLoop, LoopStart, LoopEnd, Message), Message);
}

bool SAcbEditor::TimingFieldsMatch(const FAcbSlotTiming& T) const
{
	const double Rate = double(FMath::Max(T.SamplingRate, 1));
	FString Start;
	if (T.bHasStart)
	{
		Start = bTimingSamples ? FormatTimingValue(double(T.StartMs) * Rate / 1000.0) : FString::Printf(TEXT("%.3f"), double(T.StartMs) / 1000.0);
	}
	if (TimingStart.TrimStartAndEnd() != Start || bTimingLoop != T.bLoop)
	{
		return false;
	}
	return !bTimingLoop
		|| (TimingLoopStart.TrimStartAndEnd() == FormatTimingValue(double(T.bWaveformLoops ? T.LoopStart : 0))
			&& TimingLoopEnd.TrimStartAndEnd() == FormatTimingValue(double(T.bWaveformLoops ? T.LoopEnd : T.NumSamples)));
}

void SAcbEditor::PreviewCue(bool bNearLoopEnd, int64 FromFrame)
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (!Row.IsValid() || GetSelection().Num() != 1)
	{
		AddLog(TEXT("ERROR: select one cue to play."));
		return;
	}
	const FString CueName = Row->Name;
	const int32 CueIndex = Row->Index;
	const int32 Slot = SelectedSlot;

	if (CachedTiming.bValid && !TimingFieldsMatch(CachedTiming))
	{
		TOptional<int64> StartMs;
		int64 LoopStart = 0;
		int64 LoopEnd = 0;
		if (!ParseTimingFields(StartMs, LoopStart, LoopEnd))
		{
			return;
		}
		FString Message;
		if (!Report(Session.SetSlotTiming(CueIndex, Slot, StartMs, bTimingLoop, LoopStart, LoopEnd, Message), Message))
		{
			return;
		}
	}

	const FAcbSlotTiming T = Session.GetSlotTiming(CueIndex, Slot);
	if (!T.bValid)
	{
		AddLog(FString::Printf(TEXT("ERROR: could not read the audio of %s, track %d."), *CueName, Slot));
		return;
	}
	const int64 Rate = FMath::Max(T.SamplingRate, 1);
	int64 StartFrame = T.bHasStart ? T.StartMs * Rate / 1000 : 0;
	if (bNearLoopEnd)
	{
		if (!T.bLoop || !T.bWaveformLoops)
		{
			AddLog(TEXT("ERROR: this track doesn't loop, so there is no loop end to hear."));
			return;
		}
		StartFrame = FMath::Max<int64>(0, T.LoopEnd - 4 * Rate);
	}
	if (FromFrame >= 0)
	{
		StartFrame = FMath::Clamp<int64>(FromFrame, 0, T.NumSamples);
	}

	if (!Preview.IsValid())
	{
		Preview = MakeShared<FAcbPreviewPlayer>();
	}
	Preview->Stop();
	FString Wav;
	FString Error;
	if (!Session.DecodeSlotAudio(CueIndex, Slot, Wav, Error)
		|| !Preview->Play(Wav, StartFrame, T.bLoop && T.bWaveformLoops, T.LoopStart, T.LoopEnd, GetPreviewGain(CueIndex, Slot), Error))
	{
		AddLog(TEXT("ERROR: ") + Error);
		return;
	}
	PreviewCueIndex = CueIndex;
	PreviewSlot = Slot;
	if (Peaks.Num() == 0 || PeaksCueId != Row->Id || PeaksSlot != Slot)
	{
		Preview->GetPeaks(2048, Peaks);
		PeaksCueId = Row->Id;
		PeaksSlot = Slot;
	}
	AddLog(FString::Printf(TEXT("Playing %s, track %d, from %.3fs%s."), *CueName, Slot, double(StartFrame) / double(Rate),
		bNearLoopEnd ? TEXT(" (4 s before the loop end)") : (T.bLoop && T.bWaveformLoops ? TEXT(", looping") : TEXT(""))));
}

void SAcbEditor::SeekPreview(int64 Frame)
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (Row.IsValid() && Preview.IsValid() && Preview->IsPlaying() && Row->Index == PreviewCueIndex && SelectedSlot == PreviewSlot && TimingFieldsMatch(CachedTiming))
	{
		Preview->Seek(Frame);
		return;
	}
	PreviewCue(false, Frame);
}

int64 SAcbEditor::GetTimelinePosition() const
{
	const TSharedPtr<FAcbCueRow> Row = GetFirstSelected();
	if (!Row.IsValid() || !Preview.IsValid() || !Preview->IsPlaying() || Row->Index != PreviewCueIndex || SelectedSlot != PreviewSlot)
	{
		return -1;
	}
	return Preview->GetPositionFrames();
}

int64 SAcbEditor::ParseTimingField(const FString& Text) const
{
	double Samples = 0.0;
	return CachedTiming.bValid && ParseTimingValue(Text, Samples) ? int64(FMath::RoundToDouble(Samples)) : int64(-1);
}

void SAcbEditor::SetTimingMarker(EAcbTimelineMarker Marker, int64 Frame)
{
	if (!CachedTiming.bValid || Frame < 0)
	{
		return;
	}
	Frame = FMath::Clamp<int64>(Frame, 0, CachedTiming.NumSamples);
	switch (Marker)
	{
	case EAcbTimelineMarker::Start:
		if (CachedTiming.bCanSetStart)
		{
			TimingStart = FormatTimingValue(double(Frame));
		}
		break;
	case EAcbTimelineMarker::LoopStart:
		if (!bTimingLoop || ParseTimingField(TimingLoopEnd) <= Frame)
		{
			TimingLoopEnd = FormatTimingValue(double(CachedTiming.NumSamples));
		}
		bTimingLoop = true;
		TimingLoopStart = FormatTimingValue(double(Frame));
		break;
	case EAcbTimelineMarker::LoopEnd:
		if (!bTimingLoop || ParseTimingField(TimingLoopStart) < 0 || ParseTimingField(TimingLoopStart) >= Frame)
		{
			TimingLoopStart = FormatTimingValue(0.0);
		}
		bTimingLoop = true;
		TimingLoopEnd = FormatTimingValue(double(Frame));
		break;
	default:
		break;
	}
}

void SAcbEditor::LoadPeaks()
{
	const TSharedPtr<FAcbCueRow> Row = GetSelection().Num() == 1 ? GetFirstSelected() : nullptr;
	const uint32 CueId = Row.IsValid() ? Row->Id : MAX_uint32;
	if (CueId == PeaksCueId && SelectedSlot == PeaksSlot)
	{
		return;
	}
	Peaks.Reset();
	PeaksCueId = CueId;
	PeaksSlot = SelectedSlot;
	FString Wav;
	FString Error;
	if (Row.IsValid() && CachedTiming.bValid && Session.DecodeSlotAudio(Row->Index, SelectedSlot, Wav, Error, true))
	{
		FAcbPreviewPlayer::ReadPeaks(Wav, 2048, Peaks);
	}
}

void SAcbEditor::StopPreview()
{
	if (Preview.IsValid())
	{
		Preview->Stop();
	}
}

FText SAcbEditor::GetPreviewText() const
{
	const int64 Position = Preview.IsValid() ? Preview->GetPositionMs() : -1;
	if (Position < 0)
	{
		return FAcbEditorSession::GetVGAudioCli().IsEmpty() ? LOCTEXT("PreviewUnavailable", "VGAudioCli.exe not found") : FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("%d:%06.3f"), int32(Position / 60000), double(Position % 60000) / 1000.0));
}

void SAcbEditor::Refresh()
{
	TArray<int32> SelectedIds;
	for (const TSharedPtr<FAcbCueRow>& Row : GetSelection())
	{
		SelectedIds.Add(int32(Row->Id));
	}
	AllRows = Session.BuildRows();
	UpdateSizeEstimate();
	ApplyFilter();
	if (ListView.IsValid())
	{
		for (const TSharedPtr<FAcbCueRow>& Row : VisibleRows)
		{
			if (SelectedIds.Contains(int32(Row->Id)))
			{
				ListView->SetItemSelection(Row, true, ESelectInfo::Direct);
			}
		}
	}
}

void SAcbEditor::ApplyFilter()
{
	VisibleRows.Reset();
	for (const TSharedPtr<FAcbCueRow>& Row : AllRows)
	{
		if (FilterText.IsEmpty() || Row->Name.Contains(FilterText) || Row->Categories.Contains(FilterText)
			|| FString::Printf(TEXT("%u"), Row->Id) == FilterText)
		{
			VisibleRows.Add(Row);
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SAcbEditor::OnGenerateRow(TSharedPtr<FAcbCueRow> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(SAcbCueTableRow, Owner).Item(Item);
}

void SAcbEditor::OnSelectionChanged(TSharedPtr<FAcbCueRow> Item, ESelectInfo::Type)
{
	const uint32 NewId = Item.IsValid() ? Item->Id : MAX_uint32;
	if (NewId != SelectedCueId)
	{
		SelectedCueId = NewId;
		if (Item.IsValid())
		{
			RenameTo = Item->Name;
			CopyTo = Item->Name + TEXT("_copy");
			SelectedCategory = Item->Categories;
			if (SelectedCategoryCombo.IsValid())
			{
				const TSharedPtr<FString>* Match = CategoryOptions.FindByPredicate([this](const TSharedPtr<FString>& Option) { return *Option == SelectedCategory; });
				SelectedCategoryCombo->SetSelectedItem(Match ? *Match : CategoryOptions[0]);
			}
		}
		SelectedSlot = 0;
	}
	RefreshSlots();
}

TArray<TSharedPtr<FAcbCueRow>> SAcbEditor::GetSelection() const
{
	return ListView.IsValid() ? ListView->GetSelectedItems() : TArray<TSharedPtr<FAcbCueRow>>();
}

TSharedPtr<FAcbCueRow> SAcbEditor::GetFirstSelected() const
{
	const TArray<TSharedPtr<FAcbCueRow>> Selection = GetSelection();
	return Selection.Num() > 0 ? Selection[0] : TSharedPtr<FAcbCueRow>();
}

void SAcbEditor::UpdateSizeEstimate()
{
	CachedEstimate = Session.IsOpen() ? Session.EstimateOutputAcbSize(*GetDefault<UAcbEditorSettings>()) : 0;
}

void SAcbEditor::OnSettingsChanged(UObject*, FPropertyChangedEvent&)
{
	UpdateSizeEstimate();
	RefreshPresets();
}

FText SAcbEditor::GetSizeText() const
{
	if (!Session.IsOpen())
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("Saved .acb ~ %.2f GiB (import limit 2.00)"), double(CachedEstimate) / double(1ull << 30)));
}

FSlateColor SAcbEditor::GetSizeColor() const
{
	if (!Session.IsOpen())
	{
		return FSlateColor::UseForeground();
	}
	if (CachedEstimate > FAcbEditorSession::CriImportLimit)
	{
		return FSlateColor(FLinearColor(1.f, 0.25f, 0.2f));
	}
	if (CachedEstimate > FAcbEditorSession::CriImportLimit / 100 * 95)
	{
		return FSlateColor(FLinearColor(1.f, 0.75f, 0.2f));
	}
	return FSlateColor::UseForeground();
}

void SAcbEditor::AddLogLines(const TArray<FString>& Lines)
{
	for (const FString& Line : Lines)
	{
		AddLog(Line);
	}
}

void SAcbEditor::AddLog(const FString& Line)
{
	UE_LOG(LogAEWAcbEditor, Log, TEXT("%s"), *Line);
	LogText += Line + TEXT("\n");
}

#undef LOCTEXT_NAMESPACE
