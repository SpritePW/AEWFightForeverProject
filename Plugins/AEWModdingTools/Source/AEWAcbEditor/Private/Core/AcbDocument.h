#pragma once

#include "Afs2.h"
#include "AudioProbe.h"
#include "CriUtf.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AcbCore
{
	enum class CueReferenceType : uint8_t
	{
		Waveform = 1,
		Synth = 2,
		Sequence = 3
	};

	enum class SequenceType : uint8_t
	{
		Polyphonic = 0,
		Sequential = 1,
		Shuffle = 2,
		Random = 3,
		RandomNoRepeat = 4,
		Switch = 5,
		ComboSequential = 6
	};
	const char* SequenceTypeName(uint8_t Type);
	bool SequenceTypeUsesWeights(uint8_t Type);

	struct CueTrack
	{
		size_t TrackIndex = 0;
		std::optional<size_t> EventIndex;
		std::optional<size_t> SynthIndex;
		std::vector<size_t> WaveformIndices;
	};

	struct CueChain
	{
		std::optional<size_t> SequenceIndex;
		std::optional<uint8_t> SequenceType;
		std::vector<CueTrack> Tracks;
		std::optional<size_t> TrackIndex;
		std::optional<size_t> TrackEventIndex;
		std::optional<size_t> SynthIndex;
		std::optional<size_t> WaveformIndex;
		std::vector<size_t> WaveformIndices;
	};

	struct WaveformInfo
	{
		uint8_t Streaming = 0;
		uint8_t Encode = 0;
		uint32_t AwbId = 0;
		uint32_t NumChannels = 0;
		uint32_t SamplingRate = 0;
		uint32_t NumSamples = 0;
		bool bLoop = false;
	};

	struct SlotTiming
	{
		size_t WaveformIndex = 0;
		uint32_t SamplingRate = 0;
		uint32_t NumSamples = 0;
		bool bLoop = false;
		bool bWaveformLoops = false;
		bool bIgnoreLoop = false;
		uint32_t LoopStart = 0;
		uint32_t LoopEnd = 0;
		bool bCanSetStartOffset = false;
		std::optional<uint32_t> StartOffsetMs;
	};

	struct TrackOptions
	{
		std::optional<bool> Streaming;
		bool bLoop = false;
		std::optional<EncodeType> Encode;
		std::optional<uint32_t> NumChannels;
		std::optional<uint32_t> SamplingRate;
		std::optional<uint32_t> NumSamples;
		std::optional<uint32_t> LoopStart;
		std::optional<uint32_t> LoopEnd;
		std::optional<uint32_t> AwbEntryId;
		std::optional<uint16_t> TrackCommandIndex;
		std::optional<uint16_t> SequenceCommandIndex;
		std::optional<uint16_t> SynthCommandIndex;
		std::string Category;
		SequenceType Variations = SequenceType::RandomNoRepeat;
	};

	struct CueSpec
	{
		std::string Name;
		std::string Category;
		std::optional<size_t> TemplateCue;
	};

	struct SequenceCommandInfo
	{
		size_t Index = 0;
		std::vector<std::string> Categories;
		std::string Label;
		size_t UsedBy = 0;
	};

	struct CleanupResult
	{
		size_t Sequences = 0;
		size_t Tracks = 0;
		size_t Events = 0;
		size_t Synths = 0;
		size_t Waveforms = 0;
		size_t Extensions = 0;
		size_t MemoryAudio = 0;
		size_t StreamAudio = 0;
		uint64_t AudioBytes = 0;
		std::string Skipped;

		bool Any() const { return Sequences + Tracks + Events + Synths + Waveforms + Extensions + MemoryAudio + StreamAudio > 0; }
	};

	struct AcbSaveOptions
	{
		std::string AcbPath;
		std::string AwbPath;
		bool bForcePerRow = false;
		bool bUpdateStreamAwbInfo = true;
		ProgressCallback OnProgress = nullptr;
		void* ProgressUser = nullptr;
	};

	class AcbDocument
	{
	public:
		static std::unique_ptr<AcbDocument> Load(const std::string& AcbPath, const std::string& AwbPath = std::string());
		static uint64_t FindAcbOffset(ReadStream& Stream);

		const std::string& SourceAcbPath() const { return AcbPathLoaded; }
		const std::string& SourceAwbPath() const { return AwbPathLoaded; }
		uint64_t SourceOffset() const { return AcbOffset; }

		UtfTable& Header() { return *HeaderTable; }
		const UtfTable& Header() const { return *HeaderTable; }
		UtfTable* FindTable(const std::string& HeaderColumn) const;

		Afs2Archive* StreamAwb() const { return StreamAwbArchive.get(); }
		Afs2Archive* MemoryAwb() const;
		bool UsesStreamingAwb() const;

		std::string CueSheetName() const;

		size_t CueCount() const;
		uint32_t CueId(size_t CueIndex) const;
		std::string CueName(size_t CueIndex) const;
		CueReferenceType CueRefType(size_t CueIndex) const;
		uint32_t CueRefIndex(size_t CueIndex) const;
		uint32_t CueLength(size_t CueIndex) const;
		std::optional<size_t> FindCueByName(const std::string& Name) const;
		std::optional<size_t> FindCueById(uint32_t Id) const;
		CueChain ResolveChain(size_t CueIndex) const;
		std::vector<std::string> CueCategories(size_t CueIndex) const;
		size_t WaveformCount() const;
		WaveformInfo Waveform(size_t WaveformIndex) const;
		std::map<std::string, uint32_t> ListCategories() const;
		std::vector<SequenceCommandInfo> ListSequenceCommands() const;
		std::vector<std::string> CategoryLabels() const;

		size_t AddWaveform(uint32_t AwbId, bool bStreaming, const std::string& AudioPath, const TrackOptions& Options);
		uint32_t AddCue(const std::string& Name, size_t WaveformIndex, uint16_t TrackCommandIndex = 0,
			uint16_t SequenceCommandIndex = 0, const std::string& Category = std::string());
		uint32_t AddTrack(const std::string& Name, const std::string& AudioPath, const TrackOptions& Options);
		std::vector<uint32_t> AddTrackCues(const std::vector<std::string>& AudioPaths, const std::vector<CueSpec>& Cues,
			const TrackOptions& Options);
		uint32_t AddCueSynthDirect(const std::string& Name, size_t WaveformIndex);

		size_t AddWaveformToCue(size_t CueIndex, const std::string& AudioPath, const TrackOptions& Options,
			std::optional<SequenceType> Type = std::nullopt);
		size_t ReplaceWaveformOnCue(size_t CueIndex, const std::string& AudioPath, size_t Slot, const TrackOptions& Options);
		void SetCueWaveform(size_t CueIndex, size_t WaveformIndex, size_t Slot = 0);
		void RemoveWaveformFromCue(size_t CueIndex, size_t Slot);
		void UseCueAudio(size_t TargetCue, size_t SourceCue);
		void SetCueSequenceType(size_t CueIndex, SequenceType Type);
		SlotTiming GetSlotTiming(size_t CueIndex, size_t Slot) const;
		void SetSlotStartOffset(size_t CueIndex, size_t Slot, std::optional<uint32_t> StartMs);
		size_t CountCuesUsingWaveform(size_t WaveformIndex) const;
		void SetSlotTiming(size_t CueIndex, size_t Slot, std::optional<uint32_t> StartMs, bool bLoop, uint32_t LoopStart, uint32_t LoopEnd);
		void SetSlotLoop(size_t CueIndex, size_t Slot, bool bLoop, uint32_t LoopStart, uint32_t LoopEnd);

		std::vector<size_t> CuesUsingSynth(size_t SynthIndex) const;

		uint32_t AddCueForCueAudio(size_t SourceCue, const std::string& Name, const std::string& Category);
		uint32_t CopyCue(size_t CueIndex, const std::string& NewName = std::string());
		void RenameCue(size_t CueIndex, const std::string& NewName);
		void SetCueCategory(size_t CueIndex, const std::string& Category);
		std::optional<uint16_t> CueVolume(size_t CueIndex) const;
		std::optional<uint16_t> SlotVolume(size_t CueIndex, size_t Slot) const;
		void SetCueVolume(size_t CueIndex, std::optional<uint16_t> Volume);
		void SetSlotVolume(size_t CueIndex, size_t Slot, std::optional<uint16_t> Volume);
		void DeleteCue(size_t CueIndex);
		void DeleteCues(std::vector<size_t> CueIndices);
		void MergeAwbIntoAcb(bool bConvertStreamingWaveforms = true);
		CleanupResult RemoveUnusedData();

		bool IsModified() const;
		uint64_t EstimateAcbSize(bool bForcePerRow = false) const;
		uint64_t EstimateAwbSize() const;
		bool IsStreamAwbModified() const { return bStreamAwbModified; }
		void Save(const AcbSaveOptions& Options);

		static std::string SanitizeCueName(const std::string& Name);

	private:
		std::shared_ptr<ReadStream> AcbStream;
		std::shared_ptr<UtfTable> HeaderTable;
		std::shared_ptr<Afs2Archive> StreamAwbArchive;
		std::string AcbPathLoaded;
		std::string AwbPathLoaded;
		uint64_t AcbOffset = 0;
		bool bStreamAwbModified = false;
		bool bMemoryAwbModified = false;
		size_t StreamAwbLoadedCount = 0;

		struct PreparedWaveform
		{
			EncodeType Encode = EncodeType::HCA;
			uint32_t NumChannels = 0;
			uint32_t SamplingRate = 0;
			uint32_t NumSamples = 0;
			bool bLoop = false;
			uint32_t LoopStart = 0;
			uint32_t LoopEnd = 0;
		};
		struct ChainSettings
		{
			uint16_t SequenceCommand = 0;
			uint16_t TrackCommand = 0;
			uint16_t SynthCommand = 0;
			uint8_t SingleTrackType = 0;
		};
		struct LiveRows
		{
			std::vector<char> Sequences, Tracks, Events, Synths, Waveforms;
			std::string Unknown;
			std::string Blocked;
		};
		LiveRows MarkLive(const std::vector<char>& CueAlive) const;

		struct RefCounts
		{
			std::vector<size_t> Sequences, Tracks, Events, Synths;
		};
		struct Slot
		{
			std::optional<size_t> TrackPosition;
			size_t ItemPosition = 0;
		};

		UtfTable& Require(const std::string& HeaderColumn) const;
		Afs2Archive& TargetAwb(bool bStreaming);
		bool ResolveStreaming(const TrackOptions& Options) const;
		void RequireTargetAwb(bool bStreaming) const;
		PreparedWaveform PrepareWaveform(const std::string& AudioPath, const TrackOptions& Options) const;
		size_t CommitWaveform(uint32_t AwbId, bool bStreaming, const PreparedWaveform& Prepared);
		uint32_t AddAudioEntry(const std::string& AudioPath, bool bStreaming, std::optional<uint32_t> EntryId);
		std::vector<size_t> AddAudioFiles(const std::vector<std::string>& AudioPaths, const TrackOptions& Options);

		size_t AddSynth(size_t WaveformIndex, uint16_t CommandIndex);
		size_t AddTrackEvent(size_t SynthIndex);
		size_t AddTrackRow(size_t EventIndex, uint16_t CommandIndex);
		size_t AddSequence(const std::vector<size_t>& TrackIndices, uint16_t CommandIndex, uint8_t Type);
		size_t AddTrackChain(size_t WaveformIndex, uint16_t SynthCommand, uint16_t TrackCommand);
		size_t SequenceWorkArea2For(size_t NewIndex) const;
		uint16_t MostCommonValue(const std::string& HeaderColumn, const std::string& Column, uint16_t Fallback) const;
		ChainSettings ResolveChainSettings(const std::string& Category, std::optional<size_t> TemplateCue, const TrackOptions& Options) const;

		uint32_t ComputeLengthMs(size_t WaveformIndex) const;
		void RecomputeLength(size_t CueIndex);
		uint32_t NextCueId() const;
		uint16_t SequenceCommandIndexForCategory(const std::string& Category) const;
		std::vector<uint32_t> CategoryIdsOfCommand(size_t CommandRow) const;
		void InsertCueName(const std::string& Name, size_t CueIndex);
		void RequireUniqueName(const std::string& Name) const;
		std::vector<size_t> WaveformsFromSynth(size_t SynthIndex) const;
		std::optional<size_t> SynthOfEvent(size_t EventIndex) const;
		uint32_t AppendCueRow(const std::string& Name, CueReferenceType Type, size_t ReferenceIndex, uint32_t Length,
			size_t NumWaveforms, std::optional<size_t> TemplateCue);

		std::vector<uint16_t> SequenceTracks(size_t SequenceIndex) const;
		void SetSequenceTracks(size_t SequenceIndex, const std::vector<uint16_t>& Tracks, std::optional<uint8_t> NewType);
		RefCounts CountReferences() const;
		std::vector<Slot> SlotsOf(size_t CueIndex) const;
		size_t MakeSequencePrivate(size_t CueIndex);
		size_t MakeSlotSynthPrivate(size_t CueIndex, const Slot& Where);
		void SetSlotWaveform(size_t CueIndex, size_t SlotIndex, size_t WaveformIndex);
		std::optional<size_t> SlotEvent(size_t CueIndex, const Slot& Where) const;
		void CheckSlotStartOffset(const SlotTiming& Current, size_t CueIndex, std::optional<uint32_t> StartMs) const;
		void CheckSlotLoop(const SlotTiming& Current, bool bLoop, uint32_t LoopStart, uint32_t LoopEnd) const;
		size_t SlotSynth(size_t CueIndex, const Slot& Where) const;
		size_t SlotWaveform(size_t CueIndex, const Slot& Where) const;
		void SetSynthIgnoreLoop(size_t SynthIndex, bool bIgnore);
		bool SynthIgnoresLoop(size_t SynthIndex) const;
		size_t WaveformReferenceCount(size_t WaveformIndex) const;
		void UpdateStreamAwbInfo(const std::vector<uint8_t>& AwbHeader, const std::array<uint8_t, 16>& AwbMd5);
	};
}
