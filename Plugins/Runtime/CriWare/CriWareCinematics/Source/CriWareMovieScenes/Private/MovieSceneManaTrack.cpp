#include "MovieSceneManaTrack.h"

UMovieSceneManaTrack::UMovieSceneManaTrack() {
}


FMovieSceneEvalTemplatePtr UMovieSceneManaTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const {
    return FMovieSceneEvalTemplatePtr();
}