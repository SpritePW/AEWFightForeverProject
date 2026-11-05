#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AEWArenaTemplateSettings.generated.h"

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Arena Maker"))
class AEWARENATEMPLATE_API UAEWArenaTemplateSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAEWArenaTemplateSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "UserDefinedEnum", DisplayName = "Arena ID Enum", ToolTip = "The enum of arena IDs (EArenaID). A new arena gets a new entry here."))
	FSoftObjectPath ArenaEnum;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "DataTable", DisplayName = "Arena Table", ToolTip = "The arena table the game reads (DT_ArenaOriginal). Used to find used IDs and names and the row to copy."))
	FSoftObjectPath ArenaTable;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "DataTable", DisplayName = "New Arena Rows Table", ToolTip = "New arena rows are written here. Must be a parent of the Arena Table."))
	FSoftObjectPath ArenaNewTable;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (DisplayName = "Full Map Path In LevelName", ToolTip = "Write the map's full package path (/Game/.../17_MyArena) into LevelName. The game looks short names up in .pak file lists only, so a mod map (which lives in a .utoc) is only found by its full path. Sync Arena List converts existing rows."))
	bool bUseFullLevelPath = true;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (DisplayName = "Display Name Via Enum Text", ToolTip = "The game shows ST_Name/ST_Key looked up in the localization table. Give the arena's enum entry a localizable display name and point ST_Name/ST_Key at its namespace and key, so the display name needs no string table. Sync Arena List applies it to existing arenas, using their Arena Name."))
	bool bLocalizedDisplayName = true;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (DisplayName = "Display Name Namespace", EditCondition = "bLocalizedDisplayName"))
	FString DisplayNameNamespace = TEXT("AEWModArena");

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (DisplayName = "Create Unlock Row", ToolTip = "Give each new arena its own default-unlocked unlock row and point the arena's ItemID at it."))
	bool bCreateUnlock = true;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (AllowedClasses = "DataTable", DisplayName = "Unlock Table", ToolTip = "New unlock rows are written here. Must be a parent of the Base Unlock Table."))
	FSoftObjectPath UnlockTable;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (AllowedClasses = "DataTable", DisplayName = "Base Unlock Table", ToolTip = "The composite unlock table the game reads. Used to find used unlock IDs and the template row."))
	FSoftObjectPath BaseUnlockTable;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (DisplayName = "Unlock Template Row", ToolTip = "Unlock row copied when the template's arena has no unlock row of its own."))
	FName UnlockTemplateRow;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (DisplayName = "First Unlock ID"))
	int32 FirstUnlockID = 10100000;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (DisplayName = "Last Unlock ID"))
	int32 LastUnlockID = 10199999;

	UPROPERTY(EditAnywhere, config, Category = "Removing", meta = (DisplayName = "Update Tables When A Map Is Deleted", ToolTip = "When a mod arena's map is deleted (for example by deleting its folder), remove its arena row, enum entry and unlock row."))
	bool bSyncOnDelete = true;

	UPROPERTY(EditAnywhere, config, Category = "Removing", meta = (DisplayName = "Renumber Arena IDs", ToolTip = "After removing an arena, close the gap: the mod arenas after it move down one ID (rows, ArenaId, enum entries)."))
	bool bRenumberOnDelete = true;

	UPROPERTY(EditAnywhere, config, Category = "Removing", meta = (DisplayName = "Rename Maps To New ID", ToolTip = "When an arena's ID changes, rename its <ID>_Name folder, maps, sublevels and label to match."))
	bool bRenameMapsOnRenumber = true;

	UPROPERTY(EditAnywhere, config, Category = "Level Blueprints", meta = (DisplayName = "Update Level Names In Level Blueprints", ToolTip = "When maps are copied from a template or renamed to a new ID, replace the old map and sublevel names typed into Level Blueprint pins and variable defaults (Load Stream Level, Get Streaming Level, name literals...) and recompile."))
	bool bFixLevelBlueprints = true;

	UPROPERTY(EditAnywhere, config, Category = "Defaults", meta = (ContentDir, DisplayName = "Template Folder"))
	FDirectoryPath TemplateRoot;

	UPROPERTY(EditAnywhere, config, Category = "Defaults", meta = (ContentDir, DisplayName = "Parent Folder For New Arenas"))
	FDirectoryPath DefaultParentFolder;

	UPROPERTY(EditAnywhere, config, Category = "Arena Overrides", meta = (AllowedClasses = "DataTable", DisplayName = "Game Camera Table", ToolTip = "The game's gameplay camera table (DT_GameplayCameraParam). Add Camera Override copies it into the arena folder."))
	FSoftObjectPath GameCameraTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/DT_GameplayCameraParam.DT_GameplayCameraParam"));

	UPROPERTY(EditAnywhere, config, Category = "Arena Overrides", meta = (ClampMin = "1.0", DisplayName = "Camera Zoom-Out Scale", ToolTip = "Applied to the arena's copy of the camera table: RadiusMax_Arena is multiplied by this, RadiusMax_Ring and RadiusMin_Arena by a smaller share of it."))
	float CameraZoomOutScale = 2.0f;

};
