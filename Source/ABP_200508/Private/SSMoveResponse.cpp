#include "SSMoveResponse.h"

FSSMoveResponse::FSSMoveResponse() {
    this->Timestamp = 0.00f;
    this->Yaw = 0.00f;
    this->MovementBaseActor = NULL;
    this->Location = FVector::ZeroVector;
    this->Velo = FVector::ZeroVector;
}

