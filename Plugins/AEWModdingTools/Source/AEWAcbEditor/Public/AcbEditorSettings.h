#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "AcbEditorSettings.generated.h"

UENUM()
enum class EAcbNewAudioStorage : uint8
{
	Auto UMETA(ToolTip = "Streaming AWB if open, else the embedded AWB."),
	Streaming UMETA(DisplayName = "Streaming AWB"),
	Embedded UMETA(DisplayName = "Embedded in ACB")
};

USTRUCT()
struct FAcbCueTemplate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Cue", meta = (ToolTip = "Cue name or prefix, e.g. mus_ent_."))
	FString Pattern;

	UPROPERTY(EditAnywhere, Category = "Cue", meta = (ToolTip = "ACB category, e.g. EntranceMusic. Empty = the copied cue's."))
	FString Category;

	UPROPERTY(EditAnywhere, Category = "Cue", meta = (ToolTip = "Cue to copy sound settings from."))
	FString Template;
};

USTRUCT()
struct FAcbCuePreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Preset")
	FString Name;

	UPROPERTY(EditAnywhere, Category = "Preset", meta = (ToolTip = "Cue sheet this preset is for. Empty = all."))
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

	UPROPERTY(EditAnywhere, Category = "Table", meta = (AllowedClasses = "DataTable", ToolTip = "Table the game reads (usually the composite)."))
	FSoftObjectPath Table;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (AllowedClasses = "DataTable", DisplayName = "New Table", ToolTip = "New rows go here. Must be a parent of Table."))
	FSoftObjectPath NewTable;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (DisplayName = "Numbered Rows", ToolTip = "Rows are named by number. Off for named rows (e.g. SFX_FOOTSTEP_WALK)."))
	bool bNumberedRows = true;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (EditCondition = "bNumberedRows", EditConditionHides, DisplayName = "First ID", ToolTip = "First free row number from here. 0 = after the highest. WrestlerID tables use the WrestlerID."))
	int32 FirstID = 0;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (EditCondition = "bNumberedRows", EditConditionHides, DisplayName = "Last ID"))
	int32 LastID = 0;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (AllowedClasses = "DataTable", DisplayName = "Unlock New Table", ToolTip = "New unlock items go here. None = share the template's."))
	FSoftObjectPath UnlockNewTable;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (DisplayName = "First Unlock ID", ToolTip = "First free unlock ID from here. 0 = after the highest."))
	int32 FirstUnlockID = 0;

	UPROPERTY(EditAnywhere, Category = "Table", meta = (DisplayName = "Last Unlock ID"))
	int32 LastUnlockID = 0;
};

UCLASS(config = AEWModdingTools, meta = (DisplayName = "ACB Editor"))
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
		ToolTip = "Where Save writes the .acb/.awb. Empty = Saved/AEWAcbEditor."))
	FDirectoryPath OutputDirectory;

	UPROPERTY(EditAnywhere, config, Category = "Output", AdvancedDisplay, meta = (DisplayName = "Write all columns per row",
		ToolTip = "Store every column per row, like cri_utf_tool."))
	bool bForcePerRow = false;

	UPROPERTY(EditAnywhere, config, Category = "Output", meta = (DisplayName = "Remove unused audio",
		ToolTip = "On save, remove sequences, tracks and audio no cue uses."))
	bool bRemoveUnusedData = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Import after save",
		ToolTip = "Save also imports into the import folder. Off = only write the .acb/.awb."))
	bool bImportAfterSave = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Save assets after import",
		ToolTip = "Save the cue sheet and changed cue assets."))
	bool bSaveAssetsAfterImport = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Cues use cue sheet's chunk",
		ToolTip = "On import, give each cue asset the cue sheet's chunk."))
	bool bAssignSheetChunkToCues = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Ignore cue cache refresh",
		ToolTip = "Ignore CriWare's cached-info refresh on load."))
	bool bIgnoreCueCacheRefresh = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Copy AWB to project",
		ToolTip = "On save, copy the .awb to Content/<AWB folder>."))
	bool bCopyAwbToProject = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "AWB folder",
		ToolTip = "Folder under Content the game loads .awb files from."))
	FString AwbDirectory = TEXT("GameData/Resources/CriWareData");

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Package AWB folder",
		ToolTip = "Package the AWB folder, each .awb in its cue sheet's chunk."))
	bool bAddAwbFolderToPackaging = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Delete removed cue assets",
		ToolTip = "On import, delete unused assets of deleted or renamed cues."))
	bool bDeleteRemovedCueAssets = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Delete rows of deleted cues",
		ToolTip = "On import, remove rows left with no cues, plus their unused unlock items."))
	bool bDeleteRowsOfDeletedCues = true;

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Update DataTables",
		ToolTip = "On import, re-point renamed cues, clear deleted ones and add planned rows."))
	bool bUpdateDataTables = true;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Cue Tables", TitleProperty = "Name",
		ToolTip = "DataTables that reference cues. New audio copies the template cue's row."))
	TArray<FAcbRowTable> RowTables;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Base Unlock Table",
		ToolTip = "Unlock table the game reads (composite)."))
	FSoftObjectPath BaseUnlockTable;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Unlock New Items",
		ToolTip = "Copied unlock items start unlocked (no DLC, shop or challenge, price 0)."))
	bool bUnlockNewItems = true;


	UPROPERTY(EditAnywhere, config, Category = "New tracks", meta = (DisplayName = "Cue presets",
		ToolTip = "Your own cue sets, listed after the built-in ones."))
	TArray<FAcbCuePreset> CuePresets;

	UPROPERTY(EditAnywhere, config, Category = "New tracks", meta = (DisplayName = "Loop new tracks"))
	bool bLoopNewTracks = true;

	UPROPERTY(EditAnywhere, config, Category = "New tracks", meta = (DisplayName = "Store new audio in"))
	EAcbNewAudioStorage NewAudioStorage = EAcbNewAudioStorage::Auto;

	UPROPERTY(EditAnywhere, config, Category = "New tracks", meta = (FilePathFilter = "exe", DisplayName = "VGAudioCli.exe",
		ToolTip = "Custom VGAudioCli.exe. Empty = the bundled one."))
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
