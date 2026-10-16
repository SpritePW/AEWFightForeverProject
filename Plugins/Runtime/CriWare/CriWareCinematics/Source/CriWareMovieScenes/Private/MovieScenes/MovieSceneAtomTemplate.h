/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Movie Scene Atom Template
 * File     : MovieSceneAtomTemplate.h
 *
 ****************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
// #include "AtomComponent.h"
// #include "Sound/SoundAttenuation.h"
// #include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneAtomTemplate.generated.h"

class UAtomComponent;
class UMovieSceneAtomSection;
class USoundAtomCue;

USTRUCT()
struct FMovieSceneAtomSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	FMovieSceneAtomSectionTemplate();
	FMovieSceneAtomSectionTemplate(const UMovieSceneAtomSection& Section);

	UPROPERTY()
	const UMovieSceneAtomSection* AtomSection;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
