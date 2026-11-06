#pragma once

#include "CoreMinimal.h"

class UObject;
class UPackage;
class UPrimaryAssetLabel;
class UAEWModdingSettings;

namespace AEWPackaging
{
	AEWMODDINGCORE_API const UAEWModdingSettings* Settings();

	AEWMODDINGCORE_API int32 ChooseLabelsToCook();

	AEWMODDINGCORE_API int32 NextFreeChunkId();

	AEWMODDINGCORE_API int32 ChunkForLabel(const FString& LabelPackage);

	AEWMODDINGCORE_API UPrimaryAssetLabel* UpdateLabel(const FString& LabelPackage, int32 ChunkId, const TArray<UObject*>& Assets,
		bool bLabelDirectory, bool bRecursive, TArray<UPackage*>& ToSave, TArray<FString>* Log = nullptr);

	AEWMODDINGCORE_API UPrimaryAssetLabel* LabelSharedTables(const TArray<UObject*>& Objects, TArray<UPackage*>& ToSave, TArray<FString>* Log = nullptr);

	AEWMODDINGCORE_API int32 RebuildTableLabels(TArray<FString>& Log);

	AEWMODDINGCORE_API int32 CookOnlyLabelledContent(TArray<FString>& Log, int32* OutMissing = nullptr);

	AEWMODDINGCORE_API int32 ClearUnlabelledChunks(bool bAsk, TArray<FString>& Log);

	AEWMODDINGCORE_API int32 CollectLabelledPackages(TArray<FString>& OutPackages);

	AEWMODDINGCORE_API bool AllowCooking(const FString& PackageName, TArray<FString>* Log = nullptr);

	AEWMODDINGCORE_API void AllowCookingOf(const TArray<UObject*>& Objects, TArray<FString>* Log = nullptr);
}
