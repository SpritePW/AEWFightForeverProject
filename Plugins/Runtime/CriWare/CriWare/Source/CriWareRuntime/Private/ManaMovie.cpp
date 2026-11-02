#include "ManaMovie.h"
#include "EngineUtils.h"
#include "UObject/LinkerLoad.h"

UManaMovie::UManaMovie() {
    this->NumSubtitleChannels = 0;
    this->MaxSubtitleSize = 0;
    this->MinBufferSize = 0;
    this->MaxChunkSize = 0;
    this->bIsAlpha = false;
}

bool UManaMovie::Validate() const {
    return false;
}

bool UManaMovie::IsAlphaMovie() {
    return false;
}

int32 UManaMovie::GetVideoTrackTextureDimensionsY(int32 TrackIndex) const {
    return 0;
}

int32 UManaMovie::GetVideoTrackTextureDimensionsX(int32 TrackIndex) const {
    return 0;
}

FString UManaMovie::GetUrl() const {
    return TEXT("");
}

float UManaMovie::GetMinBufferSize() {
    return 0.0f;
}

float UManaMovie::GetMaxMovieSecond() {
    return 0.0f;
}

FIntPoint UManaMovie::GetDisplayDimensions() {
    return FIntPoint{};
}

float UManaMovie::GetAverageBitrate() {
    return 0.0f;
}



void UManaMovie::Serialize(FArchive& Ar) {
    Super::Serialize(Ar);

    bool bSerializeFlags = true;
    if (Ar.IsLoading() && Ar.GetLinker() != nullptr) {
        FLinkerLoad* Linker = GetLinker();
        const int32 Index = GetLinkerIndex();
        if (Linker != nullptr && Linker->ExportMap.IsValidIndex(Index)) {
            const FObjectExport& Export = Linker->ExportMap[Index];
            bSerializeFlags = Ar.Tell() + 2 <= Export.SerialOffset + Export.SerialSize;
        }
    }
    if (bSerializeFlags) {
        FStripDataFlags StripFlags(Ar);
    }
}
