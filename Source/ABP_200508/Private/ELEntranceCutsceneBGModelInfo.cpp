#include "ELEntranceCutsceneBGModelInfo.h"

FELEntranceCutsceneBGModelInfo::FELEntranceCutsceneBGModelInfo() {
    this->SkeletalMesh = NULL;
    this->AnimSequence = NULL;
    this->StaticMesh = NULL;
    this->Blueprint = NULL;
    this->Location = FVector::ZeroVector;
    this->Rotation = FRotator::ZeroRotator;
    this->Scale = FVector::ZeroVector;
}

