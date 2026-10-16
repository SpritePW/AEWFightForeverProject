#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CustomizeAssetBase.generated.h"

UCLASS(Abstract, Blueprintable)
class CREATION_API UCustomizeAssetBase : public UDataAsset {
    GENERATED_BODY()
public:
    UCustomizeAssetBase();

};

