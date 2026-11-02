#include "ManaComponentTexture.h"

UManaComponentTexture::UManaComponentTexture() {
    this->NeverStream = true;
    this->SizeX = 0;
    this->SizeY = 0;
    this->Format = PF_G8;
    this->ComponentType = EManaComponentTextureType::Texture_Y;
    this->AddressX = TA_Wrap;
    this->AddressY = TA_Wrap;
}



FTextureResource* UManaComponentTexture::CreateResource() {
    return nullptr;
}

EMaterialValueType UManaComponentTexture::GetMaterialType() const {
    return MCT_Texture2D;
}

float UManaComponentTexture::GetSurfaceWidth() const {
    return SizeX > 0 ? (float)SizeX : 1.0f;
}

float UManaComponentTexture::GetSurfaceHeight() const {
    return SizeY > 0 ? (float)SizeY : 1.0f;
}
