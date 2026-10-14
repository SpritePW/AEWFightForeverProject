#include "ELAttachCondition.h"

FELAttachCondition::FELAttachCondition() {
    this->AxisCondition = 0;
    this->SocketOffset = FVector::ZeroVector;
    this->AttachOffset = FVector::ZeroVector;
}

