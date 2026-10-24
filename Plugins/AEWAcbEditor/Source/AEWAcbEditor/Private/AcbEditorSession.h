#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace AcbCore
{
	class AcbDocument;
	struct TrackOptions;
}
class UAcbEditorSettings;
struct FAcbCueTemplate;

DECLARE_LOG_CATEGORY_EXTERN(LogAEWAcbEditor, Log, All);

struct FAcbCueRow
{
	int32 Index = 0;
	uint32 Id = 0;
	FString Name;
	FString Type;
	FString PlayMode;
	FString Categories;
	int32 NumWaveforms = 0;
	FString Length;
	FString Audio;
	bool bLoop = false;
};

struct FAcbSlotTiming
{
	bool bValid = false;
	int32 SamplingRate = 0;
	int64 NumSamples = 0;
	bool bLoop = false;
	bool bWaveformLoops = false;
	int64 LoopStart = 0;
	int64 LoopEnd = 0;
	bool bCanSetStart = false;
	bool bHasStart = false;
	int64 StartMs = 0;
	int32 SharedBy = 0;
};

struct FAcbSaveResult
{
	bool bOk = false;
	FString AcbPath;
	FString AwbPath;
	FString Error;
	FString Cleanup;
};

class FAcbEditorSession
{
public:
	FAcbEditorSession();
	~FAcbEditorSession();

	bool Open(const FString& AcbPath, const FString& AwbPath, FString& OutError);
	void Close();
	bool IsOpen() const;

	AcbCore::AcbDocument& Doc() const;
	const FString& GetAcbPath() const { return LoadedAcb; }
	const FString& GetAwbPath() const { return LoadedAwb; }

	bool DecodeSlotAudio(int32 CueIndex, int32 Slot, FString& OutWav, FString& OutError, bool bOnlyIfDecoded = false) const;

	static FString GetVGAudioCli();

	TArray<TSharedPtr<FAcbCueRow>> BuildRows() const;
	TArray<FString> GetCategories() const;
	TArray<FString> DescribeSlots(int32 CueIndex) const;
	int32 FindCue(const FString& Name) const;
	FString GetCueSheetName() const;
	FString Describe() const;
	uint64 EstimateOutputAcbSize(const UAcbEditorSettings& Settings) const;

	bool Edit(TFunctionRef<void(AcbCore::AcbDocument&)> Operation, FString& OutError);

	bool PrepareAudio(const FString& InPath, FString& OutPath, FString& OutError) const;

	static AcbCore::TrackOptions MakeTrackOptions(bool bLoop);
	static FString ResolveCueName(const FString& Pattern, const FString& BaseName);

	bool AddTrack(const FString& AudioFile, const TArray<FAcbCueTemplate>& Cues,
		int32 TemplateCue, bool bLoop, FString& OutMessage, TArray<FString>* OutNames = nullptr);

	bool AddCueFromAudio(int32 SourceIndex, const FString& Name, const FString& Category, FString& OutMessage);

	bool PointCuesAtAudio(const TArray<int32>& TargetIndices, int32 SourceIndex, FString& OutMessage);

	bool AddVariations(int32 CueIndex, const TArray<FString>& AudioFiles, int32 PlayMode, FString& OutMessage);
	bool ReplaceVariation(int32 CueIndex, int32 Slot, const FString& AudioFile, FString& OutMessage);
	bool RemoveVariation(int32 CueIndex, int32 Slot, FString& OutMessage);
	bool SetPlayMode(int32 CueIndex, uint8 PlayMode, FString& OutMessage);

	FAcbSlotTiming GetSlotTiming(int32 CueIndex, int32 Slot) const;
	bool SetSlotTiming(int32 CueIndex, int32 Slot, TOptional<int64> StartMs, bool bLoop, int64 LoopStart, int64 LoopEnd, FString& OutMessage);

	FAcbSaveResult Save(const UAcbEditorSettings& Settings);

	static FString FindSiblingAwb(const FString& AcbPath);
	static std::string ToUtf8(const FString& Text);
	static FString FromUtf8(const std::string& Text);
	static constexpr uint64 CriImportLimit = 0x7FFFFFFFull;

private:
	std::unique_ptr<AcbCore::AcbDocument> Document;
	FString LoadedAcb;
	FString LoadedAwb;
};
