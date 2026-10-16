#include "AssetTypeActions_ManaAsset.h"
#include "FileManaMovie.h"
#include "ManaTexture.h"

UClass* FAssetTypeActions_FileManaMovie::GetSupportedClass() const
{
	return UFileManaMovie::StaticClass();
}

UClass* FAssetTypeActions_ManaTexture::GetSupportedClass() const
{
	return UManaTexture::StaticClass();
}
