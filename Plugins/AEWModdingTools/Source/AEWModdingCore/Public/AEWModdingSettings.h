#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AEWModdingSettings.generated.h"

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "AEW Modding Tools"))
class AEWMODDINGCORE_API UAEWModdingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAEWModdingSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(EditAnywhere, config, Category = "Packaging", meta = (DisplayName = "Create Asset Labels", ToolTip = "The tools put what they create into PrimaryAssetLabels (always cook, own chunk), so Package Project picks it up."))
	bool bCreateLabels = true;

	UPROPERTY(EditAnywhere, config, Category = "Packaging", meta = (DisplayName = "Chunk ID", ClampMin = "1", ToolTip = "Chunk the labels put assets in (pakchunk<ID>-WindowsNoEditor)."))
	int32 ChunkId = 100;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Label Tables", ToolTip = "Put the DataTables the tools change into the two table labels below. Turn off if another mod ships those tables."))
	bool bLabelSharedTables = true;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "New Tables Label", ToolTip = "Label for every DataTable whose name ends in _New (the rows you add)."))
	FString NewTablesLabel;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "New Tables Chunk ID", ClampMin = "1"))
	int32 NewTablesChunkId = 101;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Base Tables Label", ToolTip = "Label for every other DataTable (composites, _Original tables, ...) and the arena enum."))
	FString BaseTablesLabel;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Base Tables Chunk ID", ClampMin = "1"))
	int32 BaseTablesChunkId = 102;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (ContentDir, DisplayName = "Table Folders", ToolTip = "Folders Rebuild Table Labels scans (recursively)."))
	TArray<FDirectoryPath> TableFolders;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Skip Unchanged Tables", ToolTip = "Leave a composite and its _Original/_New parents out of the table labels when every _New parent is empty, since the game's own table is then unchanged."))
	bool bSkipUnchangedTables = true;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Rebuild: All Tables", ToolTip = "Off: Rebuild Table Labels only takes composites that have a _New parent, plus their parents. On: every DataTable in the Table Folders (this ships copies of all those game tables)."))
	bool bRebuildAllTables = false;

	UPROPERTY(EditAnywhere, config, Category = "Packaging", meta = (DisplayName = "Cook Labelled Assets", ToolTip = "When packaging, add every asset of every Always Cook label to the cook. UE 4.27 doesn't do this by itself: without it, labelled assets are only cooked if something else pulls them in."))
	bool bCookLabelledAssets = true;

	UPROPERTY(EditAnywhere, config, Category = "Packaging", meta = (DisplayName = "Allow Cooking Labelled Assets", ToolTip = "If a labelled asset sits under a 'Directories to never cook' entry, narrow that entry so the asset can be cooked."))
	bool bAllowCookingLabelledAssets = true;
};
