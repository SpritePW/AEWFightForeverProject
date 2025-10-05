#pragma once
#include "CoreMinimal.h"
#include "EOSTitleStorageBase.h"
#include "EOSTitleStorage.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDownloadJson, const FString&, JsonData);

UCLASS(Blueprintable)
class YEOSSDK_API UEOSTitleStorage : public UEOSTitleStorageBase {
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "EOS|TitleStorage")
    FOnDownloadJson OnDownloadJson;
    
    UEOSTitleStorage();

};

