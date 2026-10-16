#include "ManaAssetFactories.h"
#include "UsmHeaderReader.h"
#include "FileManaMovie.h"
#include "ManaTexture.h"
#include "ManaVideoTrackInfo.h"
#include "ManaAudioTrackInfo.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogManaImport, Log, All);

namespace ManaAssetFactoriesPrivate {
	template <typename T>
	T* PropertyPtr(UObject* Object, const TCHAR* Name) {
		FProperty* Property = Object->GetClass()->FindPropertyByName(FName(Name));
		return Property ? Property->ContainerPtrToValuePtr<T>(Object) : nullptr;
	}

	void SetBool(UObject* Object, const TCHAR* Name, bool bValue) {
		if (FBoolProperty* Property = CastField<FBoolProperty>(Object->GetClass()->FindPropertyByName(FName(Name)))) {
			Property->SetPropertyValue_InContainer(Object, bValue);
		}
	}

	EManaMovieType ToMovieType(int64 Codec) {
		switch (Codec) {
		case 1: return EManaMovieType::SofdecPrime;
		case 5: return EManaMovieType::H264;
		case 9: return EManaMovieType::VP9;
		default: return EManaMovieType::Unknown;
		}
	}

	EManaSoundType ToSoundType(int64 Codec) {
		switch (Codec) {
		case 2: return EManaSoundType::Adx;
		case 4: return EManaSoundType::HCA;
		default: return EManaSoundType::Unknown;
		}
	}

	FManaVideoTrackInfo MakeVideoTrack(const FUsmRow& Row, bool bAlpha) {
		FManaVideoTrackInfo Track;
		Track.TextureDimensions = FIntPoint((int32)FUsmInfo::GetInt(Row, TEXT("width")), (int32)FUsmInfo::GetInt(Row, TEXT("height")));
		Track.DisplayDimensions = FIntPoint((int32)FUsmInfo::GetInt(Row, TEXT("disp_width"), Track.TextureDimensions.X),
											(int32)FUsmInfo::GetInt(Row, TEXT("disp_height"), Track.TextureDimensions.Y));
		const int64 RateN = FUsmInfo::GetInt(Row, TEXT("framerate_n"));
		const int64 RateD = FUsmInfo::GetInt(Row, TEXT("framerate_d"), 1);
		Track.FrameRate = RateD != 0 ? (float)((RateN * 1000) / RateD) * 0.001f : 0.0f;
		Track.TotalFrames = (int32)FUsmInfo::GetInt(Row, TEXT("total_frames"));
		Track.bIsAlpha = bAlpha || FUsmInfo::GetInt(Row, TEXT("alpha_type")) != 0;
		Track.Type = ToMovieType(FUsmInfo::GetInt(Row, TEXT("mpeg_codec"), 1));
		return Track;
	}

	FString ResolveMoviePath(const FString& SourceFile, FFeedbackContext* Warn) {
		const FString MoviesDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies/"));
		FString FullSource = FPaths::ConvertRelativePathToFull(SourceFile);
		FPaths::NormalizeFilename(FullSource);

		FString Relative = FullSource;
		if (FPaths::IsUnderDirectory(FullSource, MoviesDir) && FPaths::MakePathRelativeTo(Relative, *MoviesDir)) {
			return Relative;
		}

		const FString Destination = MoviesDir / FPaths::GetCleanFilename(FullSource);
		if (IFileManager::Get().Copy(*Destination, *FullSource, true, true) == COPY_OK) {
			UE_LOG(LogManaImport, Display, TEXT("Copied %s to %s"), *FullSource, *Destination);
		} else {
			Warn->Logf(ELogVerbosity::Warning, TEXT("Couldn't copy %s into %s; set FilePath by hand."), *FullSource, *MoviesDir);
		}
		return FPaths::GetCleanFilename(FullSource);
	}
}

using namespace ManaAssetFactoriesPrivate;

UFileManaMovieImportFactory::UFileManaMovieImportFactory() {
	SupportedClass = UFileManaMovie::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	Formats.Add(TEXT("usm;CRI Sofdec2 movie"));
}

UObject* UFileManaMovieImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) {
	FUsmInfo Info;
	FString Error;
	if (!ReadUsmHeader(Filename, Info, Error)) {
		Warn->Logf(ELogVerbosity::Error, TEXT("%s"), *Error);
		return nullptr;
	}

	UFileManaMovie* Movie = NewObject<UFileManaMovie>(InParent, InClass, InName, Flags | RF_Transactional);

	if (FString* FilePath = PropertyPtr<FString>(Movie, TEXT("FilePath"))) {
		*FilePath = ResolveMoviePath(Filename, Warn);
	}

	if (TArray<FManaVideoTrackInfo>* VideoTracks = PropertyPtr<TArray<FManaVideoTrackInfo>>(Movie, TEXT("VideoTracks"))) {
		VideoTracks->Reset();
		for (const FUsmStreamHeader& Header : Info.VideoHeaders) {
			if (Header.Table.Rows.Num() > 0) {
				VideoTracks->Add(MakeVideoTrack(Header.Table.Rows[0], false));
			}
		}
	}

	if (TArray<FManaVideoTrackInfo>* AlphaTracks = PropertyPtr<TArray<FManaVideoTrackInfo>>(Movie, TEXT("AlphaTracks"))) {
		AlphaTracks->Reset();
		for (const FUsmStreamHeader& Header : Info.AlphaHeaders) {
			if (Header.Table.Rows.Num() > 0) {
				AlphaTracks->Add(MakeVideoTrack(Header.Table.Rows[0], true));
			}
		}
		SetBool(Movie, TEXT("bIsAlpha"), AlphaTracks->Num() > 0);
	}

	if (TArray<FManaAudioTrackInfo>* AudioTracks = PropertyPtr<TArray<FManaAudioTrackInfo>>(Movie, TEXT("AudioTracks"))) {
		AudioTracks->Reset();
		for (const FUsmStreamHeader& Header : Info.AudioHeaders) {
			if (Header.Table.Rows.Num() == 0) {
				continue;
			}
			const FUsmRow& Row = Header.Table.Rows[0];
			FManaAudioTrackInfo Track;
			Track.NumChannels = (int32)FUsmInfo::GetInt(Row, TEXT("num_channels"));
			Track.SamplingRate = (int32)FUsmInfo::GetInt(Row, TEXT("sampling_rate"));
			Track.TotalSamples = (int32)FUsmInfo::GetInt(Row, TEXT("total_samples"));
			Track.bIsAmbisonics = FUsmInfo::GetInt(Row, TEXT("ambisonics")) != 0;
			Track.Type = ToSoundType(FUsmInfo::GetInt(Row, TEXT("audio_codec")));
			AudioTracks->Add(Track);
		}
	}

	int64 MinBuffer = 0;
	bool bFoundWholeFile = false;
	for (const FUsmTable& Table : Info.DirectoryTables) {
		for (const FUsmRow& Row : Table.Rows) {
			const int64 RowMinBuffer = FUsmInfo::GetInt(Row, TEXT("minbuf"));
			if (FUsmInfo::GetInt(Row, TEXT("stmid"), -1) == 0) {
				MinBuffer = RowMinBuffer;
				bFoundWholeFile = true;
			} else if (!bFoundWholeFile) {
				MinBuffer = FMath::Max(MinBuffer, RowMinBuffer);
			}
		}
	}
	if (int32* MinBufferSize = PropertyPtr<int32>(Movie, TEXT("MinBufferSize"))) {
		*MinBufferSize = (int32)MinBuffer;
	}
	int64 MaxChunk = 0;
	for (const TArray<FUsmStreamHeader>* Headers : { &Info.VideoHeaders, &Info.AlphaHeaders, &Info.AudioHeaders }) {
		for (const FUsmStreamHeader& Header : *Headers) {
			if (Header.Table.Rows.Num() > 0) {
				MaxChunk = FMath::Max(MaxChunk, FUsmInfo::GetInt(Header.Table.Rows[0], TEXT("ixsize")));
			}
		}
	}
	if (MaxChunk <= 0) {
		MaxChunk = Info.LargestChunkSize;
	}
	if (int32* MaxChunkSize = PropertyPtr<int32>(Movie, TEXT("MaxChunkSize"))) {
		*MaxChunkSize = (int32)MaxChunk;
	}
	if (int32* NumSubtitleChannels = PropertyPtr<int32>(Movie, TEXT("NumSubtitleChannels"))) {
		*NumSubtitleChannels = Info.NumSubtitleChannels;
	}

	UE_LOG(LogManaImport, Display, TEXT("Imported %s: %d video, %d alpha, %d audio track(s), MinBufferSize %lld, MaxChunkSize %lld"),
		*Filename, Info.VideoHeaders.Num(), Info.AlphaHeaders.Num(), Info.AudioHeaders.Num(), MinBuffer, MaxChunk);
	return Movie;
}

UFileManaMovieFactory::UFileManaMovieFactory() {
	SupportedClass = UFileManaMovie::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UFileManaMovieFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) {
	return NewObject<UFileManaMovie>(InParent, InClass, InName, Flags | RF_Transactional);
}

UManaTextureFactory::UManaTextureFactory() {
	SupportedClass = UManaTexture::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UManaTextureFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) {
	return NewObject<UManaTexture>(InParent, InClass, InName, Flags | RF_Transactional);
}
