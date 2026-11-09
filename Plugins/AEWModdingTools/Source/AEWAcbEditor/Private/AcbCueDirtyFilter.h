#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

class UPackage;
class USoundAtomCue;

class FAcbCueDirtyFilter
{
public:
	static FAcbCueDirtyFilter& Get();

	void Startup();
	void Shutdown();

	int32 Flush();

private:
	void OnPackageMarkedDirty(UPackage* Package, bool bWasDirty);
	void OnAssetLoaded(UObject* Object);
	void OnPackageSaved(const FString& Filename, UObject* Outer);
	bool OnTick(float);

	static USoundAtomCue* FindCue(UPackage* Package);
	static FString Fingerprint(const USoundAtomCue* Cue);
	void Remember(USoundAtomCue* Cue);

	TMap<FObjectKey, FString> SavedState;
	TArray<TWeakObjectPtr<UPackage>> Pending;
	FDelegateHandle DirtyHandle;
	FDelegateHandle LoadedHandle;
	FDelegateHandle SavedHandle;
	FDelegateHandle TickHandle;
};
