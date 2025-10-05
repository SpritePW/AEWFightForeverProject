#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ELDataAssetBase.generated.h"

UCLASS(Abstract, Blueprintable)
class ELITE_CORE_API UELDataAssetBase : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UELDataAssetBase();

};

