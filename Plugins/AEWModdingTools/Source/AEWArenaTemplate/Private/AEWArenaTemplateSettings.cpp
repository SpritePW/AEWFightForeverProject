#include "AEWArenaTemplateSettings.h"

UAEWArenaTemplateSettings::UAEWArenaTemplateSettings()
{
	ArenaEnum = FSoftObjectPath(TEXT("/Game/GameData/Assets/Enum/Arena/EArenaID.EArenaID"));
	ArenaTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Arena/DT_ArenaOriginal.DT_ArenaOriginal"));
	ArenaNewTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Arena/New/DT_ArenaOriginal_New.DT_ArenaOriginal_New"));
	UnlockTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Unlockable/New/DT_UnlockableItemParam_Arena.DT_UnlockableItemParam_Arena"));
	BaseUnlockTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Unlockable/DT_UnlockableItemParam.DT_UnlockableItemParam"));
	UnlockTemplateRow = TEXT("Item9000120");
	TemplateRoot.Path = TEXT("/Game/GameData/Levels/TemplateLevel");
	DefaultParentFolder.Path = TEXT("/Game/GameData/Levels/ModLevels");
}
