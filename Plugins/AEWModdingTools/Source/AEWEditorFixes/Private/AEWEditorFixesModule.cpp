#include "Modules/ModuleManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimCurveCompressionSettings.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "Engine/World.h"
#include "AEWNullCurveCodec.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/Package.h"
#include "Editor.h"
#include "Misc/CoreDelegates.h"
#include "EdGraphSchema_K2.h"

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
			UE_LOG(LogAEWAnimFix, Warning, TEXT("%s: bone codec not found (%s); plays the reference pose"),
				*Seq->GetPathName(), *GetPathNameSafe(Seq->BoneCompressionSettings));
			Data.CompressedTrackToSkeletonMapTable.Empty();
			Data.CompressedDataStructure.Reset();
		}

		if (Data.CurveCompressionCodec == nullptr)
		{
			if (Data.CompressedCurveByteStream.Num() > 0 || Data.CompressedCurveNames.Num() > 0)
			{
				UE_LOG(LogAEWAnimFix, Warning, TEXT("%s: curve codec not found (%s); curves disabled"),
					*Seq->GetPathName(), *GetPathNameSafe(Seq->CurveCompressionSettings));
			}
			Data.CurveCompressionCodec = GetMutableDefault<UAEWNullCurveCodec>();
		}
	}

	static void FixCookedStruct(UUserDefinedStruct* Struct)
	{
		if (!Struct || Struct->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			return;
		}
		if (UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Struct, false))
		{
			return;
		}
		const bool bWasAllowed = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Struct, true);
		const int32 OldStatus = (int32)Struct->Status.GetValue();
		if (Struct->Status != EUserDefinedStructureStatus::UDSS_UpToDate)
		{
			Struct->Status = EUserDefinedStructureStatus::UDSS_UpToDate;
		}
		if (!Struct->GetBoolMetaData(TEXT("BlueprintType")))
		{
			Struct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		}
		const bool bAllowed = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Struct, true);
		if (!bWasAllowed || !bAllowed)
		{
			UE_LOG(LogAEWAnimFix, Log, TEXT("cooked struct %s: status %d -> %d, BlueprintType meta '%s', allowed %d -> %d"),
				*Struct->GetPathName(), OldStatus, (int32)Struct->Status.GetValue(), *Struct->GetMetaData(TEXT("BlueprintType")), bWasAllowed ? 1 : 0, bAllowed ? 1 : 0);
		}
	}

	static void FixAllCookedStructs()
	{
		for (TObjectIterator<UUserDefinedStruct> It; It; ++It)
		{
			FixCookedStruct(*It);
		}
	}

	static void GuardAllAnims()
	{
		for (TObjectIterator<UAnimSequence> It; It; ++It)
		{
			GuardAnim(*It);
		}
	}

	static void OnPreWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
	{
		GuardAllAnims();
		FixAllCookedStructs();
	}

	static void OnBlueprintPreCompile(UBlueprint* Blueprint)
	{
		FixAllCookedStructs();
	}

	static void OnAssetLoaded(UObject* Obj)
	{
		if (UAnimSequence* Seq = Cast<UAnimSequence>(Obj))
		{
			GuardAnim(Seq);
		}
		else if (UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(Obj))
		{
			FixCookedStruct(Struct);
		}
	}
}

class FAEWEditorFixesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		AEWEditorFixes::GuardAllAnims();
		AEWEditorFixes::FixAllCookedStructs();
		Handle = FCoreUObjectDelegates::OnAssetLoaded.AddStatic(&AEWEditorFixes::OnAssetLoaded);
		WorldHandle = FWorldDelegates::OnPreWorldInitialization.AddStatic(&AEWEditorFixes::OnPreWorldInit);
		if (GEditor)
		{
			CompileHandle = GEditor->OnBlueprintPreCompile().AddStatic(&AEWEditorFixes::OnBlueprintPreCompile);
		}
		else
		{
			PostInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAEWEditorFixesModule::BindCompile);
		}
	}

	void BindCompile()
	{
		if (GEditor && !CompileHandle.IsValid())
		{
			CompileHandle = GEditor->OnBlueprintPreCompile().AddStatic(&AEWEditorFixes::OnBlueprintPreCompile);
		}
	}

	virtual void ShutdownModule() override
	{
		FCoreUObjectDelegates::OnAssetLoaded.Remove(Handle);
		FWorldDelegates::OnPreWorldInitialization.Remove(WorldHandle);
		FCoreDelegates::OnPostEngineInit.Remove(PostInitHandle);
		if (GEditor)
		{
			GEditor->OnBlueprintPreCompile().Remove(CompileHandle);
		}
	}

private:
	FDelegateHandle Handle;
	FDelegateHandle WorldHandle;
	FDelegateHandle CompileHandle;
	FDelegateHandle PostInitHandle;
};

IMPLEMENT_MODULE(FAEWEditorFixesModule, AEWEditorFixes)
