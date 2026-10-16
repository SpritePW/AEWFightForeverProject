#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_FileManaMovie : public FAssetTypeActions_Base
{
public:
	explicit FAssetTypeActions_FileManaMovie(uint32 InCategories) : Categories(InCategories) {}

	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_FileManaMovie", "File Mana Movie"); }
	virtual FColor GetTypeColor() const override { return FColor(230, 150, 30); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return Categories; }

private:
	uint32 Categories;
};

class FAssetTypeActions_ManaTexture : public FAssetTypeActions_Base
{
public:
	explicit FAssetTypeActions_ManaTexture(uint32 InCategories) : Categories(InCategories) {}

	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ManaTexture", "Mana Texture"); }
	virtual FColor GetTypeColor() const override { return FColor(230, 200, 30); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return Categories; }

private:
	uint32 Categories;
};
