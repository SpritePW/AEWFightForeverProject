/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Atom Track Editor
 * File     : AtomTrackEditor.cpp
 *
 ****************************************************************************/

#include "AtomTrackEditor.h"
#include "Textures/SlateTextureData.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "Modules/ModuleManager.h"
//#include "Audio.h"
//#include "Sound/SoundBase.h"
//#include "Sound/SoundWave.h"
//#include "Sound/DialogueWave.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "SequencerSectionPainter.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "SoundAtomCue.h"
#include "UnrealEdGlobals.h"
#include "MovieSceneAtomTrack.h"
#include "MovieSceneAtomSection.h"
#include "CommonMovieSceneTools.h"
//#include "AudioDevice.h"
//#include "Sound/SoundNodeWavePlayer.h"
//#include "Sound/SoundNodeDialoguePlayer.h"
#include "Slate/SlateTextures.h"
//#include "AudioDecompress.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif // ENGINE_MAJOR_VERSION >= 5
#include "ISectionLayoutBuilder.h"
#include "MovieSceneToolHelpers.h"

#include "DragAndDrop/AssetDragDropOp.h"
#include "Misc/QualifiedFrameTime.h"

#include "AtomThumbnail.h"
#include "Shared/CriWareSequencerEditorStyle.h"


#define LOCTEXT_NAMESPACE "FAtomTrackEditor"


namespace AnimatableAudioEditorConstants
{
	// Optimization - maximum samples per pixel this sound allows
	const uint32 MaxSamplesPerPixel = 60;
}


/** These utility functions should go away once we start handling sound cues properly */
USoundAtomCue* DeriveSoundWave(UMovieSceneAtomSection* AtomSection)
{
	USoundAtomCue* SoundWave = NULL;

	USoundAtomCue* Sound = AtomSection->GetSound();
	if (!Sound)
	{
		return SoundWave;
	}

	if (Sound->IsA<USoundAtomCue>())
	{
		SoundWave = Cast<USoundAtomCue>(Sound);
	}
	else if (Sound->IsA<USoundAtomCue>())
	{
		USoundAtomCue* SoundCue = Cast<USoundAtomCue>(Sound);

		// @todo Sequencer - Right now for sound cues, we just use the first sound wave in the cue
		// In the future, it would be better to properly generate the sound cue's data after forcing determinism
		/*const TArray<USoundNode*>& AllNodes = SoundCue->AllNodes;
		for (int32 Index = 0; Index < AllNodes.Num() && SoundWave == nullptr; ++Index)
		{
			if (AllNodes[Index])
			{
				if (USoundNodeWavePlayer* SoundNodeWavePlayer = Cast<USoundNodeWavePlayer>(AllNodes[Index]))
				{
					SoundWave = SoundNodeWavePlayer->GetSoundWave();
				}
				else if (USoundNodeDialoguePlayer* SoundNodeDialoguePlayer = Cast<USoundNodeDialoguePlayer>(AllNodes[Index]))
				{
					if (UDialogueWave* DialogueWave = SoundNodeDialoguePlayer->GetDialogueWave())
					{
						if (DialogueWave->ContextMappings.Num() > 0)
						{
							SoundWave = DialogueWave->ContextMappings[0].SoundWave;
						}
					}
				}
			}
		}*/
	}

	return SoundWave;
}

float DeriveUnloopedDuration(UMovieSceneAtomSection* AtomSection)
{
	USoundAtomCue* Sound = AtomSection->GetSound();
	if (Sound && Sound->GetDuration() != INDEFINITELY_LOOPING_DURATION)
	{
		return Sound->GetDuration();
	}

	USoundAtomCue* SoundWave = DeriveSoundWave(AtomSection);
	const float Duration = (SoundWave ? SoundWave->GetDuration() : 0.f);
	return Duration == INDEFINITELY_LOOPING_DURATION ? SoundWave->Duration : Duration;
}


/** The maximum number of channels we support */
static const int32 MaxSupportedChannels = 2;
/** The number of pixels between which to place control points for cubic interpolation */
static const int32 SmoothingAmount = 6;
/** The size of the sroked border of the audio wave */
static const int32 StrokeBorderSize = 2;


FAtomSection::FAtomSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
	: Section(InSection)
	, StoredDrawRange(TRange<float>::Empty())
	, StoredSectionHeight(0.f)
	, bStoredLooping(true)
	, Sequencer(InSequencer)
	, InitialStartOffsetDuringResize(0)
	, InitialStartTimeDuringResize(0)
{
}

FAtomSection::~FAtomSection()
{
}

UMovieSceneSection* FAtomSection::GetSectionObject()
{
	return &Section;
}

FText FAtomSection::GetSectionTitle() const
{
	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);
	if (AtomSection && AtomSection->GetSound())
	{
		return FText::FromString(AtomSection->GetSound()->GetName());
	}

	return NSLOCTEXT("FAtomSection", "NoAtomTitleName", "No Atom");
}

FText FAtomSection::GetSectionToolTip() const
{
	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);
	const USoundAtomCue* Sound = AtomSection ? DeriveSoundWave(AtomSection) : nullptr;

	if (AtomSection && Sound && AtomSection->HasStartFrame() && AtomSection->HasEndFrame())
	{
		UMovieScene* MovieScene = AtomSection->GetTypedOuter<UMovieScene>();
		FFrameRate TickResolution = MovieScene->GetTickResolution();

		const float AudioStartTime = AtomSection->GetStartOffset() / TickResolution;
		const float SectionDuration = (AtomSection->GetExclusiveEndFrame() - AtomSection->GetInclusiveStartFrame()) / TickResolution;

		if (AtomSection->IsLooping())
		{
			return FText::Format(LOCTEXT("ToolTipContentFormatLooping", "Start: {0}s\nDuration: {1}s\nLooping"),
				AudioStartTime,
				SectionDuration);
		}
		else
		{
			const float SoundDuration = Sound->Duration - AudioStartTime;
			const float Duration = FMath::Min(SoundDuration, SectionDuration);

			if (Duration > 0.0f)
			{
				return FText::Format(LOCTEXT("ToolTipContentFormat", "{0}s - {1}s ({2} seconds)"),
					AudioStartTime,
					AudioStartTime + Duration,
					Duration);
			}
		}
	}

	return FText::GetEmpty();
}

float FAtomSection::GetSectionHeight() const
{
	return Section.GetTypedOuter<UMovieSceneAtomTrack>()->GetRowHeight();
}

int32 FAtomSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	FLinearColor TintColor = FLinearColor::White;
	if (StoredSoundAtomCue.IsValid())
	{
		TintColor = StoredSoundAtomCue->CueColor;
	}

	if (WaveformThumbnail.IsValid() && WaveformThumbnail->ShouldRender())
	{
		// @todo Sequencer draw multiple times if looping possibly - requires some thought about SoundCues
		FSlateDrawElement::MakeViewport(
			Painter.DrawElements,
			++LayerId,
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
			Painter.SectionGeometry.ToPaintGeometry(FVector2D(StoredXSize, GetSectionHeight() + 8.f), FSlateLayoutTransform(FVector2f(StoredXOffset, 0))),
#else
			Painter.SectionGeometry.ToPaintGeometry(FVector2D(StoredXOffset, 0), FVector2D(StoredXSize, GetSectionHeight() + 8.f)),
#endif // ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
			WaveformThumbnail,
			(Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect) | ESlateDrawEffect::NoGamma,
			TintColor
		);
	}

	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	static const FSlateBrush* GenericDivider = FEditorStyle::GetBrush("Sequencer.GenericDivider");

	if (!Section.HasStartFrame() || !Section.HasEndFrame())
	{
		return LayerId;
	}

	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);
	if (!AtomSection || !AtomSection->GetSound())
	{
		return LayerId;
	}

	// Add lines where the animation starts and ends/loops
	FFrameRate TickResolution = TimeToPixelConverter.GetTickResolution();
	const float AudioDuration = DeriveUnloopedDuration(AtomSection);

	if (AudioDuration > KINDA_SMALL_NUMBER)
	{
		const float MaxOffset = Section.GetRange().Size<FFrameTime>() / TickResolution;
		const float StartOffsetTime = TickResolution.AsSeconds(AtomSection->GetStartOffset());
		const float SectionStartTime = TickResolution.AsSeconds(AtomSection->GetInclusiveStartFrame());

		float OffsetTime = AudioDuration - StartOffsetTime;
		while (OffsetTime < MaxOffset)
		{
			float OffsetPixel = TimeToPixelConverter.SecondsToPixel(SectionStartTime + OffsetTime);

			FSlateDrawElement::MakeBox(
				Painter.DrawElements,
				LayerId,
				Painter.SectionGeometry.MakeChild(
					FVector2D(2.f, Painter.SectionGeometry.Size.Y - 2.f),
					FSlateLayoutTransform(FVector2D(OffsetPixel, 1.f))
				).ToPaintGeometry(),
				GenericDivider,
				DrawEffects
			);

			OffsetTime += AudioDuration;

			if (!AtomSection->IsLooping())
			{
				break;
			}
		}
	}

	return LayerId;
}

void FAtomSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);
	UMovieSceneTrack* Track = Section.GetTypedOuter<UMovieSceneTrack>();

	USoundAtomCue* SoundWave = DeriveSoundWave(AtomSection);
	if (Track && SoundWave/* && (SoundWave->NumChannels == 1 || SoundWave->NumChannels == 2)*/)
	{
		const FSlateRect ParentRect = TransformRect(
			Concatenate(
				ParentGeometry.GetAccumulatedLayoutTransform(),
				AllottedGeometry.GetAccumulatedLayoutTransform().Inverse()
			),
			FSlateRect(FVector2D(0, 0), ParentGeometry.GetLocalSize())
		);

		const float LeftMostVisiblePixel = FMath::Max(ParentRect.Left, 0.f);
		const float RightMostVisiblePixel = FMath::Min(ParentRect.Right, AllottedGeometry.GetLocalSize().X);

		FFrameRate   TickResolution = AtomSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FTimeToPixel TimeToPixel(AllottedGeometry, AtomSection->GetRange() / TickResolution, TickResolution);

		TRange<float> DrawRange = TRange<float>(
			TimeToPixel.PixelToSeconds(LeftMostVisiblePixel),
			TimeToPixel.PixelToSeconds(RightMostVisiblePixel)
		);

		// generate texture x offset and x size
		int32 XOffset = LeftMostVisiblePixel;//PixelRange.GetLowerBoundValue() - TimeToPixelConverter.TimeToPixel(SectionRange.GetLowerBoundValue());
		int32 XSize = RightMostVisiblePixel - LeftMostVisiblePixel;//PixelRange.Size<int32>();

		if (!FMath::IsNearlyEqual(DrawRange.GetLowerBoundValue(), StoredDrawRange.GetLowerBoundValue()) ||
			!FMath::IsNearlyEqual(DrawRange.GetUpperBoundValue(), StoredDrawRange.GetUpperBoundValue()) ||
			XOffset != StoredXOffset || XSize != StoredXSize || Track->GetColorTint() != StoredColor ||
			StoredSoundAtomCue != SoundWave ||
			StoredSectionHeight != GetSectionHeight() ||
			StoredStartOffset != AtomSection->GetStartOffset() ||
			bStoredLooping != AtomSection->IsLooping())
		{
			float DisplayScale = XSize / DrawRange.Size<float>();

			// Use the view range if possible, as it's much more stable than using the texture size and draw range
			TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
			if (SequencerPin.IsValid())
			{
				DisplayScale = SequencerPin->GetViewRange().Size<float>() / ParentGeometry.GetLocalSize().X;
			}

			RegenerateWaveforms(DrawRange, XOffset, XSize, Track->GetColorTint(), DisplayScale);
			StoredSoundAtomCue = SoundWave;
		}
	}
	else
	{
		WaveformThumbnail.Reset();
		StoredDrawRange = TRange<float>::Empty();
		StoredSoundAtomCue.Reset();
	}
}

void FAtomSection::BeginResizeSection()
{
	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);
	InitialStartOffsetDuringResize = AtomSection->GetStartOffset();
	InitialStartTimeDuringResize = AtomSection->HasStartFrame() ? AtomSection->GetInclusiveStartFrame() : 0;
}

void FAtomSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);

	if (ResizeMode == SSRM_LeadingEdge && AtomSection)
	{
		FFrameNumber NewStartOffset = ResizeTime - InitialStartTimeDuringResize;
		NewStartOffset += InitialStartOffsetDuringResize;

		// Ensure start offset is not less than 0
		if (NewStartOffset < 0)
		{
			ResizeTime = ResizeTime - NewStartOffset;
			NewStartOffset = FFrameNumber(0);
		}

		AtomSection->SetStartOffset(NewStartOffset);
	}

	ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
}

void FAtomSection::BeginSlipSection()
{
	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);
	InitialStartOffsetDuringResize = AtomSection->GetStartOffset();
	InitialStartTimeDuringResize = AtomSection->HasStartFrame() ? AtomSection->GetInclusiveStartFrame() : 0;
}

void FAtomSection::SlipSection(FFrameNumber SlipTime)
{
	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);

	FFrameNumber NewStartOffset = SlipTime - InitialStartTimeDuringResize;
	NewStartOffset += InitialStartOffsetDuringResize;

	// Ensure start offset is not less than 0
	AtomSection->SetStartOffset(FMath::Max(NewStartOffset, FFrameNumber(0)));

	ISequencerSection::SlipSection(SlipTime);
}

void FAtomSection::RegenerateWaveforms(TRange<float> DrawRange, int32 XOffset, int32 XSize, const FColor& ColorTint, float DisplayScale)
{
	UMovieSceneAtomSection* AtomSection = Cast<UMovieSceneAtomSection>(&Section);

	StoredDrawRange = DrawRange;
	StoredXOffset = XOffset;
	StoredXSize = XSize;
	StoredColor = ColorTint;
	StoredStartOffset = AtomSection->GetStartOffset();
	StoredSectionHeight = GetSectionHeight();
	bStoredLooping = AtomSection->IsLooping();

	if (DrawRange.IsDegenerate() || DrawRange.IsEmpty() || AtomSection->GetSound() == NULL)
	{
		WaveformThumbnail.Reset();
	}
	else
	{
		WaveformThumbnail = MakeShareable(new FAtomThumbnail(Section, DrawRange, XSize, ColorTint, DisplayScale));
	}
}


FAtomTrackEditor::FAtomTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

FAtomTrackEditor::~FAtomTrackEditor()
{
}

TSharedRef<ISequencerTrackEditor> FAtomTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FAtomTrackEditor(InSequencer));
}

void FAtomTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddTrack", "Atom Track"),
		LOCTEXT("AddTooltip", "Adds a new atom track that can play sounds."),
		FSlateIcon(FCriWareSequencerEditorStyle::Get().GetStyleSetName(), "ClassIcon.AtomPlane"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FAtomTrackEditor::HandleAddAtomTrackMenuEntryExecute)
		)
	);
}

void FAtomTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if (ObjectClass != nullptr && ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("AddAttachedAtomTrack", "Atom"),
			LOCTEXT("AddAttachedAtomTooltip", "Adds an atom track attached to the object."),
			FNewMenuDelegate::CreateSP(this, &FAtomTrackEditor::HandleAddAttachedAtomTrackMenuEntryExecute, ObjectBindings));
	}
}

bool FAtomTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneAtomTrack::StaticClass();
}

bool FAtomTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	ETrackSupport TrackSupported = InSequence ? ETrackSupport::Supported : ETrackSupport::NotSupported;
	return TrackSupported == ETrackSupport::Supported;
}

const FSlateBrush* FAtomTrackEditor::GetIconBrush() const
{
	return FCriWareSequencerEditorStyle::Get().GetBrush("ClassIcon.AtomPlane");
}

bool FAtomTrackEditor::IsResizable(UMovieSceneTrack* InTrack) const
{
	return true;
}

void FAtomTrackEditor::Resize(float NewSize, UMovieSceneTrack* InTrack)
{
	UMovieSceneAtomTrack* AtomTrack = Cast<UMovieSceneAtomTrack>(InTrack);
	if (AtomTrack)
	{
		AtomTrack->Modify();

		int32 MaxNumRows = 1;
		for (UMovieSceneSection* Section : AtomTrack->GetAllSections())
		{
			MaxNumRows = FMath::Max(MaxNumRows, Section->GetRowIndex() + 1);
		}

		AtomTrack->SetRowHeight(FMath::RoundToInt(NewSize) / MaxNumRows);
	}
}

#if ENGINE_MAJOR_VERSION >= 5
bool FAtomTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams)
#else
bool FAtomTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track, int32 RowIndex, const FGuid& TargetObjectGuid)
#endif // ENGINE_MAJOR_VERSION >= 5
{
#if ENGINE_MAJOR_VERSION >= 5
	if (!DragDropParams.Track.IsValid() || !DragDropParams.Track.Get()->IsA(UMovieSceneAtomTrack::StaticClass()))
	{
		return false;
	}
#else
	if (!Track->IsA(UMovieSceneAtomTrack::StaticClass()))
	{
		return false;
	}
#endif // ENGINE_MAJOR_VERSION >= 5

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FAssetDragDropOp>())
	{
		return false;
	}

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr)
	{
		return false;
	}

	UMovieSceneSequence* FocusedSequence = SequencerPtr->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return false;
	}

	TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);

	for (const FAssetData& AssetData : DragDropOp->GetAssets())
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
		if (!MovieSceneToolHelpers::IsValidAsset(FocusedSequence, AssetData))
		{
			continue;
		}
#endif // ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1

		if (USoundAtomCue* Sound = Cast<USoundAtomCue>(AssetData.GetAsset()))
		{
			FFrameRate TickResolution = SequencerPtr->GetFocusedTickResolution();
			FFrameNumber LengthInFrames = TickResolution.AsFrameNumber(Sound->GetDuration());
#if ENGINE_MAJOR_VERSION >= 5
			DragDropParams.FrameRange = TRange<FFrameNumber>(DragDropParams.FrameNumber, DragDropParams.FrameNumber + LengthInFrames);
#endif // ENGINE_MAJOR_VERSION >= 5
			return true;
		}
	}

	return false;
}

#if ENGINE_MAJOR_VERSION >= 5
FReply FAtomTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams)
#else
FReply FAtomTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track, int32 RowIndex, const FGuid& TargetObjectGuid)
#endif // ENGINE_MAJOR_VERSION >= 5
{
#if ENGINE_MAJOR_VERSION >= 5
	if (!DragDropParams.Track.IsValid() || !DragDropParams.Track.Get()->IsA(UMovieSceneAtomTrack::StaticClass()))
	{
		return FReply::Unhandled();
	}
#else
	if (!Track->IsA(UMovieSceneAtomTrack::StaticClass()))
	{
		return FReply::Unhandled();
	}
#endif // ENGINE_MAJOR_VERSION >= 5

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (!Operation.IsValid() || !Operation->IsOfType<FAssetDragDropOp>())
	{
		return FReply::Unhandled();
	}

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr)
	{
		return FReply::Unhandled();
	}

	UMovieSceneSequence* FocusedSequence = SequencerPtr->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return FReply::Unhandled();
	}

	const FScopedTransaction Transaction(LOCTEXT("DropAssets", "Drop Assets"));

	TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);

#if ENGINE_MAJOR_VERSION >= 5
	FMovieSceneTrackEditor::BeginKeying(DragDropParams.FrameNumber);
#endif // ENGINE_MAJOR_VERSION >= 5

	bool bAnyDropped = false;
	for (const FAssetData& AssetData : DragDropOp->GetAssets())
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
		if (!MovieSceneToolHelpers::IsValidAsset(FocusedSequence, AssetData))
		{
			continue;
		}
#endif // ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1

		USoundAtomCue* Sound = Cast<USoundAtomCue>(AssetData.GetAsset());

		if (Sound)
		{
			UMovieSceneAtomTrack* AtomTrack = Cast<UMovieSceneAtomTrack>(Track);

			if (TargetObjectGuid.IsValid())
			{
				TArray<TWeakObjectPtr<>> OutObjects;
				for (TWeakObjectPtr<> Object : SequencerPtr->FindObjectsInCurrentSequence(TargetObjectGuid))
				{
					OutObjects.Add(Object);
				}

				AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(
					this,
					&FAtomTrackEditor::AddNewAttachedSound,
					Sound,
					AtomTrack,
					OutObjects));
			}
			else
			{
				AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(
					this,
					&FAtomTrackEditor::AddNewSound,
					Sound,
					AtomTrack,
					RowIndex));
			}

			bAnyDropped = true;
		}
	}

	FMovieSceneTrackEditor::EndKeying();

	return bAnyDropped ? FReply::Handled() : FReply::Unhandled();
}

TSharedRef<ISequencerSection> FAtomTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShareable(new FAtomSection(SectionObject, GetSequencer()));
}

TSharedPtr<SWidget> FAtomTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	// Create a container edit box
	return SNew(SHorizontalBox)

	// Add the audio combo box
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("AtomText", "Atom"), FOnGetContent::CreateSP(this, &FAtomTrackEditor::BuildAtomSubMenu, FOnAssetSelected::CreateRaw(this, &FAtomTrackEditor::OnAtomAssetSelected, Track), FOnAssetEnterPressed::CreateRaw(this, &FAtomTrackEditor::OnAtomAssetEnterPressed, Track)), Params.NodeIsHovered, GetSequencer())
	];
}

bool FAtomTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	if (Asset->IsA<USoundAtomCue>())
	{
		auto Sound = Cast<USoundAtomCue>(Asset);
		UMovieSceneAtomTrack* DummyTrack = nullptr;

		const FScopedTransaction Transaction(LOCTEXT("AddAudio_Transaction", "Add Audio"));

		if (TargetObjectGuid.IsValid())
		{
			TArray<TWeakObjectPtr<>> OutObjects;
			for (TWeakObjectPtr<> Object : GetSequencer()->FindObjectsInCurrentSequence(TargetObjectGuid))
			{
				OutObjects.Add(Object);
			}

			AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FAtomTrackEditor::AddNewAttachedSound, Sound, DummyTrack, OutObjects));
		}
		else
		{
			int32 RowIndex = INDEX_NONE;
			AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FAtomTrackEditor::AddNewSound, Sound, DummyTrack, RowIndex));
		}

		return true;
	}
	return false;
}

FKeyPropertyResult FAtomTrackEditor::AddNewSound(FFrameNumber KeyTime, USoundAtomCue* Sound, UMovieSceneAtomTrack* AtomTrack, int32 RowIndex)
{
	FKeyPropertyResult KeyPropertyResult;

	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (FocusedMovieScene->IsReadOnly())
	{
		return KeyPropertyResult;
	}

	FocusedMovieScene->Modify();

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
	FFindOrCreateRootTrackResult<UMovieSceneAtomTrack> TrackResult;
#else
	FFindOrCreateMasterTrackResult<UMovieSceneAtomTrack> TrackResult;
#endif // ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2

	TrackResult.Track = AtomTrack;
	if (!AtomTrack)
	{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
		TrackResult = FindOrCreateRootTrack<UMovieSceneAtomTrack>();
#else
		TrackResult = FindOrCreateMasterTrack<UMovieSceneAtomTrack>();
#endif // ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2

		AtomTrack = TrackResult.Track;
	}

	if (ensure(AtomTrack))
	{
		AtomTrack->Modify();

		UMovieSceneSection* NewSection = AtomTrack->AddNewSoundOnRow(Sound, KeyTime, RowIndex);

		if (TrackResult.bWasCreated)
		{
			AtomTrack->SetDisplayName(LOCTEXT("AtomTrackName", "Audio"));

			if (GetSequencer().IsValid())
			{
				GetSequencer()->OnAddTrack(AtomTrack, FGuid());
			}
		}

		KeyPropertyResult.bTrackModified = true;
		KeyPropertyResult.SectionsCreated.Add(NewSection);
	}

	return KeyPropertyResult;
}

FKeyPropertyResult FAtomTrackEditor::AddNewAttachedSound(FFrameNumber KeyTime, USoundAtomCue* Sound, UMovieSceneAtomTrack* AtomTrack, TArray<TWeakObjectPtr<UObject>> ObjectsToAttachTo)
{
	FKeyPropertyResult KeyPropertyResult;

	for (int32 ObjectIndex = 0; ObjectIndex < ObjectsToAttachTo.Num(); ++ObjectIndex)
	{
		UObject* Object = ObjectsToAttachTo[ObjectIndex].Get();

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
		FGuid ObjectHandle = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;

		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult;
			TrackResult.Track = AtomTrack;
			if (!AtomTrack)
			{
				TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieSceneAtomTrack::StaticClass());
				AtomTrack = Cast<UMovieSceneAtomTrack>(TrackResult.Track);
			}

			KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(AtomTrack))
			{
				AtomTrack->Modify();

				UMovieSceneSection* NewSection = AtomTrack->AddNewSound(Sound, KeyTime);
				AtomTrack->SetDisplayName(LOCTEXT("AtomTrackName", "Atom"));
				KeyPropertyResult.bTrackModified = true;
				KeyPropertyResult.SectionsCreated.Add(NewSection);

				GetSequencer()->EmptySelection();
				GetSequencer()->SelectSection(NewSection);
				GetSequencer()->ThrobSectionSelection();
			}
		}
	}

	return KeyPropertyResult;
}


/* FAtomTrackEditor callbacks
 *****************************************************************************/

void FAtomTrackEditor::HandleAddAtomTrackMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddAtomTrack_Transaction", "Add Atom Track"));
	FocusedMovieScene->Modify();

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
	auto NewTrack = FocusedMovieScene->AddTrack<UMovieSceneAtomTrack>();
#else
	auto NewTrack = FocusedMovieScene->AddMasterTrack<UMovieSceneAtomTrack>();
#endif // ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2

	ensure(NewTrack);

	NewTrack->SetDisplayName(LOCTEXT("AtomTrackName", "Atom"));

	if (GetSequencer().IsValid())
	{
		GetSequencer()->OnAddTrack(NewTrack, FGuid());
	}
}

void FAtomTrackEditor::HandleAddAttachedAtomTrackMenuEntryExecute(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings)
{
	MenuBuilder.AddWidget(BuildAtomSubMenu(FOnAssetSelected::CreateRaw(this, &FAtomTrackEditor::OnAttachedAtomAssetSelected, ObjectBindings), FOnAssetEnterPressed::CreateRaw(this, &FAtomTrackEditor::OnAttachedAtomEnterPressed, ObjectBindings)), FText::GetEmpty(), true);
}

TSharedRef<SWidget> FAtomTrackEditor::BuildAtomSubMenu(FOnAssetSelected OnAssetSelected, FOnAssetEnterPressed OnAssetEnterPressed)
{
	UMovieSceneSequence* Sequence = GetSequencer() ? GetSequencer()->GetFocusedMovieSceneSequence() : nullptr;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
	TArray<FTopLevelAssetPath> ClassNames;
	ClassNames.Add(USoundBase::StaticClass()->GetClassPathName());
	TSet<FTopLevelAssetPath> DerivedClassNames;
	AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FTopLevelAssetPath>(), DerivedClassNames);
#else
	TArray<FName> ClassNames;
	ClassNames.Add(USoundAtomCue::StaticClass()->GetFName());
	TSet<FName> DerivedClassNames;
	AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);
#endif // ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1

	FMenuBuilder MenuBuilder(true, nullptr);

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = OnAssetSelected;
		AssetPickerConfig.OnAssetEnterPressed = OnAssetEnterPressed;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.bAddFilterUI = true;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		for (auto ClassName : DerivedClassNames) 
		{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
			AssetPickerConfig.Filter.ClassPaths.Add(ClassName);
#else
			AssetPickerConfig.Filter.ClassNames.Add(ClassName);
#endif // ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
		}
		AssetPickerConfig.SaveSettingsName = TEXT("SequencerAssetPicker");
		AssetPickerConfig.AdditionalReferencingAssets.Add(FAssetData(Sequence));
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);

	return MenuBuilder.MakeWidget();
}

void FAtomTrackEditor::OnAtomAssetSelected(const FAssetData& AssetData, UMovieSceneTrack* Track)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject)
	{
		USoundAtomCue* NewSound = CastChecked<USoundAtomCue>(AssetData.GetAsset());
		if (NewSound != nullptr)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddAtom_Transaction", "Add Atom"));

			auto AtomTrack = Cast<UMovieSceneAtomTrack>(Track);
			AtomTrack->Modify();

			FFrameTime KeyTime = GetSequencer()->GetLocalTime().Time;
			UMovieSceneSection* NewSection = AtomTrack->AddNewSound(NewSound, KeyTime.FrameNumber);

			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();

			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
		}
	}
}

void FAtomTrackEditor::OnAtomAssetEnterPressed(const TArray<FAssetData>& AssetData, UMovieSceneTrack* Track)
{
	if (AssetData.Num() > 0)
	{
		OnAtomAssetSelected(AssetData[0].GetAsset(), Track);
	}
}

void FAtomTrackEditor::OnAttachedAtomAssetSelected(const FAssetData& AssetData, TArray<FGuid> ObjectBindings)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddAtom_Transaction", "Add Atom"));

		for (FGuid ObjectBinding : ObjectBindings)
		{
			HandleAssetAdded(SelectedObject, ObjectBinding);
		}
	}
}

void FAtomTrackEditor::OnAttachedAtomEnterPressed(const TArray<FAssetData>& AssetData, TArray<FGuid> ObjectBindings)
{
	if (AssetData.Num() > 0)
	{
		OnAttachedAtomAssetSelected(AssetData[0].GetAsset(), ObjectBindings);
	}
}


#undef LOCTEXT_NAMESPACE
