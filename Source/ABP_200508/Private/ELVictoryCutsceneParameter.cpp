#include "ELVictoryCutsceneParameter.h"

FELVictoryCutsceneParameter::FELVictoryCutsceneParameter() {
    this->MatchType = EELVictoryCutsceneTransformAndAnimationMatchType::None;
    this->WinnerLocation = FVector::ZeroVector;
    this->WinnerRotation = FRotator::ZeroRotator;
}

