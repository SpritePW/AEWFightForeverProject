/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Movie Scene Atom Section
 * File     : MovieSceneAtomSection.h
 *
 ****************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Sections/MovieSceneActorReferenceSection.h"
#include "Sections/MovieSceneParameterSection.h"

#include "SoundAtomCue.h"
#include "AtomComponent.h"

#include "MovieSceneAtomSection.generated.h"


struct FAtomSelectorParam;

/**
 * Atom section, for use in the master audio, or by attached audio objects
 */
UCLASS()
class CRIWAREMOVIESCENES_API UMovieSceneAtomSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:
	/** Sets this section's sound */
	void SetSound(class USoundAtomCue* InSound) { Sound = InSound; }

	/** Gets the sound for this section */
	class USoundAtomCue* GetSound() const { return Sound; }

	/** Set the offset into the beginning of the audio clip */
	UFUNCTION(BlueprintCallable, Category = "Movie Scene")
	void SetStartOffset(FFrameNumber InStartOffset) { StartFrameOffset = InStartOffset;}

	/** Get the offset into the beginning of the audio clip */
	UFUNCTION(BlueprintPure, Category = "Movie Scene")
	FFrameNumber GetStartOffset() const {return StartFrameOffset;}

	/** Get sound loop settings */
	UFUNCTION(BlueprintPure, Category = "Movie Scene")
	bool IsLooping() const { return bLooping; }

	/**
	 * Gets the sound volume curve
	 *
	 * @return The rich curve for this sound volume
	 */
	const FMovieSceneFloatChannel& GetSoundVolumeChannel() const { return SoundVolume; }

	/**
	 * Gets the sound pitch curve
	 *
	 * @return The rich curve for this sound pitch
	 */
	const FMovieSceneFloatChannel& GetPitchMultiplierChannel() const { return PitchMultiplier; }

	/**
	 * Return the sound volume
	 *
	 * @param InTime	The position in time within the movie scene
	 * @return The volume the sound will be played with.
	 */
	float GetSoundVolume(FFrameTime InTime) const
	{
		float OutValue = 0.f;
		SoundVolume.Evaluate(InTime, OutValue);
		return OutValue;
	}

	/**
	 * Return the pitch multiplier
	 *
	 * @param Position	The position in time within the movie scene
	 * @return The pitch multiplier the sound will be played with.
	 */
	float GetPitchMultiplier(FFrameTime InTime) const
	{
		float OutValue = 0.f;
		PitchMultiplier.Evaluate(InTime, OutValue);
		return OutValue;
	}

	/**
	 * @return Whether subtitles should be suppressed
	 */
	bool GetSuppressSubtitles() const
	{
		return bSuppressSubtitles;
	}

	/**
	 * @return Whether override settings on this section should be used
	 */
	bool GetOverrideAttenuation() const
	{
		return bOverrideAttenuation;
	}

	/**
	 * @return The attenuation settings
	 */
	USoundAttenuation* GetAttenuationSettings() const
	{
		return AttenuationSettings;
	}

	/**
	 * @return The attach actor data
	 */
	const FMovieSceneActorReferenceData& GetAttachActorData() const { return AttachActorData; }

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
	/*
	 * @return The attach component given the bound actor and the actor attach key with the component and socket names
	 */
	USceneComponent* GetAttachComponent(const AActor* InParentActor, const FMovieSceneActorReferenceKey& Key) const;
#endif

	/**
	 * @return Whether Playing Sound should be continued When Sequence Is End
	 */
	bool GetContinueSoundWhenSequenceIsEnd() const { return bContinueSoundWhenSequenceIsEnd; }

	TArray<int32> GetASRRackIDs() const { return ASRRackIDs; }

	TArray<struct FAtomSelectorParam> GetSelectorLabels() const { return SelectorLabels; }

	/** Gets the animated scalar parameters and their associated curves. */
	TArray<FScalarParameterNameAndCurve>& GetScalarParameterNamesAndCurves();
	const TArray<FScalarParameterNameAndCurve>& GetScalarParameterNamesAndCurves() const;

	FFrameNumber GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, FFrameNumber StartOffset, FFrameNumber StartFrame);

public:
	// MovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber>> GetAutoSizeRange() const override;
#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 24) || ENGINE_MAJOR_VERSION == 5
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys) override;
#else
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime) override;
#endif
	virtual TOptional<FFrameTime> GetOffsetTime() const override;
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 25	
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;
#endif

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
	virtual EMovieSceneChannelProxyType CacheChannelProxy() override;
#else
	void CacheChannelProxy();
#endif

	//~ UObject interface
	virtual void Serialize(FArchive& Ar) override;

	/** Functions setup a currently used AtomComponent while evaluating. */
	FORCEINLINE void SetAtomComponent(UAtomComponent* InAtomComponent) { AtomComponent = InAtomComponent; }

	/** Functions to access currently used AtomComponent while evaluating (may be null). */
	FORCEINLINE UAtomComponent* GetAtomComponent() { return AtomComponent.Get(); }

private:
	/** The sound cue or wave that this section plays. */
	UPROPERTY(EditAnywhere, Category="Atom")
	USoundAtomCue* Sound;

	/** The offset into the beginning of the audio clip. */
	UPROPERTY(EditAnywhere, Category="Atom")
	FFrameNumber StartFrameOffset;

	/** The volume the sound will be played with. */
	UPROPERTY()
	FMovieSceneFloatChannel SoundVolume;

	/** The pitch multiplier the sound will be played with. */
	UPROPERTY()
	FMovieSceneFloatChannel PitchMultiplier;

	UPROPERTY()
	FMovieSceneActorReferenceData AttachActorData;

	/** The scalar parameter names and their associated curves. */
	UPROPERTY()
	TArray<FScalarParameterNameAndCurve> ScalarParameterNamesAndCurves;

	UPROPERTY(EditAnywhere, Category="Atom")
	bool bSuppressSubtitles;

	/** Should the attenuation settings on this section be used. */
	UPROPERTY(EditAnywhere, Category = "Attenuation")
	bool bOverrideAttenuation;

	/** The attenuation settings to use. */
	UPROPERTY(EditAnywhere, Category = "Attenuation")
	class USoundAttenuation* AttenuationSettings;

	/* シーケンサー終了後も再生が継続するよう設定するためのフラグ */
	UPROPERTY(EditAnywhere, Category = "Atom")
	bool bContinueSoundWhenSequenceIsEnd;

	/* サウンド再生時に設定するASRRack */
	UPROPERTY(EditAnywhere, Category = "Atom")
	TArray<int32> ASRRackIDs;

	/* サウンド再生時に設定するセレクタラベル */
	UPROPERTY(EditAnywhere, Category = "Atom")
	TArray<struct FAtomSelectorParam> SelectorLabels;

	/** サウンドのループ設定 */
	UPROPERTY(EditAnywhere, Category = "Atom")
	bool bLooping;

	/** helper to access AtomComponemt used by this section. */
	TWeakObjectPtr<UAtomComponent> AtomComponent;
};
