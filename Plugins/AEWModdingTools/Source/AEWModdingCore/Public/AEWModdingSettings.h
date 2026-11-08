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

	UPROPERTY(EditAnywhere, Category = "Chunk", meta = (ToolTip = "Chunks with the same zip name share one archive."))
	FString Zip;
};

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "AEW Modding Tools"))
class AEWMODDINGCORE_API UAEWModdingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAEWModdingSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(EditAnywhere, config, Category = "Packaging", meta = (DisplayName = "First Chunk ID", ClampMin = "1", ToolTip = "New labels take the first free chunk ID from here."))
	int32 ChunkId = 14;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Base Tables Label", ToolTip = "Label for all other DataTables and the arena enum."))
	FString BaseTablesLabel;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "Base Tables Chunk ID", ClampMin = "1"))
	int32 BaseTablesChunkId = 11;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "New Tables Label", ToolTip = "Label for every _New DataTable."))
	FString NewTablesLabel;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (DisplayName = "New Tables Chunk ID", ClampMin = "1"))
	int32 NewTablesChunkId = 12;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (ContentDir, DisplayName = "Table Folders", ToolTip = "Folders whose DataTables are labelled automatically."))
	TArray<FDirectoryPath> TableFolders;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Always Label Tables", ToolTip = "Game DataTables edited directly that must always be packaged."))
	TArray<FSoftObjectPath> AlwaysLabelTables;

	UPROPERTY(EditAnywhere, config, Category = "Packaged Mods", meta = (DisplayName = "Chunk Names", ToolTip = "Set in Cook > Package Project."))
	TArray<FAEWChunkOutput> ChunkOutputs;

	UPROPERTY(EditAnywhere, config, Category = "Packaged Mods", meta = (DisplayName = "Output Folder", ToolTip = "Where archives go. Empty = Mods in the package folder."))
	FDirectoryPath PackagedModsFolder;

	UPROPERTY(EditAnywhere, config, Category = "Packaged Mods", meta = (DisplayName = "Rename Automatically", ToolTip = "Rename chunks to their file names after packaging."))
	bool bRenamePackagedChunks = true;

	UPROPERTY(EditAnywhere, config, Category = "Packaged Mods", meta = (DisplayName = "Archive Automatically", ToolTip = "Put chunks into their archives after packaging."))
	bool bArchivePackagedChunks = true;

};
