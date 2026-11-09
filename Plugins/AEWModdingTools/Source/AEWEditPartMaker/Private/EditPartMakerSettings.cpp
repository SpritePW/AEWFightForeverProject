#include "EditPartMakerSettings.h"

UEditPartMakerSettings::UEditPartMakerSettings()
{
	PartsTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Customize/DT_EditParts_New.DT_EditParts_New"));
	PartsListTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Customize/DT_EditPartsList.DT_EditPartsList"));
	UnlockTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Unlockable/DT_UnlockableItemParam_WrestlerParts.DT_UnlockableItemParam_WrestlerParts"));
	BaseUnlockTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Unlockable/DT_UnlockableItemParam.DT_UnlockableItemParam"));
	CategoryTable = FSoftObjectPath(TEXT("/Game/GameData/Assets/DataTable/Customize/DT_PartsCategory.DT_PartsCategory"));
	UnlockTemplateRow = TEXT("Item3200000");
	DefaultThumbnail = FSoftObjectPath(TEXT("/Game/GameData/Resources/UI/Menu/Custom/_Common/T_Thumbnail_None.T_Thumbnail_None"));
}
