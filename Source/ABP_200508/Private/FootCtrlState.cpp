#include "FootCtrlState.h"

FFootCtrlState::FFootCtrlState() {
    this->bCheck = false;
    this->bHit = false;
    this->Location = FVector::ZeroVector;
    this->PrevLocation = FVector::ZeroVector;
}

