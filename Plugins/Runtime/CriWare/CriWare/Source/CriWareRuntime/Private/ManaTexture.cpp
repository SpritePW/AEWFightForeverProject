#include "ManaTexture.h"

UManaTexture::UManaTexture() {
    this->MovieSource = NULL;
    this->bRenderToTexture = false;
    this->TargetGamma = 0.00f;
    this->bHDR = false;
    this->AddressX = TA_Clamp;
    this->AddressY = TA_Clamp;
    this->bOverrideDimensions = false;
    this->OverrideFormat = PF_Unknown;
}

void UManaTexture::SetMovieSource(UManaSource* InSource) {
}



FTextureResource* UManaTexture::CreateResource() {
    return nullptr;
}

EMaterialValueType UManaTexture::GetMaterialType() const {
    return MCT_Texture2D;
}

float UManaTexture::GetSurfaceWidth() const {
    return (bOverrideDimensions && TargetDimensions.X > 0) ? (float)TargetDimensions.X : 1.0f;
}

float UManaTexture::GetSurfaceHeight() const {
    return (bOverrideDimensions && TargetDimensions.Y > 0) ? (float)TargetDimensions.Y : 1.0f;
}
