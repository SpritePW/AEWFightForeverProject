#include "SSProjectileHitInfo.h"

FSSProjectileHitInfo::FSSProjectileHitInfo() {
    this->HitTime = 0.00f;
    this->Location = FVector::ZeroVector;
    this->ImpactLocation = FVector::ZeroVector;
    this->ImpactPower = FVector::ZeroVector;
}

