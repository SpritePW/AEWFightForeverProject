#include "Modules/ModuleManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimCurveCompressionSettings.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "AEWNullCurveCodec.h"

DEFINE_LOG_CATEGORY_STATIC(LogAEWAnimFix, Log, All);

namespace AEWEditorFixes
{
	static void GuardAnim(UAnimSequence* Seq)
	{
		if (!Seq || Seq->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			return;
		}
		FCompressedAnimSequence& Data = Seq->CompressedData;

		if (Data.CompressedTrackToSkeletonMapTable.Num() > 0 &&
			(Data.BoneCompressionCodec == nullptr || !Data.CompressedDataStructure.IsValid()))
		{
			UE_LOG(LogAEWAnimFix, Warning, TEXT("%s: bone compression codec not found (settings %s) - bone tracks disabled, it plays the reference pose"),
				*Seq->GetPathName(), *GetPathNameSafe(Seq->BoneCompressionSettings));
			Data.CompressedTrackToSkeletonMapTable.Empty();
			Data.CompressedDataStructure.Reset();
		}

		if (Data.CurveCompressionCodec == nullptr)
		{
			if (Data.CompressedCurveByteStream.Num() > 0 || Data.CompressedCurveNames.Num() > 0)
			{
				UE_LOG(LogAEWAnimFix, Warning, TEXT("%s: curve compression codec not found (settings %s) - curves disabled"),
					*Seq->GetPathName(), *GetPathNameSafe(Seq->CurveCompressionSettings));
			}
			Data.CurveCompressionCodec = GetMutableDefault<UAEWNullCurveCodec>();
		}
	}

	static void OnAssetLoaded(UObject* Obj)
	{
		if (UAnimSequence* Seq = Cast<UAnimSequence>(Obj))
		{
			GuardAnim(Seq);
		}
	}
}

class FAEWEditorFixesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		for (TObjectIterator<UAnimSequence> It; It; ++It)
		{
			AEWEditorFixes::GuardAnim(*It);
		}
		Handle = FCoreUObjectDelegates::OnAssetLoaded.AddStatic(&AEWEditorFixes::OnAssetLoaded);
	}

	virtual void ShutdownModule() override
	{
		FCoreUObjectDelegates::OnAssetLoaded.Remove(Handle);
	}

private:
	FDelegateHandle Handle;
};

IMPLEMENT_MODULE(FAEWEditorFixesModule, AEWEditorFixes)
