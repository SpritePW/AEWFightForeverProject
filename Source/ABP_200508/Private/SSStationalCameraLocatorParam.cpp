#include "SSStationalCameraLocatorParam.h"

FSSStationalCameraLocatorParam::FSSStationalCameraLocatorParam() {
    this->LocatorId = 0;
    this->FocalLength = 0.00f;
    this->LookAtMode = ESSStationalCameraLookAt::None;
    this->TrackingInterpSpeed = 0.00f;
    this->Location = FVector::ZeroVector;
    this->Rotation = FRotator::ZeroRotator;
    this->LookAtOffset = FVector::ZeroVector;
    this->SatelliteMoveSpeed = FRotator::ZeroRotator;
}

