#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "AcbEditorSettings.generated.h"

UENUM()
enum class EAcbNewAudioStorage : uint8
{
	Auto UMETA(ToolTip = "Streaming AWB if one is open, otherwise the ACB's embedded AWB."),
	Streaming UMETA(DisplayName = "Streaming AWB"),
	Embedded UMETA(DisplayName = "Embedded in ACB")
};

USTRUCT()
struct FAcbCueTemplate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Cue", meta = (ToolTip = "Cue name, or the start of one finished in the Add audio rows, e.g. mus_ent_."))
	FString Pattern;

	UPROPERTY(EditAnywhere, Category = "Cue", meta = (ToolTip = "Category as the ACB names it, e.g. EntranceMusic or Crowd + CrowdChant. Empty = the copied cue's."))
	FString Category;

	UPROPERTY(EditAnywhere, Category = "Cue", meta = (ToolTip = "Existing cue to copy sound settings from, e.g. mus_ingame_AdamPage."))
	FString Template;
};

USTRUCT()
struct FAcbCuePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Preset")
	FString Name;

	UPROPERTY(EditAnywhere, Category = "Preset", meta = (ToolTip = "Cue sheet this preset is for, e.g. music. Empty = every cue sheet."))
	FString CueSheet;

	UPROPERTY(EditAnywhere, Category = "Preset")
	TArray<FAcbCueTemplate> Cues;
};

USTRUCT()
struct FAcbRowTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Table")
	FString Name;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (AllowedClasses = "DataTable", ToolTip = "The table the game reads (usually the composite). Template rows and used IDs come from here."))
	FSoftObjectPath Table;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (AllowedClasses = "DataTable", DisplayName = "New Table", ToolTip = "New rows are written here. Must be a parent of Table."))
	FSoftObjectPath NewTable;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (DisplayName = "Numbered Rows", ToolTip = "Rows are named by number (most tables). Off for tables with named rows, e.g. SFX_FOOTSTEP_WALK; new rows then take the copied row's name with the new name in it."))
	bool bNumberedRows = true;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (EditCondition = "bNumberedRows", EditConditionHides, DisplayName = "First ID", ToolTip = "New rows get the first free number from here. 0 = after the highest used. Tables numbered by WrestlerID (chants, roster names, voice notifies, roster themes) use the new row's WrestlerID instead when it isn't None."))
	int32 FirstID = 0;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (EditCondition = "bNumberedRows", EditConditionHides, DisplayName = "Last ID"))
	int32 LastID = 0;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (AllowedClasses = "DataTable", DisplayName = "Unlock New Table", ToolTip = "Copies of this table's unlock items are written here, e.g. DT_UnlockableItemParam_Music. Must be a parent of Base Unlock Table. None = new rows share the template row's unlock item."))
	FSoftObjectPath UnlockNewTable;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (DisplayName = "First Unlock ID", ToolTip = "New unlock items get the first free ID from here (or the template's row-to-ID offset when that lands in the range). 0 = after the highest used."))
	int32 FirstUnlockID = 0;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (DisplayName = "Last Unlock ID"))
	int32 LastUnlockID = 0;
};

UCLASS(config = AEWAcbEditor, meta = (DisplayName = "AEW ACB Editor"))
class AEWACBEDITOR_API UAcbEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAcbEditorSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("AEWAcbEditor"); }
	virtual bool SupportsAutoRegistration() const override { return false; }
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, config, Category = "Output", meta = (DisplayName = "Output folder",
		ToolTip = "Where Save writes the .acb (and .awb). Empty = Saved/AEWAcbEditor."))
	FDirectoryPath OutputDirectory;

	UPROPERTY(EditAnywhere, config, Category = "Output", AdvancedDisplay, meta = (DisplayName = "Write all columns per row",
		ToolTip = "Store every column per row, like cri_utf_tool. Off keeps the original layout."))
	bool bForcePerRow = false;

	UPROPERTY(EditAnywhere, config, Category = "Output", meta = (DisplayName = "Remove unused audio",
		ToolTip = "On save, remove sequences, tracks, synths and waveforms no cue uses any more (e.g. after deleting cues) and drop their audio from the AWB. Shared audio that another cue still plays is kept."))
	bool bRemoveUnusedData = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Import after save",
		ToolTip = "Save also imports the cue sheet into the import folder with the CriWare importer, updating cue assets and DataTables. Off = Save only writes the .acb and .awb."))
	bool bImportAfterSave = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Save assets after import",
		ToolTip = "Save the cue sheet and changed cue assets."))
	bool bSaveAssetsAfterImport = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Cues use cue sheet's chunk",
		ToolTip = "On import, give each cue asset the cue sheet's chunk."))
	bool bAssignSheetChunkToCues = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Ignore cue cache refresh",
		ToolTip = "Don't mark cues unsaved when CriWare only refreshes cached info on load. Real changes still save."))
	bool bIgnoreCueCacheRefresh = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Copy AWB to project",
		ToolTip = "On save, copy the .awb to Content/<AWB folder>."))
	bool bCopyAwbToProject = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "AWB folder",
		ToolTip = "Folder under Content the game loads .awb files from."))
	FString AwbDirectory = TEXT("GameData/Resources/CriWareData");

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Package AWB folder",
		ToolTip = "Add the AWB folder to packaging and put each .awb in its cue sheet's chunk."))
	bool bAddAwbFolderToPackaging = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Delete removed cue assets",
		ToolTip = "When Save imports, delete the SoundAtomCue assets of cues you deleted (and the old assets of renamed cues), after DataTables are updated. Assets something else still uses are kept and listed."))
	bool bDeleteRemovedCueAssets = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Delete rows of deleted cues",
		ToolTip = "When Save imports, a DataTable row left with no cues after its cues were deleted is removed (from whichever table holds it, Original or New). Its unlock item goes too if it is in a Cue Table's Unlock New Table and nothing else uses it. Off = the row stays with its cue fields cleared."))
	bool bDeleteRowsOfDeletedCues = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Update DataTables",
		ToolTip = "When Save imports, DataTable rows that used a renamed cue point to its new asset, rows that used a deleted cue are cleared, and the rows planned under Add audio are added."))
	bool bUpdateDataTables = true;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Cue Tables", TitleProperty = "Name",
		ToolTip = "DataTables that reference cues. When audio is added, the row that uses the template cue is copied into the New Table with the new cues, and its unlock item into the Unlock New Table."))
	TArray<FAcbRowTable> RowTables;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Base Unlock Table",
		ToolTip = "The unlock table the game reads (composite). Template unlock items and used unlock IDs come from here."))
	FSoftObjectPath BaseUnlockTable;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Unlock New Items",
		ToolTip = "Copied unlock items are unlocked from the start: no DLC, shop, crate, pass or challenge condition, price 0. Off = they keep the template's conditions (a DLC theme's item would stay locked to that DLC)."))
	bool bUnlockNewItems = true;


	UPROPERTY(EditAnywhere, config, Category = "New tracks", meta = (DisplayName = "Cue presets",
		ToolTip = "Your own cue sets for new tracks, listed after the built-in ones. Names can be a prefix such as mus_ent_, finished when adding."))
	TArray<FAcbCuePreset> CuePresets;

	UPROPERTY(EditAnywhere, config, Category = "New tracks", meta = (DisplayName = "Loop new tracks"))
	bool bLoopNewTracks = true;

	UPROPERTY(EditAnywhere, config, Category = "New tracks", meta = (DisplayName = "Store new audio in"))
	EAcbNewAudioStorage NewAudioStorage = EAcbNewAudioStorage::Auto;

	UPROPERTY(EditAnywhere, config, Category = "New tracks", meta = (FilePathFilter = "exe", DisplayName = "VGAudioCli.exe",
		ToolTip = "Optional: another VGAudioCli.exe. Empty = the one shipped in the plugin (ThirdParty/VGAudio). Converts added .wav files to .hca and decodes audio for preview."))
	FFilePath VGAudioCliPath;

	UPROPERTY(config)
	FString LastAudioFolder;
	UPROPERTY(config)
	FString LastAcbFolder;
	UPROPERTY(config)
	FString LastCueSheetFolder;

	FString GetOutputDirectory() const;

	static const TArray<FAcbCuePreset>& BuiltInPresets();
	TArray<FAcbCuePreset> GetPresetsFor(const FString& CueSheetName) const;
};
