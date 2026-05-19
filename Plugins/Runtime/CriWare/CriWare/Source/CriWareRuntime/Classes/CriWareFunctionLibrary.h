/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Blueprint Function Library
 * File     : CriWareFunctionLibrary.h
 *
 ****************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CriWareFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class ECriWareSpeakerSystem : uint8
{
	Surround5Point1 = 0,	/** 5.1ch */
	Surround7Point1 = 1		/** 7.1ch */
};

UCLASS()
class CRIWARERUNTIME_API UCriWareFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/* スピーカー角度の設定 */
	UFUNCTION(BlueprintCallable, Category="CriWareFunctionLibrary")
	static void SetSpeakerAngleArray(ECriWareSpeakerSystem SpeakerSystem, const TArray<float>& Angles);

	/* ゲーム変数の設定 */
	UFUNCTION(BlueprintCallable, Category="CriWareFunctionLibrary")
	static void SetGameVariableByName(FString GameVariableName, float Value);

	/* セレクタに対するグローバル参照ラベルの設定 */
	UFUNCTION(BlueprintCallable, Category="CriWareFunctionLibrary")
	static void SetGlobalLabelToSelectorByName(FString SelectorName, FString LabelName);

	/** Search in Asset Manager all SoundAtomCue that uses the given SoundAtomCueSheet and cue name. */
	UFUNCTION(BlueprintCallable, Category = "CriWareFunctionLibrary")
	static TArray<USoundAtomCue*> SearchAtomCuesInAssetRegistry(const USoundAtomCueSheet* CueSheet);
};
