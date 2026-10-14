#pragma once
#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "ManaAssetFactories.generated.h"

UCLASS(HideCategories=Object)
class UFileManaMovieImportFactory : public UFactory {
	GENERATED_BODY()
public:
	UFileManaMovieImportFactory();

	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};

UCLASS(HideCategories=Object)
class UFileManaMovieFactory : public UFactory {
	GENERATED_BODY()
public:
	UFileManaMovieFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

UCLASS(HideCategories=Object)
class UManaTextureFactory : public UFactory {
	GENERATED_BODY()
public:
	UManaTextureFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
