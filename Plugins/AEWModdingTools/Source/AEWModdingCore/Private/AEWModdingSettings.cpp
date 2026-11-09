#include "AEWModdingSettings.h"

UAEWModdingSettings::UAEWModdingSettings()
{
	NewTablesLabel = TEXT("/Game/Mods/PAL_AEWNewTables");
	BaseTablesLabel = TEXT("/Game/Mods/PAL_AEWBaseTables");
	FDirectoryPath Tables;
	Tables.Path = TEXT("/Game/GameData/Assets/DataTable");
	TableFolders.Add(Tables);
}
