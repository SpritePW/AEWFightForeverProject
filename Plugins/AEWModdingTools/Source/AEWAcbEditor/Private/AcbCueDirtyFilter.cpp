#include "AcbCueDirtyFilter.h"
#include "AcbEditorSession.h"
#include "AcbEditorSettings.h"
#include "Containers/Ticker.h"
#include "Misc/PackageName.h"
#include "SoundAtomCue.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

FAcbCueDirtyFilter& FAcbCueDirtyFilter::Get()
{
	static FAcbCueDirtyFilter Instance;
	return Instance;
}

void FAcbCueDirtyFilter::Startup()
{
	DirtyHandle = UPackage::PackageMarkedDirtyEvent.AddRaw(this, &FAcbCueDirtyFilter::OnPackageMarkedDirty);
	LoadedHandle = FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FAcbCueDirtyFilter::OnAssetLoaded);
	SavedHandle = UPackage::PackageSavedEvent.AddRaw(this, &FAcbCueDirtyFilter::OnPackageSaved);

	for (TObjectIterator<USoundAtomCue> It; It; ++It)
	{
		if (!It->HasAnyFlags(RF_ClassDefaultObject) && !It->GetOutermost()->IsDirty())
		{
			Remember(*It);
		}
	}
}

void FAcbCueDirtyFilter::Shutdown()
{
	UPackage::PackageMarkedDirtyEvent.Remove(DirtyHandle);
	FCoreUObjectDelegates::OnAssetLoaded.Remove(LoadedHandle);
	UPackage::PackageSavedEvent.Remove(SavedHandle);
	if (TickHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	SavedState.Reset();
	Pending.Reset();
}

USoundAtomCue* FAcbCueDirtyFilter::FindCue(UPackage* Package)
{
	if (!Package)
	{
		return nullptr;
	}
	return Cast<USoundAtomCue>(StaticFindObjectFastSafe(USoundAtomCue::StaticClass(), Package, FName(*FPackageName::GetShortName(Package))));
}

FString FAcbCueDirtyFilter::Fingerprint(const USoundAtomCue* Cue)
{
	static const TSet<FName> CacheOnly = {
		TEXT("Duration"), TEXT("FirstWaveDuration"), TEXT("FirstWaveInfo"),
		TEXT("CategoryName"), TEXT("AisacControlName"), TEXT("AttenuationDistanceParam"), TEXT("UserData") };

	const UPackage* Package = Cue->GetOutermost();
	FString Out = Package->GetName();
	Out += TEXT("|chunks=");
	for (int32 Chunk : Package->GetChunkIDs())
	{
		Out += FString::FromInt(Chunk) + TEXT(",");
	}
	for (TFieldIterator<FProperty> It(Cue->GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_Transient) || CacheOnly.Contains(Property->GetFName()))
		{
			continue;
		}
		for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
		{
			FString Value;
			Property->ExportTextItem(Value, Property->ContainerPtrToValuePtr<void>(Cue, Index), nullptr, const_cast<USoundAtomCue*>(Cue), PPF_None);
			Out += TEXT("|");
			Out += Property->GetName();
			Out += TEXT("=");
			Out += Value;
		}
	}
	return Out;
}

void FAcbCueDirtyFilter::Remember(USoundAtomCue* Cue)
{
	if (Cue)
	{
		SavedState.Add(FObjectKey(Cue), Fingerprint(Cue));
	}
}

void FAcbCueDirtyFilter::OnAssetLoaded(UObject* Object)
{
	if (USoundAtomCue* Cue = Cast<USoundAtomCue>(Object))
	{
		Remember(Cue);
	}
}

void FAcbCueDirtyFilter::OnPackageSaved(const FString&, UObject* Outer)
{
	if (USoundAtomCue* Cue = FindCue(Cast<UPackage>(Outer)))
	{
		Remember(Cue);
	}
}

void FAcbCueDirtyFilter::OnPackageMarkedDirty(UPackage* Package, bool bWasDirty)
{
	if (bWasDirty || GIsSavingPackage || IsRunningCommandlet() || !GetDefault<UAcbEditorSettings>()->bIgnoreCueCacheRefresh)
	{
		return;
	}
	USoundAtomCue* Cue = FindCue(Package);
	if (!Cue || !SavedState.Contains(FObjectKey(Cue)))
	{
		return;
	}
	Pending.AddUnique(Package);
	if (!TickHandle.IsValid())
	{
		TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAcbCueDirtyFilter::OnTick), 0.0f);
	}
}

bool FAcbCueDirtyFilter::OnTick(float)
{
	if (GIsSavingPackage || GIsTransacting)
	{
		return true;
	}
	TickHandle.Reset();
	Flush();
	return false;
}

int32 FAcbCueDirtyFilter::Flush()
{
	TArray<TWeakObjectPtr<UPackage>> ToCheck = MoveTemp(Pending);
	Pending.Reset();
	int32 Cleaned = 0;
	for (const TWeakObjectPtr<UPackage>& Weak : ToCheck)
	{
		UPackage* Package = Weak.Get();
		if (!Package || !Package->IsDirty() || Package->HasAnyPackageFlags(PKG_NewlyCreated)
			|| !FPackageName::DoesPackageExist(Package->GetName()))
		{
			continue;
		}
		USoundAtomCue* Cue = FindCue(Package);
		const FString* Saved = Cue ? SavedState.Find(FObjectKey(Cue)) : nullptr;
		if (Saved && *Saved == Fingerprint(Cue))
		{
			Package->SetDirtyFlag(false);
			++Cleaned;
		}
	}
	if (Cleaned > 0)
	{
		UE_LOG(LogAEWAcbEditor, Log, TEXT("Ignored CriWare's automatic refresh of %d cue asset(s): only cached info changed, so they don't need saving."), Cleaned);
	}
	return Cleaned;
}
