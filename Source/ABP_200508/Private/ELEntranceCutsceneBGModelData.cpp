#include "ELEntranceCutsceneBGModelData.h"

FELEntranceCutsceneBGModelData::FELEntranceCutsceneBGModelData() {
    this->GroupId = 0;
    this->EnabledCapture = false;
    this->CameraIndexForCapture = 0;
    this->LightIndexForCapture = 0;
    this->WaitTimeForCapture = 0.00f;
    this->UnlockItemId = 0;
    this->Location = FVector::ZeroVector;
    this->Rotation = FRotator::ZeroRotator;
    this->Scale = FVector::ZeroVector;
}

