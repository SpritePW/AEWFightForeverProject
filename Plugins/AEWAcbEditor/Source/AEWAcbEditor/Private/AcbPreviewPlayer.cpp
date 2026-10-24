#include "AcbPreviewPlayer.h"

#include "Components/AudioComponent.h"
#include "Editor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"

namespace AcbPreviewLocal
{
	constexpr float QueueSeconds = 0.25f;

	uint32 ReadU32(const uint8* P) { return uint32(P[0]) | (uint32(P[1]) << 8) | (uint32(P[2]) << 16) | (uint32(P[3]) << 24); }
	uint16 ReadU16(const uint8* P) { return uint16(P[0] | (P[1] << 8)); }

	bool ReadWav(const TArray<uint8>& Bytes, TArray<int16>& OutPcm, int32& OutChannels, int32& OutRate, FString& OutError)
	{
		if (Bytes.Num() < 12 || FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) != 0)
		{
			OutError = TEXT("not a WAVE file");
			return false;
		}
		int32 Format = 0;
		int32 Bits = 0;
		int64 Pos = 12;
		while (Pos + 8 <= Bytes.Num())
		{
			const uint8* Chunk = Bytes.GetData() + Pos;
			const int64 Size = ReadU32(Chunk + 4);
			const int64 Body = Pos + 8;
			if (Body + Size > Bytes.Num())
			{
				break;
			}
			if (FMemory::Memcmp(Chunk, "fmt ", 4) == 0 && Size >= 16)
			{
				Format = ReadU16(Bytes.GetData() + Body);
				OutChannels = ReadU16(Bytes.GetData() + Body + 2);
				OutRate = int32(ReadU32(Bytes.GetData() + Body + 4));
				Bits = ReadU16(Bytes.GetData() + Body + 14);
			}
			else if (FMemory::Memcmp(Chunk, "data", 4) == 0)
			{
				if (Format != 1 || Bits != 16 || OutChannels <= 0 || OutRate <= 0)
				{
					OutError = FString::Printf(TEXT("unsupported WAVE format (format %d, %d bits, %d channels)"), Format, Bits, OutChannels);
					return false;
				}
				OutPcm.SetNumUninitialized(int32(Size / 2));
				FMemory::Memcpy(OutPcm.GetData(), Bytes.GetData() + Body, OutPcm.Num() * 2);
				return true;
			}
			Pos = Body + Size + (Size & 1);
		}
		OutError = TEXT("no audio data in the WAVE file");
		return false;
	}

	void ComputePeaks(const TArray<int16>& Pcm, int32 Channels, int32 Buckets, TArray<float>& OutPeaks)
	{
		OutPeaks.Reset();
		const int64 Frames = Channels > 0 ? Pcm.Num() / Channels : 0;
		if (Frames <= 0 || Buckets <= 0)
		{
			return;
		}
		OutPeaks.SetNumZeroed(Buckets);
		for (int64 Frame = 0; Frame < Frames; ++Frame)
		{
			const int32 Bucket = int32(Frame * Buckets / Frames);
			const int16* Sample = Pcm.GetData() + Frame * Channels;
			int32 Peak = 0;
			for (int32 Channel = 0; Channel < Channels; ++Channel)
			{
				Peak = FMath::Max(Peak, FMath::Abs(int32(Sample[Channel])));
			}
			OutPeaks[Bucket] = FMath::Max(OutPeaks[Bucket], float(Peak) / 32768.f);
		}
	}
}

bool FAcbPreviewPlayer::ReadPeaks(const FString& WavFile, int32 Buckets, TArray<float>& OutPeaks)
{
	OutPeaks.Reset();
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *WavFile))
	{
		return false;
	}
	TArray<int16> Samples;
	int32 Channels = 0;
	int32 Rate = 0;
	FString Error;
	if (!AcbPreviewLocal::ReadWav(Bytes, Samples, Channels, Rate, Error))
	{
		return false;
	}
	AcbPreviewLocal::ComputePeaks(Samples, Channels, Buckets, OutPeaks);
	return OutPeaks.Num() > 0;
}

void FAcbPreviewPlayer::GetPeaks(int32 Buckets, TArray<float>& OutPeaks) const
{
	AcbPreviewLocal::ComputePeaks(Pcm, NumChannels, Buckets, OutPeaks);
}

void FAcbPreviewPlayer::Seek(int64 Frame)
{
	if (!Wave.IsValid())
	{
		return;
	}
	Wave->ResetAudio();
	Cursor = FMath::Clamp<int64>(Frame, 0, NumFrames);
	bWrapped = false;
	bEnded = false;
	Feed(0.f);
}

void FAcbPreviewPlayer::SetPaused(bool bInPaused)
{
	if (!Wave.IsValid() || !GEditor)
	{
		return;
	}
	if (UAudioComponent* Component = GEditor->GetPreviewAudioComponent())
	{
		if (Component->Sound == Wave.Get())
		{
			Component->SetPaused(bInPaused);
			bPaused = bInPaused;
		}
	}
}

FAcbPreviewPlayer::~FAcbPreviewPlayer()
{
	Stop();
}

bool FAcbPreviewPlayer::Play(const FString& WavFile, int64 InStartFrame, bool bInLoop, int64 InLoopStart, int64 InLoopEnd, float InGain, FString& OutError)
{
	Stop();
	if (!GEditor)
	{
		OutError = TEXT("No editor to play the preview in.");
		return false;
	}
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *WavFile))
	{
		OutError = TEXT("Could not read ") + WavFile;
		return false;
	}
	FString WavError;
	if (!AcbPreviewLocal::ReadWav(Bytes, Pcm, NumChannels, SampleRate, WavError))
	{
		OutError = FPaths::GetCleanFilename(WavFile) + TEXT(": ") + WavError;
		return false;
	}
	NumFrames = Pcm.Num() / NumChannels;
	SetGain(InGain);
	bLoop = bInLoop && InLoopEnd > InLoopStart && InLoopStart >= 0;
	LoopEnd = FMath::Min(InLoopEnd, NumFrames);
	LoopStart = FMath::Clamp<int64>(InLoopStart, 0, FMath::Max<int64>(LoopEnd - 1, 0));
	bLoop = bLoop && LoopEnd > LoopStart;
	Cursor = FMath::Clamp<int64>(InStartFrame, 0, NumFrames);
	bEnded = false;
	bWrapped = false;

	USoundWaveProcedural* NewWave = NewObject<USoundWaveProcedural>();
	NewWave->SetSampleRate(uint32(SampleRate));
	NewWave->NumChannels = NumChannels;
	NewWave->Duration = 10000.f;
	NewWave->SoundGroup = SOUNDGROUP_Default;
	NewWave->bLooping = false;
	Wave.Reset(NewWave);

	Feed(0.f);
	GEditor->PlayPreviewSound(NewWave);
	TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAcbPreviewPlayer::Feed), 0.02f);
	return true;
}

void FAcbPreviewPlayer::Stop()
{
	if (TickHandle.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	if (Wave.IsValid())
	{
		if (GEditor)
		{
			if (UAudioComponent* Component = GEditor->GetPreviewAudioComponent())
			{
				if (Component->Sound == Wave.Get())
				{
					if (bPaused)
					{
						Component->SetPaused(false);
					}
					Component->Stop();
					Component->SetSound(nullptr);
				}
			}
		}
		Wave->ResetAudio();
		Wave.Reset();
	}
	bPaused = false;
	Pcm.Empty();
	NumFrames = 0;
	SampleRate = 0;
	NumChannels = 0;
}

int32 FAcbPreviewPlayer::Render(TArray<uint8>& Out, int32 Frames)
{
	Out.Reset();
	int32 Done = 0;
	while (Done < Frames)
	{
		const int64 End = bLoop ? LoopEnd : NumFrames;
		if (Cursor >= End)
		{
			if (!bLoop)
			{
				break;
			}
			Cursor = LoopStart;
			bWrapped = true;
		}
		const int32 Count = int32(FMath::Min<int64>(Frames - Done, End - Cursor));
		const int32 Offset = Out.Num();
		Out.Append(reinterpret_cast<const uint8*>(Pcm.GetData() + Cursor * NumChannels), Count * NumChannels * int32(sizeof(int16)));
		if (!FMath::IsNearlyEqual(Gain, 1.f))
		{
			int16* Samples = reinterpret_cast<int16*>(Out.GetData() + Offset);
			for (int32 Index = 0; Index < Count * NumChannels; ++Index)
			{
				Samples[Index] = int16(FMath::Clamp(FMath::RoundToInt(float(Samples[Index]) * Gain), -32768, 32767));
			}
		}
		Cursor += Count;
		Done += Count;
	}
	return Done;
}

bool FAcbPreviewPlayer::Feed(float)
{
	if (!Wave.IsValid())
	{
		TickHandle.Reset();
		return false;
	}
	const int32 BytesPerFrame = NumChannels * int32(sizeof(int16));
	const int32 Target = int32(AcbPreviewLocal::QueueSeconds * SampleRate) * BytesPerFrame;
	TArray<uint8> Block;
	while (!bEnded && Wave->GetAvailableAudioByteCount() < Target)
	{
		if (Render(Block, 4096) == 0)
		{
			bEnded = true;
			break;
		}
		Wave->QueueAudio(Block.GetData(), Block.Num());
	}
	if (bEnded && Wave->GetAvailableAudioByteCount() == 0)
	{
		TickHandle.Reset();
		Stop();
		return false;
	}
	return true;
}

bool FAcbPreviewPlayer::IsPlaying() const
{
	return Wave.IsValid();
}

int64 FAcbPreviewPlayer::GetPositionMs() const
{
	const int64 Frames = GetPositionFrames();
	return Frames < 0 ? -1 : Frames * 1000 / SampleRate;
}

int64 FAcbPreviewPlayer::GetPositionFrames() const
{
	if (!Wave.IsValid() || SampleRate <= 0)
	{
		return -1;
	}
	const int32 BytesPerFrame = NumChannels * int32(sizeof(int16));
	int64 Heard = Cursor - int64(const_cast<USoundWaveProcedural*>(Wave.Get())->GetAvailableAudioByteCount() / FMath::Max(BytesPerFrame, 1));
	if (bLoop && bWrapped && Heard < LoopStart)
	{
		Heard += LoopEnd - LoopStart;
	}
	return FMath::Clamp<int64>(Heard, 0, NumFrames);
}
