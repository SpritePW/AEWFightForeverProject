#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EditPartMakerSettings.generated.h"

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "AEW Edit Part Maker"))
class AEWEDITPARTMAKER_API UEditPartMakerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEditPartMakerSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Parts Table", ToolTip = "New part rows are written here. Must be a parent of the Parts List Table."))
	FSoftObjectPath PartsTable;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Parts List Table", ToolTip = "The composite table the game reads. Used to find used IDs."))
	FSoftObjectPath PartsListTable;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Unlock Table", ToolTip = "New unlock rows are written here. Must be a parent of the Base Unlock Table."))
	FSoftObjectPath UnlockTable;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Base Unlock Table", ToolTip = "The composite unlock table the game reads. Used to find used unlock IDs and the template row."))
	FSoftObjectPath BaseUnlockTable;

	UPROPERTY(EditAnywhere, config, Category = "Tables", meta = (AllowedClasses = "DataTable", DisplayName = "Category Table", ToolTip = "Categories offered for the WA."))
	FSoftObjectPath CategoryTable;

	UPROPERTY(EditAnywhere, config, Category = "Unlocks")
	FName UnlockTemplateRow;

	UPROPERTY(EditAnywhere, config, Category = "IDs")
	int32 FirstPartsID = 900000;

	UPROPERTY(EditAnywhere, config, Category = "IDs")
	int32 FirstUnlockID = 20000000;

	UPROPERTY(EditAnywhere, config, Category = "IDs")
	int32 LastUnlockID = 29999999;

	UPROPERTY(EditAnywhere, config, Category = "Defaults", meta = (AllowedClasses = "Texture2D"))
	FSoftObjectPath DefaultThumbnail;

	UPROPERTY(EditAnywhere, config, Category = "Defaults", meta = (DisplayName = "Save Changes Automatically", ToolTip = "Save the parts asset and both tables as soon as a part is made. Turn off to review the changes and save them yourself."))
	bool bSaveChangesAutomatically = true;
};
