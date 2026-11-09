#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AEWArenaMakerSettings.generated.h"

UENUM()
enum class EAEWCrowdType : uint8
{
	ByDistance UMETA(DisplayName = "Distance"),
	People UMETA(DisplayName = "People"),
	Billboards UMETA(DisplayName = "Billboards")
};

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Arena Maker"))
class AEWARENAMAKER_API UAEWArenaMakerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAEWArenaMakerSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "UserDefinedEnum", DisplayName = "Arena ID Enum", ToolTip = "Enum of arena IDs (EArenaID)."))
	FSoftObjectPath ArenaEnum;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "DataTable", DisplayName = "Arena Table", ToolTip = "Arena table the game reads (DT_ArenaOriginal)."))
	FSoftObjectPath ArenaTable;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (AllowedClasses = "DataTable", DisplayName = "New Arena Rows Table", ToolTip = "New arena rows go here. Must be a parent of Arena Table."))
	FSoftObjectPath ArenaNewTable;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (DisplayName = "Display Name Via Enum Text", ToolTip = "Give arenas a localizable display name without a string table."))
	bool bLocalizedDisplayName = true;

	UPROPERTY(EditAnywhere, config, Category = "Arena List", meta = (DisplayName = "Display Name Namespace", EditCondition = "bLocalizedDisplayName"))
	FString DisplayNameNamespace = TEXT("AEWModArena");

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (DisplayName = "Create Unlock Row", ToolTip = "Give each new arena its own default-unlocked unlock row."))
	bool bCreateUnlock = true;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (AllowedClasses = "DataTable", DisplayName = "Unlock Table", ToolTip = "New unlock rows go here. Must be a parent of Base Unlock Table."))
	FSoftObjectPath UnlockTable;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (AllowedClasses = "DataTable", DisplayName = "Base Unlock Table", ToolTip = "Composite unlock table the game reads."))
	FSoftObjectPath BaseUnlockTable;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (DisplayName = "Unlock Template Row", ToolTip = "Unlock row copied when the template arena has none."))
	FName UnlockTemplateRow;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (DisplayName = "First Unlock ID"))
	int32 FirstUnlockID = 10100000;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks", meta = (DisplayName = "Last Unlock ID"))
	int32 LastUnlockID = 10199999;

	UPROPERTY(EditAnywhere, config, Category = "Removing", meta = (DisplayName = "Update Tables When A Map Is Deleted", ToolTip = "Remove an arena's rows and enum entry when its map is deleted."))
	bool bSyncOnDelete = true;

	UPROPERTY(EditAnywhere, config, Category = "Removing", meta = (DisplayName = "Renumber Arena IDs", ToolTip = "Renumber mod arenas to close gaps."))
	bool bRenumberOnDelete = true;

	UPROPERTY(EditAnywhere, config, Category = "Level Blueprints", meta = (DisplayName = "Update Level Names In Level Blueprints", ToolTip = "Update map names in Level Blueprints when maps are copied or renamed."))
	bool bFixLevelBlueprints = true;

	UPROPERTY(EditAnywhere, config, Category = "Shared Sublevels", meta = (DisplayName = "Keep Shared", ToolTip = "Sublevels that stay shared instead of being copied."))
	TArray<FString> KeepSharedSublevels;

	UPROPERTY(EditAnywhere, config, Category = "Barricades", meta = (DisplayName = "Barricade Class", ToolTip = "Game barricade piece Make Barricade uses.", MetaClass = "Actor"))
	FSoftClassPath BarricadeClass = FSoftClassPath(TEXT("/Game/GameData/Resources/Models/EmvData/Arean/Common/Barricade/0000_Base/BP_Barricade.BP_Barricade_C"));

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Seat Gap", ClampMin = "0", ToolTip = "Gap between people in a row (cm). 0 = spread evenly."))
	float AudienceSeatSpacing = 70.f;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Row Gap", ClampMin = "0", ToolTip = "Gap between rows (cm). 0 = spread evenly."))
	float AudienceRowSpacing = 90.f;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Per Row", ClampMin = "1", ToolTip = "People in each row."))
	int32 AudiencePerRow = 10;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Rows", ClampMin = "1", ToolTip = "Number of rows."))
	int32 AudienceRows = 5;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Curve", ClampMin = "-270", ClampMax = "270", Units = "Degrees", ToolTip = "Bend rows into an arc (degrees). 0 = straight."))
	float AudienceRowCurve = 0.f;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Fill", ClampMin = "0.05", ClampMax = "1", ToolTip = "Share of spots that get a person."))
	float AudienceFill = 0.85f;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Straightness", ClampMin = "0", ClampMax = "1", ToolTip = "1 = in line; lower scatters people."))
	float AudienceStraightness = 0.8f;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Max Slope", ClampMin = "0", ClampMax = "60", Units = "Degrees", ToolTip = "Steepest surface people stand on."))
	float AudienceMaxSlope = 30.f;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Type", ToolTip = "Distance: people near the ring, billboards further out."))
	EAEWCrowdType AudienceCrowdType = EAEWCrowdType::ByDistance;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (DisplayName = "Billboards From", ClampMin = "0", ToolTip = "Beyond this distance from the ring (cm), use billboards. 0 = never."))
	float AudienceBillboardDistance = 0.f;

	UPROPERTY(EditAnywhere, config, Category = "Audience", meta = (AllowedClasses = "World", DisplayName = "Crowd Source", ToolTip = "Level whose crowd is copied when the arena has none."))
	FSoftObjectPath AudienceTemplateLevel = FSoftObjectPath(TEXT("/Game/GameData/Levels/ArenaBase/Audience/ArenaAudience_01.ArenaAudience_01"));

	UPROPERTY(config)
	bool bExistingArenasConverted = false;

	UPROPERTY(EditAnywhere, config, Category = "Defaults", meta = (ContentDir, DisplayName = "Template Folder"))
	FDirectoryPath TemplateRoot;

	UPROPERTY(EditAnywhere, config, Category = "Defaults", meta = (ContentDir, DisplayName = "Parent Folder For New Arenas"))
	FDirectoryPath DefaultParentFolder;

	UPROPERTY(EditAnywhere, config, Category = "Arena Overrides", meta = (AllowedClasses = "DataTable", DisplayName = "Game Camera Table", ToolTip = "Gameplay camera table (DT_GameplayCameraParam)."))
	FSoftObjectPath GameCameraTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/DT_GameplayCameraParam.DT_GameplayCameraParam"));

	UPROPERTY(EditAnywhere, config, Category = "Arena Overrides", meta = (DisplayName = "Camera Preview Row", ToolTip = "Camera table row Toggle Camera Preview shows."))
	FName CameraPreviewRow = TEXT("Ring_NM");

};
