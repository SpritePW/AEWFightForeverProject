#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"

class USoundWaveProcedural;

class FAcbPreviewPlayer
{
public:
	~FAcbPreviewPlayer();

	bool Play(const FString& WavFile, int64 StartFrame, bool bLoop, int64 LoopStart, int64 LoopEnd, float InGain, FString& OutError);

	void Stop();

	void SetGain(float InGain) { Gain = FMath::Max(InGain, 0.f); }

	void Seek(int64 Frame);

	void SetPaused(bool bInPaused);

	bool IsPaused() const { return bPaused; }

	int64 GetPositionFrames() const;

	static bool ReadPeaks(const FString& WavFile, int32 Buckets, TArray<float>& OutPeaks);

	void GetPeaks(int32 Buckets, TArray<float>& OutPeaks) const;

	bool IsPlaying() const;

	int64 GetPositionMs() const;

	int32 GetSampleRate() const { return SampleRate; }

private:
	bool Feed(float DeltaTime);
	int32 Render(TArray<uint8>& Out, int32 Frames);

	TArray<int16> Pcm;
	int32 NumChannels = 0;
	int32 SampleRate = 0;
	int64 NumFrames = 0;
	int64 Cursor = 0;
	bool bLoop = false;
	int64 LoopStart = 0;
	int64 LoopEnd = 0;
	bool bWrapped = false;
	bool bEnded = false;
	float Gain = 1.f;
	bool bPaused = false;
	TStrongObjectPtr<USoundWaveProcedural> Wave;
	FDelegateHandle TickHandle;
};
