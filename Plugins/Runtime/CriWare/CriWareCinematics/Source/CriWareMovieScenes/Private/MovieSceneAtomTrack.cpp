#include "MovieSceneAtomTrack.h"

UMovieSceneAtomTrack::UMovieSceneAtomTrack() {
}

void UMovieSceneAtomTrack::AddSection(UMovieSceneSection& Section) {
    AtomSections.Add(&Section);
}

void UMovieSceneAtomTrack::RemoveSection(UMovieSceneSection& Section) {
    AtomSections.Remove(&Section);
}

bool UMovieSceneAtomTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const {
    return true; // or restrict to specific section type
}

UMovieSceneSection* UMovieSceneAtomTrack::CreateNewSection() {
    return NewObject<UMovieSceneSection>(this, NAME_None, RF_Transactional);
}

const TArray<UMovieSceneSection*>& UMovieSceneAtomTrack::GetAllSections() const {
    return AtomSections;
}

bool UMovieSceneAtomTrack::HasSection(const UMovieSceneSection& Section) const {
    return AtomSections.Contains(&Section);
}

bool UMovieSceneAtomTrack::IsEmpty() const {
    return AtomSections.Num() == 0;
}

FMovieSceneEvalTemplatePtr UMovieSceneAtomTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const {
    return FMovieSceneEvalTemplatePtr();
}
