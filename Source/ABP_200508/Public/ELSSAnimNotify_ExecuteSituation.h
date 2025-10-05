#pragma once
#include "CoreMinimal.h"
#include "ELSSAnimNotifyBase.h"
#include "ELSSAnimNotify_ExecuteSituation.generated.h"

class UAnimSequenceBase;
class USkeletalMeshComponent;

UCLASS(Blueprintable, CollapseCategories)
class ABP_200508_API UELSSAnimNotify_ExecuteSituation : public UELSSAnimNotifyBase {
    GENERATED_BODY()
public:
    UELSSAnimNotify_ExecuteSituation();

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

};

