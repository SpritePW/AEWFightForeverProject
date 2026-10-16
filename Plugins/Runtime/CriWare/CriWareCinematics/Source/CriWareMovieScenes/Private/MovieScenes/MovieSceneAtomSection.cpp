/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Movie Scene Atom Section
 * File     : MovieSceneAtomSection.cpp
 *
 ****************************************************************************/

#include "MovieSceneAtomSection.h"

#include "Audio.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "UObject/SequencerObjectVersion.h"

#include "MovieSceneAtomTemplate.h"
#include "MovieSceneAtomTrack.h"


#if WITH_EDITOR
struct FAtomChannelEditorData
{
	FAtomChannelEditorData()
	{
		Data[0].SetIdentifiers("Volume", NSLOCTEXT("MovieSceneAtomSection", "SoundVolumeText", "Volume"));
		Data[1].SetIdentifiers("Pitch", NSLOCTEXT("MovieSceneAtomSection", "PitchText", "Pitch"));
		Data[2].SetIdentifiers("AttachActor", NSLOCTEXT("MovieSceneAtomSection", "AttachActorText", "Attach"));
	}

	FMovieSceneChannelMetaData Data[3];
};
#endif // WITH_EDITOR

UMovieSceneAtomSection::UMovieSceneAtomSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Sound = nullptr;
	SoundVolume.SetDefault(1.f);
	PitchMultiplier.SetDefault(1.f);
	ScalarParameterNamesAndCurves.Empty();
	bSuppressSubtitles = false;
	bOverrideAttenuation = false;
	bContinueSoundWhenSequenceIsEnd = false;
	ASRRackIDs.Empty();
	SelectorLabels.Empty();

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
	BlendType = EMovieSceneBlendType::Absolute;

	EvalOptions.EnableAndSetCompletionMode(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ?
		EMovieSceneCompletionMode::RestoreState :
		EMovieSceneCompletionMode::ProjectDefault);
#else
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
#endif
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION < 26
	CacheChannelProxy();
#endif
}

void UMovieSceneAtomSection::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading()) {
		/* エラー回避措置の開始 */
		/* 備考）bSupportsInfiniteRangeがfalseの場合、UE4.19以前に作成したループ音を含むデータがエラーになる。 */
		bSupportsInfiniteRange = true;
	}

	/* アセットの読み書き */
	Super::Serialize(Ar);

	if (Ar.CustomVer(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::FloatToIntConversion) {
		/* UE4.19以前で作成されたループデータの範囲を補正 */
		if (EndTime_DEPRECATED > INDEFINITELY_LOOPING_DURATION) {
			EndTime_DEPRECATED = INDEFINITELY_LOOPING_DURATION;
			const FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();
			FFrameNumber StartFrame = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, StartTime_DEPRECATED);
			FFrameNumber LastFrame  = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, EndTime_DEPRECATED);
			SectionRange = TRange<FFrameNumber>(StartFrame, LastFrame + 1);
		}
	}

	if (Ar.IsLoading()) {
		/* エラー回避措置の終了 */
		bSupportsInfiniteRange = false;
	}
}

TArray<FScalarParameterNameAndCurve>& UMovieSceneAtomSection::GetScalarParameterNamesAndCurves()
{
	return ScalarParameterNamesAndCurves;
}

const TArray<FScalarParameterNameAndCurve>& UMovieSceneAtomSection::GetScalarParameterNamesAndCurves() const
{
	return ScalarParameterNamesAndCurves;
}

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 25
FMovieSceneEvalTemplatePtr UMovieSceneAtomSection::GenerateTemplate() const
{
	return FMovieSceneAtomSectionTemplate(*this);
}
#endif

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
EMovieSceneChannelProxyType  UMovieSceneAtomSection::CacheChannelProxy()
#else
void  UMovieSceneAtomSection::CacheChannelProxy()
#endif
{
	// Set up the channel proxy
	FMovieSceneChannelProxyData Channels;

	UMovieSceneAtomTrack* AtomTrack = Cast<UMovieSceneAtomTrack>(GetOuter());

#if WITH_EDITOR
	int32 SortOrder = 0;
	FAtomChannelEditorData EditorData;
	/*  
		AtomTrack要素の表示順
		0: SoundVolume
		1: PitchMultiplier
		2. AttachObject
		3: Aisac... 
	*/
	EditorData.Data[0].SortOrder = SortOrder++;
	EditorData.Data[1].SortOrder = SortOrder++;
	EditorData.Data[2].SortOrder = SortOrder++;
	Channels.Add(SoundVolume, EditorData.Data[0], TMovieSceneExternalValue<float>());
	Channels.Add(PitchMultiplier, EditorData.Data[1], TMovieSceneExternalValue<float>());

	if (AtomTrack && AtomTrack->IsAMasterTrack())
	{
		Channels.Add(AttachActorData, EditorData.Data[2]);
	}

#else

	Channels.Add(SoundVolume);
	Channels.Add(PitchMultiplier);
	if (AtomTrack && AtomTrack->IsAMasterTrack())
	{
		Channels.Add(AttachActorData);
	}

#endif

	if (IsValid(Sound))
	{
		TArray<FString> AisacControls;
		AisacControls.Empty();
		if (Sound && Sound->CueSheet && Sound->CueSheet->GetCueInfoFromName(Sound->CueName)) {
			AisacControls = Sound->CueSheet->GetCueInfoFromName(Sound->CueName)->AisacControlNames;
		}

		for (FString AisacName : AisacControls)
		{
			ScalarParameterNamesAndCurves.Add(FScalarParameterNameAndCurve(*AisacName));
			FScalarParameterNameAndCurve& AisacControl = ScalarParameterNamesAndCurves.Last();
#if WITH_EDITOR
			FMovieSceneChannelMetaData MetaData(AisacControl.ParameterName, FText::FromName(AisacControl.ParameterName));
			MetaData.SortOrder = SortOrder++;
			AisacControl.ParameterCurve.SetDefault(0.0f);
			Channels.Add(AisacControl.ParameterCurve, MetaData, TMovieSceneExternalValue<float>());
#else
			AisacControl.ParameterCurve.SetDefault(0.0f);
			Channels.Add(AisacControl.ParameterCurve);
#endif
		}
	}

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
	return EMovieSceneChannelProxyType::Dynamic;
#endif
}

TOptional<FFrameTime> UMovieSceneAtomSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(StartFrameOffset);
}

FFrameNumber UMovieSceneAtomSection::GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, FFrameNumber StartOffset, FFrameNumber StartFrame)
{
	return StartOffset + TrimTime.Time.FrameNumber - StartFrame;
}

TOptional<TRange<FFrameNumber>> UMovieSceneAtomSection::GetAutoSizeRange() const
{
	if (!Sound) {
		return TRange<FFrameNumber>();
	}

	/* フレームレートの取得 */
	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	/* 音声データの長さ（秒単位）を取得 */
	float SoundDuration = Sound->GetDuration();
	if (SoundDuration < INDEFINITELY_LOOPING_DURATION) {
		SoundDuration = Sound->GetDuration(); // seconds
	} else {
		SoundDuration = Sound->GetFirstWaveDuration(); // if not a wave this return INDEFINITELY_LOOPING_DURATION
	}

	/* フレーム単位に換算 */
	FFrameTime DurationToUse = SoundDuration * FrameRate;

	return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + DurationToUse.FrameNumber);
}

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 24) || ENGINE_MAJOR_VERSION == 5

void UMovieSceneAtomSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys)
{
	SetFlags(RF_Transactional);

	if (TryModify()) {
		if (bTrimLeft) {
			StartFrameOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft, bDeleteKeys);
	}
}

UMovieSceneSection* UMovieSceneAtomSection::SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys)
{
	const FFrameNumber InitialStartFrameOffset = StartFrameOffset;

	const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime, bDeleteKeys);
	if (NewSection != nullptr)
	{
		UMovieSceneAtomSection* NewAtomSection = Cast<UMovieSceneAtomSection>(NewSection);
		NewAtomSection->StartFrameOffset = NewOffset;
	}

	// Restore original offset modified by splitting
	StartFrameOffset = InitialStartFrameOffset;

	return NewSection;
}

#else

void UMovieSceneAtomSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft)
{
	SetFlags(RF_Transactional);

	if (TryModify()) {
		if (bTrimLeft) {
			StartFrameOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft);
	}
}

UMovieSceneSection* UMovieSceneAtomSection::SplitSection(FQualifiedFrameTime SplitTime)
{
	const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneAtomSection* NewAudioSection = Cast<UMovieSceneAtomSection>(NewSection);
		NewAudioSection->StartFrameOffset = NewOffset;
	}
	return NewSection;
}

#endif

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26) || ENGINE_MAJOR_VERSION == 5
USceneComponent* UMovieSceneAtomSection::GetAttachComponent(const AActor* InParentActor, const FMovieSceneActorReferenceKey& Key) const
{
	/* AtomCompoentをAttachすべきコンポーネントを取得する */
	FName AttachComponentName = Key.ComponentName;
	FName AttachSocketName = Key.SocketName;

	if (AttachSocketName != NAME_None)
	{
		/* Socket指定がある場合はSocket位置にアタッチする */
		if (AttachComponentName != NAME_None)
		{
			/* 指定されたコンポーネント名が見つかり、ソケットが存在する場合はそのコンポーネントを返す */
			TInlineComponentArray<USceneComponent*> PotentialAttachComponents(InParentActor);
			for (USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
			{
				if (PotentialAttachComponent->GetFName() == AttachComponentName && PotentialAttachComponent->DoesSocketExist(AttachSocketName))
				{
					return PotentialAttachComponent;
				}
			}
		}
		else if (InParentActor->GetRootComponent()->DoesSocketExist(AttachSocketName))
		{
			return InParentActor->GetRootComponent();
		}
	}
	else if (AttachComponentName != NAME_None)
	{
		/* Socket名が指定されていない場合は指定された名前のコンポーネントを返す */
		TInlineComponentArray<USceneComponent*> PotentialAttachComponents(InParentActor);
		for (USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
		{
			if (PotentialAttachComponent->GetFName() == AttachComponentName)
			{
				return PotentialAttachComponent;
			}
		}
	}

	if (InParentActor->GetDefaultAttachComponent())
	{
		/* actorにデフォルトのアタッチコンポーネントが設定されている場合は、そのコンポーネントを返す。 */
		return InParentActor->GetDefaultAttachComponent();
	}
	else
	{
		return InParentActor->GetRootComponent();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
