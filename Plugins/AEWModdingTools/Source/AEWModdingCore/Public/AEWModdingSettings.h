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

	UPROPERTY(EditAnywhere, config, Category = "Packaging", meta = (DisplayName = "Chunk ID", ClampMin = "1", ToolTip = "Chunk the labels put assets in (pakchunk<ID>-WindowsNoEditor)."))
	int32 ChunkId = 100;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "New Tables Label", ToolTip = "Label for every DataTable whose name ends in _New (the rows you add)."))
	FString NewTablesLabel;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "New Tables Chunk ID", ClampMin = "1"))
	int32 NewTablesChunkId = 101;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Base Tables Label", ToolTip = "Label for every other DataTable (composites, _Original tables, ...) and the arena enum."))
	FString BaseTablesLabel;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Base Tables Chunk ID", ClampMin = "1"))
	int32 BaseTablesChunkId = 102;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (ContentDir, DisplayName = "Table Folders", ToolTip = "Folders whose DataTables are kept in the table labels automatically: when a table here is saved, removed or renamed, the labels are rebuilt. Sub-folders are included."))
	TArray<FDirectoryPath> TableFolders;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Always Label Tables", ToolTip = "Game DataTables that are edited directly (not through a _New parent) and must always be packaged, e.g. DT_ConvertEntranceCutsceneList for entrance swaps. They are kept in the Base Tables label automatically."))
	TArray<FSoftObjectPath> AlwaysLabelTables;

};
