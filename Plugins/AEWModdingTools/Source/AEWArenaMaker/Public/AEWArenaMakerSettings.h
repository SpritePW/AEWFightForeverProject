#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AEWArenaMakerSettings.generated.h"

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Arena Maker"))
class AEWARENAMAKER_API UAEWArenaMakerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAEWArenaMakerSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "UserDefinedEnum", DisplayName = "Arena ID Enum", ToolTip = "The enum of arena IDs (EArenaID). A new arena gets a new entry here."))
	FSoftObjectPath ArenaEnum;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "DataTable", DisplayName = "Arena Table", ToolTip = "The arena table the game reads (DT_ArenaOriginal). Used to find used IDs and names and the row to copy."))
	FSoftObjectPath ArenaTable;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "DataTable", DisplayName = "New Arena Rows Table", ToolTip = "New arena rows are written here. Must be a parent of the Arena Table."))
	FSoftObjectPath ArenaNewTable;

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

	UPROPERTY(EditAnywhere, config, Category = "Level Blueprints", meta = (DisplayName = "Update Level Names In Level Blueprints", ToolTip = "When maps are copied from a template or renamed to a new ID, replace the old map and sublevel names typed into Level Blueprint pins and variable defaults (Load Stream Level, Get Streaming Level, name literals...) and recompile."))
	bool bFixLevelBlueprints = true;

	UPROPERTY(EditAnywhere, config, Category = "Shared Sublevels", meta = (DisplayName = "Keep Shared", ToolTip = "Always Loaded sublevels a template streams from outside its own folder (ArenaCollider_01, ArenaAudience_01, ArenaLightingSample...) are copied into each new arena so it can change them. Sublevels named here stay shared. Blueprint-streamed sublevels (ArenaVictory, entrance lights...) always stay shared because the game loads them by name."))
	TArray<FString> KeepSharedSublevels;

	UPROPERTY(EditAnywhere, config, Category = "Barricades", meta = (DisplayName = "Barricade Class", ToolTip = "Game barricade piece Make Barricade From Selected builds on. Its model is removed and your mesh put in its place, so only its behaviour and reaction, hit and weapon boxes are used.", MetaClass = "Actor"))
	FSoftClassPath BarricadeClass = FSoftClassPath(TEXT("/Game/GameData/Resources/Models/EmvData/Arean/Common/Barricade/0000_Base/BP_Barricade.BP_Barricade_C"));

	UPROPERTY(config)
	bool bExistingArenasConverted = false;

	UPROPERTY(EditAnywhere, config, Category = "Defaults", meta = (ContentDir, DisplayName = "Template Folder"))
	FDirectoryPath TemplateRoot;

	UPROPERTY(EditAnywhere, config, Category = "Defaults", meta = (ContentDir, DisplayName = "Parent Folder For New Arenas"))
	FDirectoryPath DefaultParentFolder;

	UPROPERTY(EditAnywhere, config, Category = "Arena Overrides", meta = (AllowedClasses = "DataTable", DisplayName = "Game Camera Table", ToolTip = "The game's gameplay camera table (DT_GameplayCameraParam). Add Camera Override copies it into the arena folder."))
	FSoftObjectPath GameCameraTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/DT_GameplayCameraParam.DT_GameplayCameraParam"));

	UPROPERTY(EditAnywhere, config, Category = "Arena Overrides", meta = (DisplayName = "Camera Preview Row", ToolTip = "Row of the camera table that Toggle Camera Preview shows."))
	FName CameraPreviewRow = TEXT("Ring_NM");

};
