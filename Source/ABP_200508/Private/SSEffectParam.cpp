#include "SSEffectParam.h"

FSSEffectParam::FSSEffectParam() {
    this->EffectClass = NULL;
    this->IsAttach = false;
    this->SoundCue3D = NULL;
    this->EffectOffset = FVector::ZeroVector;
    this->EffectRotation = FRotator::ZeroRotator;
    this->EffectScale = FVector::ZeroVector;
}

