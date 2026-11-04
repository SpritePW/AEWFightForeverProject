#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AcbEditCommandlet.generated.h"

UCLASS()
class UAcbEditCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAcbEditCommandlet();
	virtual int32 Main(const FString& Params) override;
};
