#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AEWModdingSettings.generated.h"

USTRUCT()
struct FAEWChunkOutput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Chunk")
	int32 ChunkId = 0;

	UPROPERTY(EditAnywhere, Category = "Chunk", meta = (ToolTip = "Name the mod is shipped under. _P is added."))
	FString Name;

	UPROPERTY(EditAnywhere, Category = "Chunk", meta = (ToolTip = "Archive the files go in. Chunks with the same zip name share one archive."))
	FString Zip;
};

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "AEW Modding Tools"))
class AEWMODDINGCORE_API UAEWModdingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAEWModdingSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(EditAnywhere, config, Category = "Packaging", meta = (DisplayName = "First Chunk ID", ClampMin = "1", ToolTip = "Every arena, edit part and cue sheet the tools label gets its own chunk (pakchunk<ID>-WindowsNoEditor), so each mod packages separately. New ones take the first free ID from here up, skipping the table chunks and any ID a label already uses."))
	int32 ChunkId = 14;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Base Tables Label", ToolTip = "Label for every other DataTable (composites, _Original tables, ...) and the arena enum."))
	FString BaseTablesLabel;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Base Tables Chunk ID", ClampMin = "1"))
	int32 BaseTablesChunkId = 11;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "New Tables Label", ToolTip = "Label for every DataTable whose name ends in _New (the rows you add)."))
	FString NewTablesLabel;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "New Tables Chunk ID", ClampMin = "1"))
	int32 NewTablesChunkId = 12;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (ContentDir, DisplayName = "Table Folders", ToolTip = "Folders whose DataTables are kept in the table labels automatically: when a table here is saved, removed or renamed, the labels are rebuilt. Sub-folders are included."))
	TArray<FDirectoryPath> TableFolders;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Always Label Tables", ToolTip = "Game DataTables that are edited directly (not through a _New parent) and must always be packaged, e.g. DT_ConvertEntranceCutsceneList for entrance swaps. They are kept in the Base Tables label automatically."))
	TArray<FSoftObjectPath> AlwaysLabelTables;

	UPROPERTY(EditAnywhere, config, Category = "Packaged Mods", meta = (DisplayName = "Chunk Names", ToolTip = "Set in Cook > Package Project."))
	TArray<FAEWChunkOutput> ChunkOutputs;

	UPROPERTY(EditAnywhere, config, Category = "Packaged Mods", meta = (DisplayName = "Output Folder", ToolTip = "Where the archives go. Defaults to a Mods folder inside the package folder."))
	FDirectoryPath PackagedModsFolder;

	UPROPERTY(EditAnywhere, config, Category = "Packaged Mods", meta = (DisplayName = "Rename And Archive Automatically"))
	bool bProcessPackagedChunks = true;

};
