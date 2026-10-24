#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "AcbEditorSession.h"
#include "AcbEditorSettings.h"
#include "AcbImportBridge.h"
#include "SAcbTimeline.h"

class FAcbPreviewPlayer;

class STextComboBox;
class SVerticalBox;

class SAcbEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAcbEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void OpenFile(const FString& AcbPath, const FString& AwbPath = FString(), const FString& CueSheetFolder = FString());

	void OpenForImportFolder(const FString& ContentFolder);
	void SetImportFolder(const FString& ContentFolder);
	void AddLogLines(const TArray<FString>& Lines);

private:
	void OnOpenClicked();
	void OnReloadClicked();
	void OnSaveClicked(bool bImport);
	void OpenSettings();
	bool PickCueSheetFolder(FString& OutFolder);

	void AddTrack();
	void ApplyPreset(int32 PresetIndex);
	void ResetCueNames();
	void RefreshPresets();
	void RebuildCueRows();
	void RequestCueRowsRebuild();
	int32 GetTemplateCueIndex() const;

	void RenameSelected();
	void SetCategoryOfSelected();
	void SetVolume(bool bSlot, TOptional<float> Volume);
	void CommitVolume(bool bSlot);
	void UpdatePreviewGain();
	TSharedRef<SWidget> MakeVolumeSlider(bool bSlot);
	void LoadVolumes();
	float GetPreviewGain(int32 CueIndex, int32 Slot) const;
	void CopySelected();
	void DeleteSelected();
	void SetAudioSourceFromSelection();
	void AddCueFromSource();
	void PointSelectedAtSource();
	void AddVariations();
	void ReplaceVariation();
	void RemoveVariation();
	void ApplyPlayMode();
	void RefreshSlots();
	void LoadTiming(bool bForce);
	void ApplyTiming();
	bool ParseTimingFields(TOptional<int64>& OutStartMs, int64& OutLoopStart, int64& OutLoopEnd);
	void PreviewCue(bool bNearLoopEnd, int64 FromFrame = -1);
	void SeekPreview(int64 Frame);
	void SetTimingMarker(EAcbTimelineMarker Marker, int64 Frame);
	int64 GetTimelinePosition() const;
	int64 ParseTimingField(const FString& Text) const;
	void LoadPeaks();
	bool TimingFieldsMatch(const FAcbSlotTiming& T) const;
	void StopPreview();
	FText GetPreviewText() const;
	void SetTimingUnits(bool bSamples);
	FString FormatTimingValue(double Samples) const;
	bool ParseTimingValue(const FString& Text, double& OutSamples) const;
	void MergeNow();

	void Refresh();
	void ApplyFilter();
	TSharedRef<class ITableRow> OnGenerateRow(TSharedPtr<FAcbCueRow> Item, const TSharedRef<class STableViewBase>& Owner);
	void OnSelectionChanged(TSharedPtr<FAcbCueRow> Item, ESelectInfo::Type SelectInfo);
	TArray<TSharedPtr<FAcbCueRow>> GetSelection() const;
	TSharedPtr<FAcbCueRow> GetFirstSelected() const;

	bool ConfirmDiscard();
	bool PickFiles(const FString& Title, const FString& Types, bool bMultiple, const FString& DefaultFolder, TArray<FString>& OutFiles);
	bool PickFolder(const FString& Title, const FString& DefaultFolder, FString& OutFolder);
	void RememberFolder(const FString& Path);
	void RefreshCategories();
	void UpdateSizeEstimate();
	void OnSettingsChanged(UObject* Settings, struct FPropertyChangedEvent& Event);
	FText GetSizeText() const;
	FSlateColor GetSizeColor() const;
	void AddLog(const FString& Line);
	bool Report(bool bOk, const FString& Message);

	TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Content, const FText& Tooltip = FText::GetEmpty());
	TSharedRef<SWidget> MakeSection(const FText& Title, TSharedRef<SWidget> Content, bool bCollapsed = false);
	TSharedRef<SWidget> MakeTextField(FString* Target, const FText& Hint);
	TSharedRef<SWidget> MakeSettingsView();
	TSharedRef<STextComboBox> MakeCategoryCombo(FString* Target, TSharedPtr<FAcbCueTemplate> Owner = nullptr);
	TSharedRef<STextComboBox> MakePlayModeCombo(uint8* Target);

	FAcbEditorSession Session;
	TArray<TSharedPtr<FAcbCueRow>> AllRows;
	TArray<TSharedPtr<FAcbCueRow>> VisibleRows;
	TSharedPtr<SListView<TSharedPtr<FAcbCueRow>>> ListView;
	FString FilterText;

	TArray<TSharedPtr<FString>> CategoryOptions;
	TArray<TSharedPtr<FString>> PlayModeOptions;
	TArray<uint8> PlayModeValues;
	TArray<TSharedPtr<FString>> PresetOptions;
	TArray<FAcbCuePreset> VisiblePresets;

	FString NewAudioFile;
	FString TemplateCueName;
	TArray<TSharedPtr<FAcbCueTemplate>> CueTemplates;
	TSharedPtr<SVerticalBox> CueRowsBox;
	TSharedPtr<STextComboBox> PresetCombo;
	int32 CurrentPreset = 0;
	bool bRefreshingPresets = false;
	bool bLoopNewTrack = true;

	TMap<FString, FString> RenamedCues;
	TArray<FString> DeletedCues;
	TArray<AcbTableRows::FPlannedRow> PendingRows;
	TArray<TSharedPtr<AcbTableRows::FPlannedRow>> TableCandidates;
	TSharedPtr<SVerticalBox> TableRowsBox;
	void FindTableRows();
	void RebuildTableRows();
	TArray<FString> GetTemplateCueNames() const;
	void NoteRename(const FString& OldName, const FString& NewName);
	void NoteDelete(const FString& Name);
	void ClearPending();
	FText GetPendingText() const;

	FString RenameTo;
	FString CopyTo;
	FString AudioSourceName;
	FString FromSourceName;
	FString FromSourceCategory;
	FString SelectedCategory;
	float CueVolume = 1.f;
	float SlotVolume = 1.f;
	bool bHasCueVolume = false;
	bool bHasSlotVolume = false;
	int32 PreviewCueIndex = INDEX_NONE;
	TArray<float> Peaks;
	uint32 PeaksCueId = MAX_uint32;
	int32 PeaksSlot = INDEX_NONE;
	int32 PreviewSlot = INDEX_NONE;
	uint32 LastVolumeCommit = MAX_uint32;
	double LastVolumeCommitTime = 0.0;
	TSharedPtr<STextComboBox> SelectedCategoryCombo;
	TArray<TSharedPtr<FString>> SlotOptions;
	TSharedPtr<STextComboBox> SlotCombo;
	int32 SelectedSlot = 0;
	uint8 SelectedPlayMode = 4;
	TArray<TWeakPtr<STextComboBox>> CategoryCombos;
	uint32 SelectedCueId = MAX_uint32;

	FAcbSlotTiming CachedTiming;
	uint32 TimingCueId = MAX_uint32;
	TSharedPtr<FAcbPreviewPlayer> Preview;
	int32 TimingSlot = INDEX_NONE;
	FString TimingStart;
	FString TimingLoopStart;
	FString TimingLoopEnd;
	bool bTimingLoop = false;
	bool bTimingSamples = false;

	uint64 CachedEstimate = 0;
	FString ImportFolder;
	FString LogText;
};
