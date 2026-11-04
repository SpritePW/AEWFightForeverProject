#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AEWArenaTools.generated.h"

class ULevelStreaming;
class UBlueprint;

struct FAEWArenaListEntry
{
	FString Id;
	FString LevelName;
	bool bSelectable = false;
};

UCLASS()
class AEWARENATEMPLATE_API UAEWArenaTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static FString CreateArenaFromTemplate(const FString& TemplateMap, const FString& DestFolder, const FString& InName,
		bool bCreateLabel = true, int32 ChunkId = 100, bool bAddToMapsToCook = true, bool bOpenAfter = true,
		bool bRegisterArena = true, const FString& DisplayName = TEXT(""), int32 ArenaId = -1);

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static int32 RegisterArena(const FString& LevelName, const FString& CopyFromLevelName, const FString& DisplayName,
		const FString& LabelFolder, int32 ChunkId = 100, int32 ArenaId = -1);

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static FString MakeArenaMapName(int32 ArenaId, const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static TArray<FString> GetArenaLevelNames();

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static int32 GetNextArenaId();

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static bool IsArenaIdFree(int32 ArenaId);

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static bool AllowCooking(const FString& PackageName);

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static TArray<FString> FindTemplates(const FString& RootPath = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static bool RepointStreamingLevel(ULevelStreaming* StreamingLevel, const FString& NewPackageName);

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static int32 SyncArenaList(bool bRenumber = true, bool bAsk = true);

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static bool UnregisterArena(int32 ArenaId, bool bRenumber = true);

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static int32 AddStartMarkers();

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static bool AddCameraOverride();

	UFUNCTION(BlueprintCallable, Category = "AEW|Arena")
	static bool AddMatchRuleOverrideNodes();

	static UBlueprint* EnsureStartMarkerBlueprint(const FString& ArenaFolder, bool bRebuild = false);
	static UBlueprint* EnsureCameraOverrideBlueprint(const FString& ArenaFolder, bool bRebuild = false);

	static void HandleRemovedLevels(const TSet<FName>& Levels);
	static bool IsBusy();

	static void OpenDialog();
	static void GetArenaList(TArray<FAEWArenaListEntry>& Out);
	static void OpenSettings();
};
