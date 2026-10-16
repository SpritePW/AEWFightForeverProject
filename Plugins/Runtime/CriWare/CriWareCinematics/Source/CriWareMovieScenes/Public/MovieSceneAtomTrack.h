/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Movie Scene Atom Track
 * File     : MovieSceneAtomTrack.h
 *
 ****************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneNameableTrack.h"
#include "Runtime/Launch/Resources/Version.h"
#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
#include "Compilation/IMovieSceneTrackTemplateProducer.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#endif

#include "SoundAtomCue.h"

#include "MovieSceneAtomTrack.generated.h"


namespace AtomTrackConstants {
	/* シーケンサエディタでカーソルを移動させた際のプレビュー時間	*/
	/* 備考）ストリーム再生のバッファリング時間を考慮し、			*/
	/* 　　　UE4標準サウンドよりもプレビュー時間を長めに設定。		*/
	const float ScrubDuration = 0.200f;
}

/**
 * Handles manipulation of audio.
 */
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 25
 /* UE4.25以下のバージョンでコンパイルを成功させるためだけの空クラス */
class IMovieSceneTrackTemplateProducer
{};
#endif

/**
 * Handles manipulation of audio.
 */
UCLASS(MinimalAPI)
class UMovieSceneAtomTrack
	: public UMovieSceneNameableTrack
	, public IMovieSceneTrackTemplateProducer
{
	GENERATED_UCLASS_BODY()

public:

	/** Adds a new sound cue to the audio */
	virtual UMovieSceneSection* AddNewSoundOnRow(USoundAtomCue* Sound, FFrameNumber Time, int32 RowIndex);

	/** Adds a new sound cue on the next available/non-overlapping row */
	virtual UMovieSceneSection* AddNewSound(USoundAtomCue* Sound, FFrameNumber Time) { return AddNewSoundOnRow(Sound, Time, INDEX_NONE); }

	/** @return The audio sections on this track */
	const TArray<UMovieSceneSection*>& GetAtomSections() const
	{
		return AtomSections;
	}

	/** @return true if this is a master audio track */
	bool IsAMasterTrack() const;

public:
	// UMovieSceneTrack interface
	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool SupportsMultipleRows() const override;
	virtual FMovieSceneTrackRowSegmentBlenderPtr GetRowSegmentBlender() const override;

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;

	// ~IMovieSceneTrackTemplateProducer interface
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
#endif

private:
	/** List of all master audio sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> AtomSections;

#if WITH_EDITORONLY_DATA
public:
	/**
	 * Get the height of this track's rows
	 */
	int32 GetRowHeight() const
	{
		return RowHeight;
	}

	/**
	 * Set the height of this track's rows
	 */
	void SetRowHeight(int32 NewRowHeight)
	{
		RowHeight = FMath::Max(16, NewRowHeight);
	}

private:
	/** The height for each row of this track */
	UPROPERTY()
	int32 RowHeight;
#endif
};
