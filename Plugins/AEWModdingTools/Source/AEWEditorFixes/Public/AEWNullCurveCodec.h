#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimCurveCompressionCodec.h"
#include "AEWNullCurveCodec.generated.h"

UCLASS(hidecategories = Object)
class AEWEDITORFIXES_API UAEWNullCurveCodec : public UAnimCurveCompressionCodec
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	virtual bool Compress(const FCompressibleAnimData& AnimSeq, FAnimCurveCompressionResult& OutResult) override
	{
		OutResult.Codec = this;
		return true;
	}
#endif
	virtual void DecompressCurves(const FCompressedAnimSequence& AnimSeq, FBlendedCurve& Curves, float CurrentTime) const override {}
	virtual float DecompressCurve(const FCompressedAnimSequence& AnimSeq, SmartName::UID_Type CurveUID, float CurrentTime) const override { return 0.0f; }
};
