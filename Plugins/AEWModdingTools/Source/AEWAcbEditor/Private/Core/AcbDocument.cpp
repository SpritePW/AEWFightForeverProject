#include "AcbDocument.h"
#include "AcbCommands.h"
#include "Md5.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <set>

namespace AcbCore
{
	namespace
	{
		const char* const HeaderCue = "CueTable";
		const char* const HeaderCueName = "CueNameTable";
		const char* const HeaderWaveform = "WaveformTable";
		const char* const HeaderSynth = "SynthTable";
		const char* const HeaderSequence = "SequenceTable";
		const char* const HeaderTrack = "TrackTable";
		const char* const HeaderTrackEvent = "TrackEventTable";
		const char* const HeaderSequenceCommand = "SeqCommandTable";
		const char* const HeaderSynthCommand = "SynthCommandTable";
		const char* const HeaderTrackCommand = "TrackCommandTable";
		const char* const HeaderWaveformExtension = "WaveformExtensionDataTable";
		const char* const HeaderAcfReference = "AcfReferenceTable";
		const char* const HeaderAwbFile = "AwbFile";
		const char* const HeaderStreamAwbHeader = "StreamAwbAfs2Header";
		const char* const HeaderStreamAwbHash = "StreamAwbHash";
		const char* const HeaderStreamAwbTocWork = "StreamAwbTocWork";

		const uint16_t ReferenceItemWaveform = 1;
		const uint16_t TrackEventRefSynth = 2;
		const uint32_t CategoryAcfType = 3;
		const size_t TocWorkBytesPerEntry = 7;

		UtfValue CellU(uint64_t Value) { return UtfValue::Integer(UtfType::U64, Value); }
		UtfValue CellS(const std::string& Value) { return UtfValue::String(Value); }
		UtfValue CellB(const std::vector<uint8_t>& Value) { return UtfValue::Blob(Value); }

		uint16_t CheckU16(size_t Value, const char* What)
		{
			if (Value > 0xFFFF)
			{
				throw AcbError(std::string(What) + " index " + std::to_string(Value) + " does not fit in 16 bits");
			}
			return uint16_t(Value);
		}

		std::vector<uint8_t> PackU16Array(const std::vector<uint16_t>& Values)
		{
			std::vector<uint8_t> Bytes;
			for (uint16_t Value : Values)
			{
				PutBE16(Bytes, Value);
			}
			return Bytes;
		}

		std::vector<uint16_t> UnpackU16Array(const std::vector<uint8_t>& Bytes)
		{
			std::vector<uint16_t> Values;
			for (size_t Pos = 0; Pos + 2 <= Bytes.size(); Pos += 2)
			{
				Values.push_back(ReadBE16(Bytes.data() + Pos));
			}
			return Values;
		}

		using ReferenceItem = std::pair<uint16_t, uint16_t>;

		std::vector<uint8_t> PackReferenceItems(const std::vector<ReferenceItem>& Items)
		{
			std::vector<uint8_t> Bytes;
			for (const ReferenceItem& Item : Items)
			{
				PutBE16(Bytes, Item.first);
				PutBE16(Bytes, Item.second);
			}
			return Bytes;
		}

		std::vector<ReferenceItem> UnpackReferenceItems(const std::vector<uint8_t>& Bytes)
		{
			std::vector<ReferenceItem> Items;
			for (size_t Pos = 0; Pos + 4 <= Bytes.size(); Pos += 4)
			{
				Items.emplace_back(ReadBE16(Bytes.data() + Pos), ReadBE16(Bytes.data() + Pos + 2));
			}
			return Items;
		}

		std::vector<uint8_t> WithVolume(const std::vector<uint8_t>& Bytes, std::optional<uint16_t> Volume)
		{
			std::vector<AcbCommand> List = DecodeCommands(Bytes);
			List.erase(std::remove_if(List.begin(), List.end(), [](const AcbCommand& Command) { return Command.Op == AcbOp::Volume; }), List.end());
			if (Volume)
			{
				AcbCommand Command;
				Command.Op = AcbOp::Volume;
				PutBE16(Command.Params, *Volume);
				auto Terminator = std::find_if(List.begin(), List.end(), [](const AcbCommand& C) { return C.Op == AcbOp::Null; });
				List.insert(Terminator, Command);
			}
			return EncodeCommands(List);
		}

		std::optional<uint16_t> VolumeOf(const UtfTable* Commands, uint64_t Row)
		{
			if (!Commands || Row >= Commands->RowCount())
			{
				return std::nullopt;
			}
			const std::optional<uint64_t> Value = FindCommandValue(Commands->GetBytes(size_t(Row), "Command"), AcbOp::Volume);
			return Value ? std::optional<uint16_t>(uint16_t(*Value)) : std::nullopt;
		}

		size_t FindOrAddCommandRow(UtfTable& Commands, const std::vector<uint8_t>& Wanted, const char* What)
		{
			for (size_t Candidate = 0; Candidate < Commands.RowCount(); ++Candidate)
			{
				if (Commands.GetBytes(Candidate, "Command") == Wanted)
				{
					return Candidate;
				}
			}
			CheckU16(Commands.RowCount(), What);
			return Commands.AddRow({ { "Command", CellB(Wanted) } });
		}

		bool IsRandomType(uint8_t Type)
		{
			return Type == uint8_t(SequenceType::Shuffle) || Type == uint8_t(SequenceType::Random)
				|| Type == uint8_t(SequenceType::RandomNoRepeat);
		}

		const uint16_t DefaultTrackWeight = 100;

		std::string NormalizePath(std::string Path)
		{
			for (char& C : Path)
			{
				C = C == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(C)));
			}
			return Path;
		}
	}

	const char* SequenceTypeName(uint8_t Type)
	{
		switch (Type)
		{
		case 0: return "Polyphonic (all at once)";
		case 1: return "Sequential";
		case 2: return "Shuffle";
		case 3: return "Random";
		case 4: return "Random, no repeat";
		case 5: return "Switch";
		case 6: return "Combo sequential";
		default: return "Other";
		}
	}

	bool SequenceTypeUsesWeights(uint8_t Type)
	{
		return IsRandomType(Type);
	}

	uint64_t AcbDocument::FindAcbOffset(ReadStream& Stream)
	{
		const uint64_t Size = Stream.Size();
		const size_t ChunkSize = 4u << 20;
		std::vector<uint8_t> Chunk;
		for (uint64_t Base = 0; Base < Size; Base += ChunkSize)
		{
			const size_t Count = size_t(std::min<uint64_t>(ChunkSize + 3, Size - Base));
			Chunk = Stream.ReadVector(Base, Count);
			for (size_t Pos = 0; Pos + 4 <= Count; ++Pos)
			{
				if (Chunk[Pos] != '@' || Chunk[Pos + 1] != 'U' || Chunk[Pos + 2] != 'T' || Chunk[Pos + 3] != 'F')
				{
					continue;
				}
				const uint64_t At = Base + Pos;
				if (At + 0x20 > Size)
				{
					continue;
				}
				uint8_t Fixed[0x20];
				Stream.ReadAt(At, Fixed, sizeof(Fixed));
				const uint64_t Total = uint64_t(ReadBE32(Fixed + 4)) + 8;
				const uint32_t StringsOffset = ReadBE32(Fixed + 12);
				const int32_t NameOffset = int32_t(ReadBE32(Fixed + 20));
				if (At + Total > Size || NameOffset < 0 || 8 + uint64_t(StringsOffset) + uint64_t(NameOffset) + 7 > Total)
				{
					continue;
				}
				char Name[7];
				Stream.ReadAt(At + 8 + StringsOffset + uint32_t(NameOffset), Name, sizeof(Name));
				if (std::memcmp(Name, "Header", 7) == 0)
				{
					return At;
				}
			}
		}
		throw AcbError("No ACB found in the file");
	}

	std::unique_ptr<AcbDocument> AcbDocument::Load(const std::string& AcbPath, const std::string& AwbPath)
	{
		auto Document = std::unique_ptr<AcbDocument>(new AcbDocument());
		Document->AcbPathLoaded = AcbPath;
		Document->AcbStream = OpenFileRead(AcbPath);
		Document->AcbOffset = FindAcbOffset(*Document->AcbStream);
		Document->HeaderTable = UtfTable::ParseStream(Document->AcbStream, Document->AcbOffset);
		if (Document->HeaderTable->RowCount() != 1)
		{
			throw AcbError(AcbPath + " is not an ACB (header table must have one row)");
		}
		Document->Require(HeaderCue);
		Document->Require(HeaderCueName);
		Document->Require(HeaderWaveform);
		if (!AwbPath.empty())
		{
			Document->StreamAwbArchive = Afs2Archive::Open(AwbPath);
			Document->AwbPathLoaded = AwbPath;
			Document->StreamAwbLoadedCount = Document->StreamAwbArchive->Entries.size();
		}
		return Document;
	}

	UtfTable* AcbDocument::FindTable(const std::string& HeaderColumn) const
	{
		if (!HeaderTable->HasColumn(HeaderColumn))
		{
			return nullptr;
		}
		const UtfValue& Value = HeaderTable->Get(0, HeaderColumn);
		return Value.Type == UtfType::Data ? Value.Data.Table.get() : nullptr;
	}

	UtfTable& AcbDocument::Require(const std::string& HeaderColumn) const
	{
		UtfTable* Table = FindTable(HeaderColumn);
		if (!Table)
		{
			throw AcbError("Table '" + HeaderColumn + "' not found in the ACB");
		}
		return *Table;
	}

	Afs2Archive* AcbDocument::MemoryAwb() const
	{
		if (!HeaderTable->HasColumn(HeaderAwbFile))
		{
			return nullptr;
		}
		return HeaderTable->Get(0, HeaderAwbFile).Data.Awb.get();
	}

	bool AcbDocument::UsesStreamingAwb() const
	{
		const UtfTable& Waveforms = Require(HeaderWaveform);
		for (size_t Row = 0; Row < Waveforms.RowCount(); ++Row)
		{
			if (Waveforms.GetUnsigned(Row, "Streaming") != 0)
			{
				return true;
			}
		}
		return false;
	}

	std::string AcbDocument::CueSheetName() const
	{
		return HeaderTable->HasColumn("Name") ? HeaderTable->GetString(0, "Name") : std::string();
	}

	size_t AcbDocument::CueCount() const
	{
		return Require(HeaderCue).RowCount();
	}

	uint32_t AcbDocument::CueId(size_t CueIndex) const
	{
		return uint32_t(Require(HeaderCue).GetUnsigned(CueIndex, "CueId"));
	}

	std::string AcbDocument::CueName(size_t CueIndex) const
	{
		const UtfTable& Names = Require(HeaderCueName);
		for (size_t Row = 0; Row < Names.RowCount(); ++Row)
		{
			if (Names.GetUnsigned(Row, "CueIndex") == CueIndex)
			{
				return Names.GetString(Row, "CueName");
			}
		}
		return std::string();
	}

	CueReferenceType AcbDocument::CueRefType(size_t CueIndex) const
	{
		return CueReferenceType(Require(HeaderCue).GetUnsigned(CueIndex, "ReferenceType"));
	}

	uint32_t AcbDocument::CueRefIndex(size_t CueIndex) const
	{
		return uint32_t(Require(HeaderCue).GetUnsigned(CueIndex, "ReferenceIndex"));
	}

	uint32_t AcbDocument::CueLength(size_t CueIndex) const
	{
		return uint32_t(Require(HeaderCue).GetUnsigned(CueIndex, "Length"));
	}

	std::optional<size_t> AcbDocument::FindCueByName(const std::string& Name) const
	{
		const UtfTable& Names = Require(HeaderCueName);
		for (size_t Row = 0; Row < Names.RowCount(); ++Row)
		{
			if (Names.GetString(Row, "CueName") == Name)
			{
				return size_t(Names.GetUnsigned(Row, "CueIndex"));
			}
		}
		return std::nullopt;
	}

	std::optional<size_t> AcbDocument::FindCueById(uint32_t Id) const
	{
		const UtfTable& Cues = Require(HeaderCue);
		for (size_t Row = 0; Row < Cues.RowCount(); ++Row)
		{
			if (Cues.GetUnsigned(Row, "CueId") == Id)
			{
				return Row;
			}
		}
		return std::nullopt;
	}

	std::vector<size_t> AcbDocument::WaveformsFromSynth(size_t SynthIndex) const
	{
		const UtfTable& Synths = Require(HeaderSynth);
		std::vector<size_t> Result;
		if (SynthIndex >= Synths.RowCount())
		{
			return Result;
		}
		for (const ReferenceItem& Item : UnpackReferenceItems(Synths.GetBytes(SynthIndex, "ReferenceItems")))
		{
			if (Item.first == ReferenceItemWaveform)
			{
				Result.push_back(Item.second);
			}
		}
		return Result;
	}

	std::optional<size_t> AcbDocument::SynthOfEvent(size_t EventIndex) const
	{
		const UtfTable* Events = FindTable(HeaderTrackEvent);
		if (!Events || EventIndex >= Events->RowCount())
		{
			return std::nullopt;
		}
		const std::optional<uint64_t> Packed = FindCommandValue(Events->GetBytes(EventIndex, "Command"), AcbOp::ReferenceItem);
		if (!Packed || ((*Packed >> 16) & 0xFFFF) != TrackEventRefSynth)
		{
			return std::nullopt;
		}
		return size_t(*Packed & 0xFFFF);
	}

	std::vector<uint16_t> AcbDocument::SequenceTracks(size_t SequenceIndex) const
	{
		return UnpackU16Array(Require(HeaderSequence).GetBytes(SequenceIndex, "TrackIndex"));
	}

	CueChain AcbDocument::ResolveChain(size_t CueIndex) const
	{
		CueChain Chain;
		const CueReferenceType Type = CueRefType(CueIndex);
		const size_t RefIndex = CueRefIndex(CueIndex);
		if (Type == CueReferenceType::Synth)
		{
			Chain.SynthIndex = RefIndex;
			Chain.WaveformIndices = WaveformsFromSynth(RefIndex);
		}
		else if (Type == CueReferenceType::Waveform)
		{
			Chain.WaveformIndices.push_back(RefIndex);
		}
		else if (Type == CueReferenceType::Sequence)
		{
			const UtfTable* Sequences = FindTable(HeaderSequence);
			if (!Sequences || RefIndex >= Sequences->RowCount())
			{
				return Chain;
			}
			Chain.SequenceIndex = RefIndex;
			Chain.SequenceType = uint8_t(Sequences->GetUnsigned(RefIndex, "Type"));
			const UtfTable* Tracks = FindTable(HeaderTrack);
			for (uint16_t TrackIndex : SequenceTracks(RefIndex))
			{
				CueTrack Track;
				Track.TrackIndex = TrackIndex;
				if (Tracks && TrackIndex < Tracks->RowCount())
				{
					Track.EventIndex = size_t(Tracks->GetUnsigned(TrackIndex, "EventIndex"));
					Track.SynthIndex = SynthOfEvent(*Track.EventIndex);
					if (Track.SynthIndex)
					{
						Track.WaveformIndices = WaveformsFromSynth(*Track.SynthIndex);
					}
				}
				Chain.WaveformIndices.insert(Chain.WaveformIndices.end(), Track.WaveformIndices.begin(), Track.WaveformIndices.end());
				Chain.Tracks.push_back(std::move(Track));
			}
			if (!Chain.Tracks.empty())
			{
				Chain.TrackIndex = Chain.Tracks[0].TrackIndex;
				Chain.TrackEventIndex = Chain.Tracks[0].EventIndex;
				Chain.SynthIndex = Chain.Tracks[0].SynthIndex;
			}
		}
		if (!Chain.WaveformIndices.empty())
		{
			Chain.WaveformIndex = Chain.WaveformIndices[0];
		}
		return Chain;
	}

	std::map<std::string, uint32_t> AcbDocument::ListCategories() const
	{
		std::map<std::string, uint32_t> Result;
		const UtfTable* References = FindTable(HeaderAcfReference);
		if (!References)
		{
			return Result;
		}
		for (size_t Row = 0; Row < References->RowCount(); ++Row)
		{
			if (References->GetUnsigned(Row, "Type") == CategoryAcfType)
			{
				Result[References->GetString(Row, "Name")] = uint32_t(References->GetUnsigned(Row, "Id"));
			}
		}
		return Result;
	}

	std::vector<uint32_t> AcbDocument::CategoryIdsOfCommand(size_t CommandRow) const
	{
		std::vector<uint32_t> Ids;
		const UtfTable* Commands = FindTable(HeaderSequenceCommand);
		if (!Commands || CommandRow >= Commands->RowCount())
		{
			return Ids;
		}
		for (const AcbCommand& Command : DecodeCommands(Commands->GetBytes(CommandRow, "Command")))
		{
			if (Command.Op == AcbOp::Category)
			{
				for (size_t Pos = 0; Pos + 4 <= Command.Params.size(); Pos += 4)
				{
					Ids.push_back(ReadBE32(Command.Params.data() + Pos));
				}
			}
		}
		return Ids;
	}

	std::vector<SequenceCommandInfo> AcbDocument::ListSequenceCommands() const
	{
		std::vector<SequenceCommandInfo> Result;
		const UtfTable* Commands = FindTable(HeaderSequenceCommand);
		if (!Commands)
		{
			return Result;
		}
		std::map<uint32_t, std::string> ById;
		for (const auto& Pair : ListCategories())
		{
			ById[Pair.second] = Pair.first;
		}
		std::vector<size_t> Usage(Commands->RowCount(), 0);
		if (const UtfTable* Sequences = FindTable(HeaderSequence))
		{
			for (size_t Row = 0; Row < Sequences->RowCount(); ++Row)
			{
				const size_t Command = size_t(Sequences->GetUnsigned(Row, "CommandIndex"));
				if (Command < Usage.size())
				{
					++Usage[Command];
				}
			}
		}
		for (size_t Row = 0; Row < Commands->RowCount(); ++Row)
		{
			SequenceCommandInfo Info;
			Info.Index = Row;
			Info.UsedBy = Usage[Row];
			for (uint32_t Id : CategoryIdsOfCommand(Row))
			{
				const auto Found = ById.find(Id);
				Info.Categories.push_back(Found != ById.end() ? Found->second : "#" + std::to_string(Id));
			}
			for (const std::string& Name : Info.Categories)
			{
				Info.Label += (Info.Label.empty() ? "" : " + ") + Name;
			}
			if (Info.Label.empty())
			{
				Info.Label = "(no category)";
			}
			Result.push_back(std::move(Info));
		}
		return Result;
	}

	std::vector<std::string> AcbDocument::CategoryLabels() const
	{
		std::vector<std::string> Labels;
		for (const SequenceCommandInfo& Info : ListSequenceCommands())
		{
			if (!Info.Categories.empty() && std::find(Labels.begin(), Labels.end(), Info.Label) == Labels.end())
			{
				Labels.push_back(Info.Label);
			}
		}
		return Labels;
	}

	std::vector<std::string> AcbDocument::CueCategories(size_t CueIndex) const
	{
		const CueChain Chain = ResolveChain(CueIndex);
		if (!Chain.SequenceIndex)
		{
			return {};
		}
		const size_t CommandRow = size_t(Require(HeaderSequence).GetUnsigned(*Chain.SequenceIndex, "CommandIndex"));
		std::map<uint32_t, std::string> ById;
		for (const auto& Pair : ListCategories())
		{
			ById[Pair.second] = Pair.first;
		}
		std::vector<std::string> Names;
		for (uint32_t Id : CategoryIdsOfCommand(CommandRow))
		{
			const auto Found = ById.find(Id);
			Names.push_back(Found != ById.end() ? Found->second : "#" + std::to_string(Id));
		}
		return Names;
	}

	uint16_t AcbDocument::SequenceCommandIndexForCategory(const std::string& Category) const
	{
		const std::map<std::string, uint32_t> Known = ListCategories();
		std::vector<uint32_t> Wanted;
		size_t Start = 0;
		while (Start <= Category.size())
		{
			size_t End = Category.find('+', Start);
			if (End == std::string::npos)
			{
				End = Category.size();
			}
			std::string Part = Category.substr(Start, End - Start);
			const size_t First = Part.find_first_not_of(' ');
			const size_t Last = Part.find_last_not_of(' ');
			Part = First == std::string::npos ? std::string() : Part.substr(First, Last - First + 1);
			if (!Part.empty())
			{
				const auto Found = Known.find(Part);
				if (Found == Known.end())
				{
					std::string Names;
					for (const auto& Pair : Known)
					{
						Names += (Names.empty() ? "" : ", ") + Pair.first;
					}
					throw AcbError("Unknown category '" + Part + "' (known: " + (Names.empty() ? std::string("none") : Names) + ")");
				}
				Wanted.push_back(Found->second);
			}
			Start = End + 1;
		}
		std::sort(Wanted.begin(), Wanted.end());
		const UtfTable& Commands = Require(HeaderSequenceCommand);
		for (size_t Row = 0; Row < Commands.RowCount(); ++Row)
		{
			std::vector<uint32_t> Ids = CategoryIdsOfCommand(Row);
			std::sort(Ids.begin(), Ids.end());
			if (Ids == Wanted)
			{
				return CheckU16(Row, "SequenceCommand");
			}
		}
		throw AcbError("No SequenceCommand row has exactly the categories '" + Category + "'");
	}

	size_t AcbDocument::WaveformCount() const
	{
		return Require(HeaderWaveform).RowCount();
	}

	WaveformInfo AcbDocument::Waveform(size_t WaveformIndex) const
	{
		const UtfTable& Waveforms = Require(HeaderWaveform);
		WaveformInfo Info;
		Info.Streaming = uint8_t(Waveforms.GetUnsigned(WaveformIndex, "Streaming"));
		Info.Encode = uint8_t(Waveforms.GetUnsigned(WaveformIndex, "EncodeType"));
		Info.AwbId = uint32_t(Waveforms.GetUnsigned(WaveformIndex, Info.Streaming != 0 ? "StreamAwbId" : "MemoryAwbId"));
		Info.NumChannels = uint32_t(Waveforms.GetUnsigned(WaveformIndex, "NumChannels"));
		Info.SamplingRate = uint32_t(Waveforms.GetUnsigned(WaveformIndex, "SamplingRate"));
		Info.NumSamples = uint32_t(Waveforms.GetUnsigned(WaveformIndex, "NumSamples"));
		Info.bLoop = Waveforms.GetUnsigned(WaveformIndex, "LoopFlag") == 2;
		return Info;
	}

	bool AcbDocument::ResolveStreaming(const TrackOptions& Options) const
	{
		return Options.Streaming ? *Options.Streaming : StreamAwbArchive != nullptr;
	}

	Afs2Archive& AcbDocument::TargetAwb(bool bStreaming)
	{
		if (bStreaming)
		{
			if (!StreamAwbArchive)
			{
				throw AcbError("No streaming AWB loaded -- open the .awb together with the .acb, or add the audio to the embedded AWB");
			}
			return *StreamAwbArchive;
		}
		if (Afs2Archive* Existing = MemoryAwb())
		{
			return *Existing;
		}
		if (!HeaderTable->HasColumn(HeaderAwbFile))
		{
			throw AcbError("This ACB has no AwbFile column, so it cannot hold embedded audio");
		}
		UtfData& Data = HeaderTable->GetData(0, HeaderAwbFile);
		if (!Data.IsEmpty())
		{
			throw AcbError("The ACB's AwbFile cell holds data that is not an AWB");
		}
		Data.Awb = std::make_shared<Afs2Archive>();
		return *Data.Awb;
	}

	void AcbDocument::RequireTargetAwb(bool bStreaming) const
	{
		if (bStreaming && !StreamAwbArchive)
		{
			throw AcbError("No streaming AWB loaded -- open the .awb together with the .acb, or add the audio to the embedded AWB");
		}
		if (!bStreaming && !MemoryAwb() && !HeaderTable->HasColumn(HeaderAwbFile))
		{
			throw AcbError("This ACB has no AwbFile column, so it cannot hold embedded audio");
		}
	}

	uint32_t AcbDocument::AddAudioEntry(const std::string& AudioPath, bool bStreaming, std::optional<uint32_t> EntryId)
	{
		Afs2Archive& Archive = TargetAwb(bStreaming);
		const uint32_t Id = Archive.AddFile(AudioPath, EntryId);
		(bStreaming ? bStreamAwbModified : bMemoryAwbModified) = true;
		HeaderTable->MarkDirty();
		return Id;
	}

	AcbDocument::PreparedWaveform AcbDocument::PrepareWaveform(const std::string& AudioPath, const TrackOptions& Options) const
	{
		PreparedWaveform Out;
		Out.Encode = EncodeType::HCA;
		if (Options.Encode)
		{
			Out.Encode = *Options.Encode;
		}
		else if (!AudioPath.empty())
		{
			const std::optional<EncodeType> Detected = DetectEncodeType(AudioPath);
			if (!Detected)
			{
				throw AcbError(AudioPath + ": unsupported audio format (use .hca or .adx)");
			}
			Out.Encode = *Detected;
		}

		std::optional<uint32_t> NumChannels = Options.NumChannels;
		std::optional<uint32_t> SamplingRate = Options.SamplingRate;
		std::optional<uint32_t> NumSamples = Options.NumSamples;
		std::optional<uint32_t> LoopStart = Options.LoopStart;
		std::optional<uint32_t> LoopEnd = Options.LoopEnd;

		AudioInfo Probed;
		bool bProbed = false;
		if (!AudioPath.empty() && (!NumChannels || !SamplingRate || !NumSamples || (Options.bLoop && Out.Encode == EncodeType::HCA)))
		{
			Probed = ProbeAudioFile(AudioPath, Out.Encode);
			bProbed = true;
			if (!NumChannels) NumChannels = Probed.NumChannels;
			if (!SamplingRate) SamplingRate = Probed.SamplingRate;
			if (!NumSamples) NumSamples = Probed.NumSamples;
		}
		if (!NumChannels || !SamplingRate || !NumSamples)
		{
			throw AcbError("Need channels/sample rate/sample count, or an audio file to read them from");
		}
		Out.NumChannels = *NumChannels;
		Out.SamplingRate = *SamplingRate;
		Out.NumSamples = *NumSamples;

		Out.bLoop = Options.bLoop;
		if (Options.bLoop)
		{
			if (bProbed && Out.Encode == EncodeType::HCA)
			{
				if (!LoopStart && Probed.LoopStart) LoopStart = Probed.LoopStart;
				if (!LoopEnd && Probed.LoopEnd) LoopEnd = Probed.LoopEnd;
			}
			Out.LoopStart = LoopStart ? *LoopStart : 0;
			Out.LoopEnd = LoopEnd ? *LoopEnd : Out.NumSamples;
			const UtfTable* Extensions = FindTable(HeaderWaveformExtension);
			if (!Extensions)
			{
				throw AcbError("This ACB has no WaveformExtensionData table -- can't add loop points; turn looping off");
			}
			CheckU16(Extensions->RowCount(), "WaveformExtensionData");
		}
		CheckU16(WaveformCount(), "Waveform");
		return Out;
	}

	size_t AcbDocument::CommitWaveform(uint32_t AwbId, bool bStreaming, const PreparedWaveform& Prepared)
	{
		uint64_t ExtensionData = 0xFFFF;
		if (Prepared.bLoop)
		{
			UtfTable& Extensions = Require(HeaderWaveformExtension);
			ExtensionData = Extensions.RowCount();
			Extensions.AddRow({ { "LoopStart", CellU(Prepared.LoopStart) }, { "LoopEnd", CellU(Prepared.LoopEnd) } });
		}
		UtfTable& Waveforms = Require(HeaderWaveform);
		return Waveforms.AddRow({
			{ "MemoryAwbId", CellU(bStreaming ? 0xFFFF : AwbId) },
			{ "StreamAwbId", CellU(bStreaming ? AwbId : 0xFFFF) },
			{ "Streaming", CellU(bStreaming ? 1 : 0) },
			{ "EncodeType", CellU(uint8_t(Prepared.Encode)) },
			{ "LoopFlag", CellU(Prepared.bLoop ? 2 : 1) },
			{ "NumChannels", CellU(Prepared.NumChannels) },
			{ "SamplingRate", CellU(Prepared.SamplingRate) },
			{ "NumSamples", CellU(Prepared.NumSamples) },
			{ "StreamAwbPortNo", CellU(0) },
			{ "LipMorthIndex", CellU(0xFFFF) },
			{ "ExtensionData", CellU(ExtensionData) },
		});
	}

	size_t AcbDocument::AddWaveform(uint32_t AwbId, bool bStreaming, const std::string& AudioPath, const TrackOptions& Options)
	{
		return CommitWaveform(AwbId, bStreaming, PrepareWaveform(AudioPath, Options));
	}

	std::vector<size_t> AcbDocument::AddAudioFiles(const std::vector<std::string>& AudioPaths, const TrackOptions& Options)
	{
		if (AudioPaths.empty())
		{
			throw AcbError("No audio files given");
		}
		std::vector<PreparedWaveform> Prepared;
		for (const std::string& Path : AudioPaths)
		{
			Prepared.push_back(PrepareWaveform(Path, Options));
		}
		const bool bStreaming = ResolveStreaming(Options);
		RequireTargetAwb(bStreaming);
		std::vector<size_t> Waveforms;
		std::map<std::string, size_t> Added;
		for (size_t Index = 0; Index < AudioPaths.size(); ++Index)
		{
			const auto Existing = Added.find(AudioPaths[Index]);
			if (Existing != Added.end())
			{
				Waveforms.push_back(Existing->second);
				continue;
			}
			const std::optional<uint32_t> Id = Index == 0 ? Options.AwbEntryId : std::nullopt;
			Waveforms.push_back(CommitWaveform(AddAudioEntry(AudioPaths[Index], bStreaming, Id), bStreaming, Prepared[Index]));
			Added[AudioPaths[Index]] = Waveforms.back();
		}
		return Waveforms;
	}

	size_t AcbDocument::AddSynth(size_t WaveformIndex, uint16_t CommandIndex)
	{
		UtfTable& Synths = Require(HeaderSynth);
		const size_t Index = Synths.AddRow({
			{ "CommandIndex", CellU(CommandIndex) },
			{ "ReferenceItems", CellB(PackReferenceItems({ { ReferenceItemWaveform, CheckU16(WaveformIndex, "Waveform") } })) },
			{ "Type", CellU(0) },
			{ "VoiceLimitGroupName", CellS("") },
			{ "GlobalAisacStartIndex", CellU(0xFFFF) },
			{ "GlobalAisacNumRefs", CellU(0) },
			{ "ControlWorkArea1", CellU(0xFFFF) },
			{ "ControlWorkArea2", CellU(0xFFFF) },
			{ "ParameterPallet", CellU(0xFFFF) },
			{ "ActionTrackStartIndex", CellU(0xFFFF) },
			{ "NumActionTracks", CellU(0) },
		});
		return CheckU16(Index, "Synth");
	}

	size_t AcbDocument::AddTrackEvent(size_t SynthIndex)
	{
		AcbCommand NoteOn;
		NoteOn.Op = AcbOp::ReferenceItem;
		PutBE16(NoteOn.Params, TrackEventRefSynth);
		PutBE16(NoteOn.Params, CheckU16(SynthIndex, "Synth"));
		AcbCommand Terminator;
		Terminator.Op = AcbOp::Null;
		UtfTable& Events = Require(HeaderTrackEvent);
		const size_t Index = Events.AddRow({ { "Command", CellB(EncodeCommands({ NoteOn, Terminator })) } });
		return CheckU16(Index, "TrackEvent");
	}

	size_t AcbDocument::AddTrackRow(size_t EventIndex, uint16_t CommandIndex)
	{
		UtfTable& Tracks = Require(HeaderTrack);
		const size_t Index = Tracks.AddRow({
			{ "EventIndex", CellU(EventIndex) },
			{ "CommandIndex", CellU(CommandIndex) },
			{ "GlobalAisacStartIndex", CellU(0xFFFF) },
			{ "GlobalAisacNumRefs", CellU(0) },
			{ "ParameterPallet", CellU(0xFFFF) },
			{ "TargetType", CellU(0) },
			{ "TargetName", CellS("") },
			{ "TargetId", CellU(0xFFFFFFFFull) },
			{ "TargetAcbName", CellS("") },
			{ "Scope", CellU(0) },
			{ "TargetTrackNo", CellU(0xFFFF) },
		});
		return CheckU16(Index, "Track");
	}

	size_t AcbDocument::SequenceWorkArea2For(size_t NewIndex) const
	{
		const UtfTable& Sequences = Require(HeaderSequence);
		if (Sequences.RowCount() == 0 || !Sequences.HasColumn("ControlWorkArea2"))
		{
			return NewIndex;
		}
		const uint64_t First = Sequences.GetUnsigned(0, "ControlWorkArea2");
		return First == 0 ? NewIndex : size_t(First);
	}

	size_t AcbDocument::AddSequence(const std::vector<size_t>& TrackIndices, uint16_t CommandIndex, uint8_t Type)
	{
		UtfTable& Sequences = Require(HeaderSequence);
		const size_t NewIndex = Sequences.RowCount();
		std::vector<uint16_t> Tracks;
		for (size_t Track : TrackIndices)
		{
			Tracks.push_back(CheckU16(Track, "Track"));
		}
		const size_t Index = Sequences.AddRow({
			{ "NumTracks", CellU(Tracks.size()) },
			{ "TrackIndex", CellB(PackU16Array(Tracks)) },
			{ "CommandIndex", CellU(CommandIndex) },
			{ "PlaybackRatio", CellU(100) },
			{ "GlobalAisacStartIndex", CellU(0xFFFF) },
			{ "GlobalAisacNumRefs", CellU(0) },
			{ "ParameterPallet", CellU(0xFFFF) },
			{ "ActionTrackStartIndex", CellU(0xFFFF) },
			{ "NumActionTracks", CellU(0) },
			{ "TrackValues", CellB(IsRandomType(Type) ? PackU16Array(std::vector<uint16_t>(Tracks.size(), DefaultTrackWeight)) : std::vector<uint8_t>()) },
			{ "Type", CellU(Type) },
			{ "ControlWorkArea1", CellU(0xFFFF) },
			{ "ControlWorkArea2", CellU(SequenceWorkArea2For(NewIndex)) },
			{ "InstPluginTrackStartIndex", CellU(0xFFFF) },
			{ "NumInstPluginTracks", CellU(0) },
		});
		return CheckU16(Index, "Sequence");
	}

	size_t AcbDocument::AddTrackChain(size_t WaveformIndex, uint16_t SynthCommand, uint16_t TrackCommand)
	{
		const size_t Synth = AddSynth(WaveformIndex, SynthCommand);
		const size_t Event = AddTrackEvent(Synth);
		return AddTrackRow(Event, TrackCommand);
	}

	uint16_t AcbDocument::MostCommonValue(const std::string& HeaderColumn, const std::string& Column, uint16_t Fallback) const
	{
		const UtfTable* Table = FindTable(HeaderColumn);
		if (!Table || !Table->HasColumn(Column) || Table->RowCount() == 0)
		{
			return Fallback;
		}
		std::map<uint64_t, size_t> Counts;
		for (size_t Row = 0; Row < Table->RowCount(); ++Row)
		{
			++Counts[Table->GetUnsigned(Row, Column)];
		}
		uint64_t Best = Fallback;
		size_t BestCount = 0;
		for (const auto& Pair : Counts)
		{
			if (Pair.second > BestCount)
			{
				Best = Pair.first;
				BestCount = Pair.second;
			}
		}
		return uint16_t(Best);
	}

	AcbDocument::ChainSettings AcbDocument::ResolveChainSettings(const std::string& Category, std::optional<size_t> TemplateCue,
		const TrackOptions& Options) const
	{
		ChainSettings Settings;
		Settings.SequenceCommand = 0;
		Settings.TrackCommand = MostCommonValue(HeaderTrack, "CommandIndex", 0);
		Settings.SynthCommand = MostCommonValue(HeaderSynth, "CommandIndex", 0);
		if (TemplateCue)
		{
			if (*TemplateCue >= CueCount())
			{
				throw AcbError("Template cue index out of range");
			}
			const CueChain Chain = ResolveChain(*TemplateCue);
			if (Chain.SequenceIndex)
			{
				Settings.SequenceCommand = uint16_t(Require(HeaderSequence).GetUnsigned(*Chain.SequenceIndex, "CommandIndex"));
				Settings.SingleTrackType = Chain.SequenceType ? *Chain.SequenceType : 0;
			}
			if (Chain.TrackIndex)
			{
				Settings.TrackCommand = uint16_t(Require(HeaderTrack).GetUnsigned(*Chain.TrackIndex, "CommandIndex"));
			}
			if (Chain.SynthIndex)
			{
				Settings.SynthCommand = uint16_t(Require(HeaderSynth).GetUnsigned(*Chain.SynthIndex, "CommandIndex"));
			}
		}
		if (Options.SequenceCommandIndex) Settings.SequenceCommand = *Options.SequenceCommandIndex;
		if (Options.TrackCommandIndex) Settings.TrackCommand = *Options.TrackCommandIndex;
		if (Options.SynthCommandIndex) Settings.SynthCommand = *Options.SynthCommandIndex;
		if (!Category.empty())
		{
			Settings.SequenceCommand = SequenceCommandIndexForCategory(Category);
		}
		return Settings;
	}

	uint32_t AcbDocument::ComputeLengthMs(size_t WaveformIndex) const
	{
		const UtfTable& Waveforms = Require(HeaderWaveform);
		const uint64_t NumSamples = Waveforms.GetUnsigned(WaveformIndex, "NumSamples");
		const uint64_t SamplingRate = Waveforms.GetUnsigned(WaveformIndex, "SamplingRate");
		return SamplingRate == 0 ? 0 : uint32_t(NumSamples * 1000 / SamplingRate);
	}

	void AcbDocument::RecomputeLength(size_t CueIndex)
	{
		UtfTable& Cues = Require(HeaderCue);
		if (Cues.GetUnsigned(CueIndex, "Length") == 0xFFFFFFFFull)
		{
			return;
		}
		uint32_t Longest = 0;
		bool bAny = false;
		for (size_t Waveform : ResolveChain(CueIndex).WaveformIndices)
		{
			if (Waveform < WaveformCount())
			{
				Longest = std::max(Longest, ComputeLengthMs(Waveform));
				bAny = true;
			}
		}
		if (bAny)
		{
			Cues.SetUnsigned(CueIndex, "Length", Longest);
		}
	}

	uint32_t AcbDocument::NextCueId() const
	{
		const UtfTable& Cues = Require(HeaderCue);
		uint64_t Next = 0;
		for (size_t Row = 0; Row < Cues.RowCount(); ++Row)
		{
			Next = std::max<uint64_t>(Next, Cues.GetUnsigned(Row, "CueId") + 1);
		}
		return uint32_t(Next);
	}

	void AcbDocument::RequireUniqueName(const std::string& Name) const
	{
		if (Name.empty())
		{
			throw AcbError("Cue name is empty");
		}
		if (FindCueByName(Name))
		{
			throw AcbError("A cue named '" + Name + "' already exists");
		}
	}

	void AcbDocument::InsertCueName(const std::string& Name, size_t CueIndex)
	{
		UtfTable& Names = Require(HeaderCueName);
		std::vector<UtfValue> Row = Names.MakeDefaultRow();
		Row[size_t(Names.ColumnIndex("CueName"))].Text = Name;
		Row[size_t(Names.ColumnIndex("CueIndex"))].SetUnsigned(CheckU16(CueIndex, "Cue"));
		size_t Position = Names.RowCount();
		for (size_t Existing = 0; Existing < Names.RowCount(); ++Existing)
		{
			if (Names.GetString(Existing, "CueName") > Name)
			{
				Position = Existing;
				break;
			}
		}
		Names.InsertRow(Position, std::move(Row));
	}

	uint32_t AcbDocument::AppendCueRow(const std::string& Name, CueReferenceType Type, size_t ReferenceIndex, uint32_t Length,
		size_t NumWaveforms, std::optional<size_t> TemplateCue)
	{
		const uint32_t Id = NextCueId();
		UtfTable& Cues = Require(HeaderCue);
		if (TemplateCue)
		{
			Cues.InsertRow(Cues.RowCount(), Cues.Rows[*TemplateCue]);
			const size_t Row = Cues.RowCount() - 1;
			Cues.SetUnsigned(Row, "CueId", Id);
			Cues.SetUnsigned(Row, "ReferenceType", uint8_t(Type));
			Cues.SetUnsigned(Row, "ReferenceIndex", CheckU16(ReferenceIndex, "Cue reference"));
			Cues.SetUnsigned(Row, "Length", Length);
			Cues.SetUnsigned(Row, "NumRelatedWaveforms", NumWaveforms);
		}
		else
		{
			Cues.AddRow({
				{ "CueId", CellU(Id) },
				{ "ReferenceType", CellU(uint8_t(Type)) },
				{ "ReferenceIndex", CellU(CheckU16(ReferenceIndex, "Cue reference")) },
				{ "NumRelatedWaveforms", CellU(NumWaveforms) },
				{ "Length", CellU(Length) },
				{ "UserData", CellS("") },
				{ "Worksize", CellU(0) },
				{ "NumAisacControlMaps", CellU(0) },
				{ "HeaderVisibility", CellU(1) },
			});
		}
		InsertCueName(Name, Cues.RowCount() - 1);
		return Id;
	}

	uint32_t AcbDocument::AddCue(const std::string& Name, size_t WaveformIndex, uint16_t TrackCommandIndex,
		uint16_t SequenceCommandIndex, const std::string& Category)
	{
		RequireUniqueName(Name);
		if (WaveformIndex >= WaveformCount())
		{
			throw AcbError("Waveform index " + std::to_string(WaveformIndex) + " out of range");
		}
		if (!Category.empty())
		{
			SequenceCommandIndex = SequenceCommandIndexForCategory(Category);
		}
		const size_t Track = AddTrackChain(WaveformIndex, 0, TrackCommandIndex);
		const size_t Sequence = AddSequence({ Track }, SequenceCommandIndex, 0);
		return AppendCueRow(Name, CueReferenceType::Sequence, Sequence, ComputeLengthMs(WaveformIndex), 1, std::nullopt);
	}

	std::vector<uint32_t> AcbDocument::AddTrackCues(const std::vector<std::string>& AudioPaths, const std::vector<CueSpec>& Cues,
		const TrackOptions& Options)
	{
		if (Cues.empty())
		{
			throw AcbError("No cues to create");
		}
		std::set<std::string> Seen;
		std::vector<ChainSettings> Settings;
		for (const CueSpec& Cue : Cues)
		{
			RequireUniqueName(Cue.Name);
			if (!Seen.insert(Cue.Name).second)
			{
				throw AcbError("Cue name '" + Cue.Name + "' is listed twice");
			}
			Settings.push_back(ResolveChainSettings(Cue.Category.empty() ? Options.Category : Cue.Category, Cue.TemplateCue, Options));
		}
		const std::vector<size_t> Waveforms = AddAudioFiles(AudioPaths, Options);
		uint32_t Longest = 0;
		for (size_t Waveform : Waveforms)
		{
			Longest = std::max(Longest, ComputeLengthMs(Waveform));
		}
		std::vector<uint32_t> Ids;
		for (size_t Index = 0; Index < Cues.size(); ++Index)
		{
			const ChainSettings& S = Settings[Index];
			std::vector<size_t> Tracks;
			for (size_t Waveform : Waveforms)
			{
				Tracks.push_back(AddTrackChain(Waveform, S.SynthCommand, S.TrackCommand));
			}
			const uint8_t Type = Waveforms.size() > 1 ? uint8_t(Options.Variations) : S.SingleTrackType;
			const size_t Sequence = AddSequence(Tracks, S.SequenceCommand, Type);
			uint32_t Length = Longest;
			if (Cues[Index].TemplateCue && CueLength(*Cues[Index].TemplateCue) == 0xFFFFFFFFu)
			{
				Length = 0xFFFFFFFFu;
			}
			Ids.push_back(AppendCueRow(Cues[Index].Name, CueReferenceType::Sequence, Sequence, Length, Waveforms.size(), Cues[Index].TemplateCue));
		}
		return Ids;
	}

	uint32_t AcbDocument::AddTrack(const std::string& Name, const std::string& AudioPath, const TrackOptions& Options)
	{
		CueSpec Spec;
		Spec.Name = Name;
		Spec.Category = Options.Category;
		return AddTrackCues({ AudioPath }, { Spec }, Options)[0];
	}

	uint32_t AcbDocument::AddCueSynthDirect(const std::string& Name, size_t WaveformIndex)
	{
		RequireUniqueName(Name);
		if (WaveformIndex >= WaveformCount())
		{
			throw AcbError("Waveform index " + std::to_string(WaveformIndex) + " out of range");
		}
		const size_t Synth = AddSynth(WaveformIndex, 0);
		return AppendCueRow(Name, CueReferenceType::Synth, Synth, ComputeLengthMs(WaveformIndex), 1, std::nullopt);
	}

	AcbDocument::RefCounts AcbDocument::CountReferences() const
	{
		RefCounts Counts;
		const UtfTable* Sequences = FindTable(HeaderSequence);
		const UtfTable* Tracks = FindTable(HeaderTrack);
		const UtfTable* Events = FindTable(HeaderTrackEvent);
		const UtfTable* Synths = FindTable(HeaderSynth);
		Counts.Sequences.assign(Sequences ? Sequences->RowCount() : 0, 0);
		Counts.Tracks.assign(Tracks ? Tracks->RowCount() : 0, 0);
		Counts.Events.assign(Events ? Events->RowCount() : 0, 0);
		Counts.Synths.assign(Synths ? Synths->RowCount() : 0, 0);
		auto Bump = [](std::vector<size_t>& Vector, uint64_t Index)
		{
			if (Index < Vector.size())
			{
				++Vector[size_t(Index)];
			}
		};
		const UtfTable& Cues = Require(HeaderCue);
		for (size_t Row = 0; Row < Cues.RowCount(); ++Row)
		{
			const uint64_t Type = Cues.GetUnsigned(Row, "ReferenceType");
			const uint64_t Index = Cues.GetUnsigned(Row, "ReferenceIndex");
			if (Type == uint8_t(CueReferenceType::Sequence)) Bump(Counts.Sequences, Index);
			if (Type == uint8_t(CueReferenceType::Synth)) Bump(Counts.Synths, Index);
		}
		for (size_t Row = 0; Sequences && Row < Sequences->RowCount(); ++Row)
		{
			for (uint16_t Track : SequenceTracks(Row))
			{
				Bump(Counts.Tracks, Track);
			}
		}
		for (size_t Row = 0; Tracks && Row < Tracks->RowCount(); ++Row)
		{
			Bump(Counts.Events, Tracks->GetUnsigned(Row, "EventIndex"));
		}
		for (size_t Row = 0; Events && Row < Events->RowCount(); ++Row)
		{
			if (const std::optional<size_t> Synth = SynthOfEvent(Row))
			{
				Bump(Counts.Synths, *Synth);
			}
		}
		return Counts;
	}

	std::vector<AcbDocument::Slot> AcbDocument::SlotsOf(size_t CueIndex) const
	{
		std::vector<Slot> Slots;
		auto AddSynthSlots = [&](size_t Synth, std::optional<size_t> TrackPosition)
		{
			const std::vector<ReferenceItem> Items = UnpackReferenceItems(Require(HeaderSynth).GetBytes(Synth, "ReferenceItems"));
			for (size_t Item = 0; Item < Items.size(); ++Item)
			{
				if (Items[Item].first == ReferenceItemWaveform)
				{
					Slots.push_back({ TrackPosition, Item });
				}
			}
		};
		const CueChain Chain = ResolveChain(CueIndex);
		if (Chain.SequenceIndex)
		{
			for (size_t Position = 0; Position < Chain.Tracks.size(); ++Position)
			{
				if (Chain.Tracks[Position].SynthIndex)
				{
					AddSynthSlots(*Chain.Tracks[Position].SynthIndex, Position);
				}
			}
		}
		else if (CueRefType(CueIndex) == CueReferenceType::Synth && Chain.SynthIndex)
		{
			AddSynthSlots(*Chain.SynthIndex, std::nullopt);
		}
		return Slots;
	}

	void AcbDocument::SetSequenceTracks(size_t SequenceIndex, const std::vector<uint16_t>& Tracks, std::optional<uint8_t> NewType)
	{
		UtfTable& Sequences = Require(HeaderSequence);
		const uint8_t Type = NewType ? *NewType : uint8_t(Sequences.GetUnsigned(SequenceIndex, "Type"));
		Sequences.SetBytes(SequenceIndex, "TrackIndex", PackU16Array(Tracks));
		if (Sequences.HasColumn("NumTracks"))
		{
			Sequences.SetUnsigned(SequenceIndex, "NumTracks", Tracks.size());
		}
		Sequences.SetUnsigned(SequenceIndex, "Type", Type);
		if (Sequences.HasColumn("TrackValues"))
		{
			std::vector<uint16_t> Weights = UnpackU16Array(Sequences.GetBytes(SequenceIndex, "TrackValues"));
			if (IsRandomType(Type))
			{
				Weights.resize(Tracks.size(), DefaultTrackWeight);
			}
			else
			{
				Weights.clear();
			}
			Sequences.SetBytes(SequenceIndex, "TrackValues", PackU16Array(Weights));
		}
	}

	size_t AcbDocument::MakeSequencePrivate(size_t CueIndex)
	{
		const size_t Sequence = CueRefIndex(CueIndex);
		if (CountReferences().Sequences.at(Sequence) <= 1)
		{
			return Sequence;
		}
		UtfTable& Sequences = Require(HeaderSequence);
		Sequences.InsertRow(Sequences.RowCount(), Sequences.Rows[Sequence]);
		const size_t NewSequence = CheckU16(Sequences.RowCount() - 1, "Sequence");
		if (Sequences.HasColumn("ControlWorkArea2"))
		{
			Sequences.SetUnsigned(NewSequence, "ControlWorkArea2", SequenceWorkArea2For(NewSequence));
		}
		Require(HeaderCue).SetUnsigned(CueIndex, "ReferenceIndex", NewSequence);
		return NewSequence;
	}

	size_t AcbDocument::MakeSlotSynthPrivate(size_t CueIndex, const Slot& Where)
	{
		UtfTable& Synths = Require(HeaderSynth);
		if (!Where.TrackPosition)
		{
			const size_t Synth = CueRefIndex(CueIndex);
			if (CountReferences().Synths.at(Synth) <= 1)
			{
				return Synth;
			}
			Synths.InsertRow(Synths.RowCount(), Synths.Rows[Synth]);
			const size_t NewSynth = CheckU16(Synths.RowCount() - 1, "Synth");
			Require(HeaderCue).SetUnsigned(CueIndex, "ReferenceIndex", NewSynth);
			return NewSynth;
		}
		const size_t Sequence = MakeSequencePrivate(CueIndex);
		std::vector<uint16_t> Tracks = SequenceTracks(Sequence);
		const size_t Track = Tracks.at(*Where.TrackPosition);
		UtfTable& TrackTable = Require(HeaderTrack);
		const size_t Event = size_t(TrackTable.GetUnsigned(Track, "EventIndex"));
		const std::optional<size_t> Synth = SynthOfEvent(Event);
		if (!Synth)
		{
			throw AcbError("Track has no synth");
		}
		const RefCounts Counts = CountReferences();
		if (Counts.Tracks.at(Track) <= 1 && Counts.Events.at(Event) <= 1 && Counts.Synths.at(*Synth) <= 1)
		{
			return *Synth;
		}
		Synths.InsertRow(Synths.RowCount(), Synths.Rows[*Synth]);
		const size_t NewSynth = CheckU16(Synths.RowCount() - 1, "Synth");
		UtfTable& Events = Require(HeaderTrackEvent);
		std::vector<AcbCommand> Commands = DecodeCommands(Events.GetBytes(Event, "Command"));
		for (AcbCommand& Command : Commands)
		{
			if (Command.Op == AcbOp::ReferenceItem && Command.Params.size() == 4 && ReadBE16(Command.Params.data()) == TrackEventRefSynth)
			{
				Command.Params[2] = uint8_t(NewSynth >> 8);
				Command.Params[3] = uint8_t(NewSynth);
			}
		}
		Events.InsertRow(Events.RowCount(), Events.Rows[Event]);
		const size_t NewEvent = CheckU16(Events.RowCount() - 1, "TrackEvent");
		Events.SetBytes(NewEvent, "Command", EncodeCommands(Commands));
		TrackTable.InsertRow(TrackTable.RowCount(), TrackTable.Rows[Track]);
		const size_t NewTrack = CheckU16(TrackTable.RowCount() - 1, "Track");
		TrackTable.SetUnsigned(NewTrack, "EventIndex", NewEvent);
		Tracks[*Where.TrackPosition] = uint16_t(NewTrack);
		SetSequenceTracks(Sequence, Tracks, std::nullopt);
		return NewSynth;
	}

	std::vector<size_t> AcbDocument::CuesUsingSynth(size_t SynthIndex) const
	{
		std::vector<size_t> Users;
		for (size_t Cue = 0; Cue < CueCount(); ++Cue)
		{
			for (const CueTrack& Track : ResolveChain(Cue).Tracks)
			{
				if (Track.SynthIndex && *Track.SynthIndex == SynthIndex)
				{
					Users.push_back(Cue);
					break;
				}
			}
			if (CueRefType(Cue) == CueReferenceType::Synth && CueRefIndex(Cue) == SynthIndex)
			{
				Users.push_back(Cue);
			}
		}
		return Users;
	}

	void AcbDocument::SetSlotWaveform(size_t CueIndex, size_t SlotIndex, size_t WaveformIndex)
	{
		const std::vector<Slot> Slots = SlotsOf(CueIndex);
		const Slot Where = Slots.at(SlotIndex);
		const size_t Synth = MakeSlotSynthPrivate(CueIndex, Where);
		UtfTable& Synths = Require(HeaderSynth);
		std::vector<ReferenceItem> Items = UnpackReferenceItems(Synths.GetBytes(Synth, "ReferenceItems"));
		Items.at(Where.ItemPosition).second = CheckU16(WaveformIndex, "Waveform");
		Synths.SetBytes(Synth, "ReferenceItems", PackReferenceItems(Items));
		RecomputeLength(CueIndex);
	}

	size_t AcbDocument::ReplaceWaveformOnCue(size_t CueIndex, const std::string& AudioPath, size_t SlotIndex, const TrackOptions& Options)
	{
		const size_t Count = SlotsOf(CueIndex).size();
		if (SlotIndex >= Count)
		{
			throw AcbError("Waveform " + std::to_string(SlotIndex) + " out of range -- cue '" + CueName(CueIndex) + "' has "
				+ std::to_string(Count));
		}
		const size_t Waveform = AddAudioFiles({ AudioPath }, Options)[0];
		SetSlotWaveform(CueIndex, SlotIndex, Waveform);
		return Waveform;
	}

	void AcbDocument::SetCueWaveform(size_t CueIndex, size_t WaveformIndex, size_t SlotIndex)
	{
		if (WaveformIndex >= WaveformCount())
		{
			throw AcbError("Waveform index " + std::to_string(WaveformIndex) + " out of range");
		}
		const size_t Count = SlotsOf(CueIndex).size();
		if (SlotIndex >= Count)
		{
			throw AcbError("Waveform " + std::to_string(SlotIndex) + " out of range -- cue '" + CueName(CueIndex) + "' has "
				+ std::to_string(Count));
		}
		SetSlotWaveform(CueIndex, SlotIndex, WaveformIndex);
	}

	size_t AcbDocument::AddWaveformToCue(size_t CueIndex, const std::string& AudioPath, const TrackOptions& Options,
		std::optional<SequenceType> Type)
	{
		const CueChain Chain = ResolveChain(CueIndex);
		UtfTable& Cues = Require(HeaderCue);
		if (Chain.SequenceIndex)
		{
			uint16_t SynthCommand = MostCommonValue(HeaderSynth, "CommandIndex", 0);
			uint16_t TrackCommand = MostCommonValue(HeaderTrack, "CommandIndex", 0);
			if (Chain.SynthIndex) SynthCommand = uint16_t(Require(HeaderSynth).GetUnsigned(*Chain.SynthIndex, "CommandIndex"));
			if (Chain.TrackIndex) TrackCommand = uint16_t(Require(HeaderTrack).GetUnsigned(*Chain.TrackIndex, "CommandIndex"));
			const size_t Waveform = AddAudioFiles({ AudioPath }, Options)[0];
			const size_t Sequence = MakeSequencePrivate(CueIndex);
			std::vector<uint16_t> Tracks = SequenceTracks(Sequence);
			Tracks.push_back(CheckU16(AddTrackChain(Waveform, SynthCommand, TrackCommand), "Track"));
			SetSequenceTracks(Sequence, Tracks, Type ? std::optional<uint8_t>(uint8_t(*Type)) : std::nullopt);
			Cues.SetUnsigned(CueIndex, "NumRelatedWaveforms", Cues.GetUnsigned(CueIndex, "NumRelatedWaveforms") + 1);
			RecomputeLength(CueIndex);
			return Waveform;
		}
		if (CueRefType(CueIndex) == CueReferenceType::Synth)
		{
			const size_t Waveform = AddAudioFiles({ AudioPath }, Options)[0];
			const size_t Synth = MakeSlotSynthPrivate(CueIndex, Slot{ std::nullopt, 0 });
			UtfTable& Synths = Require(HeaderSynth);
			std::vector<ReferenceItem> Items = UnpackReferenceItems(Synths.GetBytes(Synth, "ReferenceItems"));
			Items.emplace_back(ReferenceItemWaveform, CheckU16(Waveform, "Waveform"));
			Synths.SetBytes(Synth, "ReferenceItems", PackReferenceItems(Items));
			Cues.SetUnsigned(CueIndex, "NumRelatedWaveforms", Cues.GetUnsigned(CueIndex, "NumRelatedWaveforms") + 1);
			RecomputeLength(CueIndex);
			return Waveform;
		}
		throw AcbError("Cue '" + CueName(CueIndex) + "' points straight at a waveform; it can't take tracks");
	}

	void AcbDocument::RemoveWaveformFromCue(size_t CueIndex, size_t SlotIndex)
	{
		const std::vector<Slot> Slots = SlotsOf(CueIndex);
		if (SlotIndex >= Slots.size())
		{
			throw AcbError("Waveform " + std::to_string(SlotIndex) + " out of range -- cue '" + CueName(CueIndex) + "' has "
				+ std::to_string(Slots.size()));
		}
		if (Slots.size() <= 1)
		{
			throw AcbError("That is the cue's only waveform -- delete the cue instead");
		}
		const Slot Where = Slots[SlotIndex];
		size_t ItemsInSameSynth = 0;
		for (const Slot& Other : Slots)
		{
			ItemsInSameSynth += Other.TrackPosition == Where.TrackPosition ? 1 : 0;
		}
		if (ItemsInSameSynth > 1 || !Where.TrackPosition)
		{
			const size_t Synth = MakeSlotSynthPrivate(CueIndex, Where);
			UtfTable& Synths = Require(HeaderSynth);
			std::vector<ReferenceItem> Items = UnpackReferenceItems(Synths.GetBytes(Synth, "ReferenceItems"));
			Items.erase(Items.begin() + std::ptrdiff_t(Where.ItemPosition));
			Synths.SetBytes(Synth, "ReferenceItems", PackReferenceItems(Items));
		}
		else
		{
			const size_t Sequence = MakeSequencePrivate(CueIndex);
			UtfTable& Sequences = Require(HeaderSequence);
			std::vector<uint16_t> Tracks = SequenceTracks(Sequence);
			std::vector<uint16_t> Weights = Sequences.HasColumn("TrackValues")
				? UnpackU16Array(Sequences.GetBytes(Sequence, "TrackValues")) : std::vector<uint16_t>();
			Tracks.erase(Tracks.begin() + std::ptrdiff_t(*Where.TrackPosition));
			if (*Where.TrackPosition < Weights.size())
			{
				Weights.erase(Weights.begin() + std::ptrdiff_t(*Where.TrackPosition));
				Sequences.SetBytes(Sequence, "TrackValues", PackU16Array(Weights));
			}
			SetSequenceTracks(Sequence, Tracks, std::nullopt);
		}
		UtfTable& Cues = Require(HeaderCue);
		const uint64_t Related = Cues.GetUnsigned(CueIndex, "NumRelatedWaveforms");
		Cues.SetUnsigned(CueIndex, "NumRelatedWaveforms", Related > 0 ? Related - 1 : 0);
		RecomputeLength(CueIndex);
	}

	void AcbDocument::UseCueAudio(size_t TargetCue, size_t SourceCue)
	{
		if (TargetCue == SourceCue)
		{
			throw AcbError("A cue can't take its own audio");
		}
		const CueChain Source = ResolveChain(SourceCue);
		const CueChain Target = ResolveChain(TargetCue);
		if (Source.WaveformIndices.empty())
		{
			throw AcbError("Cue '" + CueName(SourceCue) + "' has no audio");
		}
		UtfTable& Cues = Require(HeaderCue);
		if (Source.SequenceIndex && Target.SequenceIndex)
		{
			UtfTable& Sequences = Require(HeaderSequence);
			const std::vector<uint16_t> Tracks = SequenceTracks(*Source.SequenceIndex);
			const std::vector<uint8_t> Weights = Sequences.HasColumn("TrackValues")
				? Sequences.GetBytes(*Source.SequenceIndex, "TrackValues") : std::vector<uint8_t>();
			const uint8_t Type = Source.SequenceType ? *Source.SequenceType : 0;
			const size_t Sequence = MakeSequencePrivate(TargetCue);
			Sequences.SetBytes(Sequence, "TrackIndex", PackU16Array(Tracks));
			if (Sequences.HasColumn("NumTracks"))
			{
				Sequences.SetUnsigned(Sequence, "NumTracks", Tracks.size());
			}
			Sequences.SetUnsigned(Sequence, "Type", Type);
			if (Sequences.HasColumn("TrackValues"))
			{
				Sequences.SetBytes(Sequence, "TrackValues", Weights);
			}
			Cues.SetUnsigned(TargetCue, "NumRelatedWaveforms", Cues.GetUnsigned(SourceCue, "NumRelatedWaveforms"));
			Cues.SetUnsigned(TargetCue, "Length", Cues.GetUnsigned(SourceCue, "Length"));
			return;
		}
		if (SlotsOf(TargetCue).empty())
		{
			throw AcbError("Cue '" + CueName(TargetCue) + "' has no waveform slot to change");
		}
		SetSlotWaveform(TargetCue, 0, Source.WaveformIndices[0]);
	}

	void AcbDocument::SetCueSequenceType(size_t CueIndex, SequenceType Type)
	{
		if (CueRefType(CueIndex) != CueReferenceType::Sequence)
		{
			throw AcbError("Only sequence cues have a play mode");
		}
		const size_t Sequence = MakeSequencePrivate(CueIndex);
		SetSequenceTracks(Sequence, SequenceTracks(Sequence), uint8_t(Type));
	}

	uint32_t AcbDocument::AddCueForCueAudio(size_t SourceCue, const std::string& Name, const std::string& Category)
	{
		RequireUniqueName(Name);
		const std::optional<uint16_t> CategoryCommand = Category.empty()
			? std::nullopt : std::optional<uint16_t>(SequenceCommandIndexForCategory(Category));
		const CueChain Chain = ResolveChain(SourceCue);
		if (Chain.SequenceIndex)
		{
			UtfTable& Sequences = Require(HeaderSequence);
			Sequences.InsertRow(Sequences.RowCount(), Sequences.Rows[*Chain.SequenceIndex]);
			const size_t Sequence = CheckU16(Sequences.RowCount() - 1, "Sequence");
			if (Sequences.HasColumn("ControlWorkArea2"))
			{
				Sequences.SetUnsigned(Sequence, "ControlWorkArea2", SequenceWorkArea2For(Sequence));
			}
			if (CategoryCommand)
			{
				Sequences.SetUnsigned(Sequence, "CommandIndex", *CategoryCommand);
			}
			return AppendCueRow(Name, CueReferenceType::Sequence, Sequence, CueLength(SourceCue),
				size_t(Require(HeaderCue).GetUnsigned(SourceCue, "NumRelatedWaveforms")), SourceCue);
		}
		if (!Chain.WaveformIndex)
		{
			throw AcbError("Cue '" + CueName(SourceCue) + "' has no audio");
		}
		const uint32_t NewId = AddCue(Name, *Chain.WaveformIndex, 0, CategoryCommand ? *CategoryCommand : 0);
		if (CueLength(SourceCue) == 0xFFFFFFFFu)
		{
			UtfTable& Cues = Require(HeaderCue);
			Cues.SetUnsigned(Cues.RowCount() - 1, "Length", 0xFFFFFFFFu);
		}
		return NewId;
	}

	void AcbDocument::SetCueCategory(size_t CueIndex, const std::string& Category)
	{
		if (CueRefType(CueIndex) != CueReferenceType::Sequence)
		{
			throw AcbError("Cue '" + CueName(CueIndex) + "' plays a synth directly, so it has no category");
		}
		const std::map<std::string, uint32_t> Known = ListCategories();
		std::vector<uint32_t> Ids;
		size_t Start = 0;
		while (Start <= Category.size())
		{
			size_t End = Category.find('+', Start);
			if (End == std::string::npos)
			{
				End = Category.size();
			}
			std::string Part = Category.substr(Start, End - Start);
			const size_t First = Part.find_first_not_of(' ');
			const size_t Last = Part.find_last_not_of(' ');
			Part = First == std::string::npos ? std::string() : Part.substr(First, Last - First + 1);
			if (!Part.empty())
			{
				const auto Found = Known.find(Part);
				if (Found == Known.end())
				{
					throw AcbError("Unknown category '" + Part + "'");
				}
				Ids.push_back(Found->second);
			}
			Start = End + 1;
		}

		UtfTable& Sequences = Require(HeaderSequence);
		UtfTable& Commands = Require(HeaderSequenceCommand);
		const size_t Sequence = CueRefIndex(CueIndex);
		const uint64_t Current = Sequences.GetUnsigned(Sequence, "CommandIndex");
		std::vector<AcbCommand> List;
		if (Current < Commands.RowCount())
		{
			List = DecodeCommands(Commands.GetBytes(size_t(Current), "Command"));
		}
		List.erase(std::remove_if(List.begin(), List.end(), [](const AcbCommand& Command) { return Command.Op == AcbOp::Category; }), List.end());
		if (!Ids.empty())
		{
			AcbCommand Command;
			Command.Op = AcbOp::Category;
			for (uint32_t Id : Ids)
			{
				PutBE32(Command.Params, Id);
			}
			List.insert(List.begin(), Command);
		}
		const std::vector<uint8_t> Wanted = EncodeCommands(List);
		if (Current < Commands.RowCount() && Commands.GetBytes(size_t(Current), "Command") == Wanted)
		{
			return;
		}
		size_t Row = Commands.RowCount();
		for (size_t Candidate = 0; Candidate < Commands.RowCount(); ++Candidate)
		{
			if (Commands.GetBytes(Candidate, "Command") == Wanted)
			{
				Row = Candidate;
				break;
			}
		}
		if (Row == Commands.RowCount())
		{
			CheckU16(Row, "SequenceCommand");
			Row = Commands.AddRow({ { "Command", CellB(Wanted) } });
		}
		const size_t Private = MakeSequencePrivate(CueIndex);
		Require(HeaderSequence).SetUnsigned(Private, "CommandIndex", Row);
	}

	std::optional<uint16_t> AcbDocument::CueVolume(size_t CueIndex) const
	{
		if (CueRefType(CueIndex) != CueReferenceType::Sequence)
		{
			return std::nullopt;
		}
		return VolumeOf(FindTable(HeaderSequenceCommand), Require(HeaderSequence).GetUnsigned(CueRefIndex(CueIndex), "CommandIndex"));
	}

	std::optional<uint16_t> AcbDocument::SlotVolume(size_t CueIndex, size_t SlotIndex) const
	{
		const std::vector<Slot> Slots = SlotsOf(CueIndex);
		if (SlotIndex >= Slots.size() || !Slots[SlotIndex].TrackPosition)
		{
			return std::nullopt;
		}
		const size_t Track = SequenceTracks(CueRefIndex(CueIndex)).at(*Slots[SlotIndex].TrackPosition);
		return VolumeOf(FindTable(HeaderTrackCommand), Require(HeaderTrack).GetUnsigned(Track, "CommandIndex"));
	}

	void AcbDocument::SetCueVolume(size_t CueIndex, std::optional<uint16_t> Volume)
	{
		if (CueRefType(CueIndex) != CueReferenceType::Sequence)
		{
			throw AcbError("Cue '" + CueName(CueIndex) + "' plays a synth directly, so it has no cue volume");
		}
		UtfTable& Commands = Require(HeaderSequenceCommand);
		const uint64_t Current = Require(HeaderSequence).GetUnsigned(CueRefIndex(CueIndex), "CommandIndex");
		const std::vector<uint8_t> Old = Current < Commands.RowCount() ? Commands.GetBytes(size_t(Current), "Command") : std::vector<uint8_t>();
		const std::vector<uint8_t> Wanted = WithVolume(Old, Volume);
		if (Current < Commands.RowCount() && Old == Wanted)
		{
			return;
		}
		if (Current >= Commands.RowCount() && !Volume)
		{
			return;
		}
		const size_t Row = FindOrAddCommandRow(Commands, Wanted, "SequenceCommand");
		const size_t Private = MakeSequencePrivate(CueIndex);
		Require(HeaderSequence).SetUnsigned(Private, "CommandIndex", Row);
	}

	void AcbDocument::SetSlotVolume(size_t CueIndex, size_t SlotIndex, std::optional<uint16_t> Volume)
	{
		const std::vector<Slot> Slots = SlotsOf(CueIndex);
		if (SlotIndex >= Slots.size())
		{
			throw AcbError("No track " + std::to_string(SlotIndex));
		}
		if (!Slots[SlotIndex].TrackPosition)
		{
			throw AcbError("Cue '" + CueName(CueIndex) + "' plays a synth directly, so its tracks have no volume");
		}
		UtfTable* Commands = FindTable(HeaderTrackCommand);
		if (!Commands)
		{
			throw AcbError("This ACB has no TrackCommand table");
		}
		const size_t Position = *Slots[SlotIndex].TrackPosition;
		const uint64_t Current = Require(HeaderTrack).GetUnsigned(SequenceTracks(CueRefIndex(CueIndex)).at(Position), "CommandIndex");
		const std::vector<uint8_t> Old = Current < Commands->RowCount() ? Commands->GetBytes(size_t(Current), "Command") : std::vector<uint8_t>();
		const std::vector<uint8_t> Wanted = WithVolume(Old, Volume);
		if (Current < Commands->RowCount() && Old == Wanted)
		{
			return;
		}
		if (Current >= Commands->RowCount() && !Volume)
		{
			return;
		}
		const size_t Row = FindOrAddCommandRow(*Commands, Wanted, "TrackCommand");
		MakeSlotSynthPrivate(CueIndex, Slots[SlotIndex]);
		const size_t Track = SequenceTracks(CueRefIndex(CueIndex)).at(Position);
		Require(HeaderTrack).SetUnsigned(Track, "CommandIndex", Row);
	}

	uint32_t AcbDocument::CopyCue(size_t CueIndex, const std::string& NewName)
	{
		return AddCueForCueAudio(CueIndex, NewName.empty() ? CueName(CueIndex) + "_copy" : NewName, std::string());
	}

	void AcbDocument::RenameCue(size_t CueIndex, const std::string& NewName)
	{
		if (CueName(CueIndex) == NewName)
		{
			return;
		}
		RequireUniqueName(NewName);
		UtfTable& Names = Require(HeaderCueName);
		for (size_t Row = Names.RowCount(); Row-- > 0;)
		{
			if (Names.GetUnsigned(Row, "CueIndex") == CueIndex)
			{
				Names.RemoveRow(Row);
			}
		}
		InsertCueName(NewName, CueIndex);
	}

	AcbDocument::LiveRows AcbDocument::MarkLive(const std::vector<char>& CueAlive) const
	{
		LiveRows Live;
		const UtfTable& Cues = Require(HeaderCue);
		const UtfTable* Sequences = FindTable(HeaderSequence);
		const UtfTable* Tracks = FindTable(HeaderTrack);
		const UtfTable* Events = FindTable(HeaderTrackEvent);
		const UtfTable* Synths = FindTable(HeaderSynth);
		const UtfTable* Waveforms = FindTable(HeaderWaveform);
		const UtfTable* ActionTracks = FindTable("ActionTrackTable");
		Live.Sequences.assign(Sequences ? Sequences->RowCount() : 0, 0);
		Live.Tracks.assign(Tracks ? Tracks->RowCount() : 0, 0);
		Live.Events.assign(Events ? Events->RowCount() : 0, 0);
		Live.Synths.assign(Synths ? Synths->RowCount() : 0, 0);
		Live.Waveforms.assign(Waveforms ? Waveforms->RowCount() : 0, 0);

		struct FWork
		{
			uint16_t Type;
			uint64_t Index;
			size_t Root;
		};
		std::vector<FWork> Work;
		size_t Root = SIZE_MAX;
		auto Push = [&Work, &Root](uint16_t Type, uint64_t Index) { Work.push_back({ Type, Index, Root }); };
		const uint16_t KindTrack = 100;
		const uint16_t KindEvent = 101;
		const uint16_t KindCue = 7;

		for (size_t Row = 0; Row < Cues.RowCount(); ++Row)
		{
			if (Row < CueAlive.size() && !CueAlive[Row])
			{
				continue;
			}
			Root = Row;
			Push(uint16_t(Cues.GetUnsigned(Row, "ReferenceType")), Cues.GetUnsigned(Row, "ReferenceIndex"));
		}
		Root = SIZE_MAX;
		for (size_t Row = 0; ActionTracks && Row < ActionTracks->RowCount(); ++Row)
		{
			Push(KindEvent, ActionTracks->GetUnsigned(Row, "EventIndex"));
		}

		auto Mark = [](std::vector<char>& Flags, uint64_t Index)
		{
			if (Index >= Flags.size() || Flags[size_t(Index)])
			{
				return false;
			}
			Flags[size_t(Index)] = 1;
			return true;
		};
		while (!Work.empty())
		{
			const FWork Item = Work.back();
			Work.pop_back();
			Root = Item.Root;
			const size_t Index = size_t(Item.Index);
			switch (Item.Type)
			{
			case 0:
				break;
			case uint16_t(CueReferenceType::Waveform):
				Mark(Live.Waveforms, Index);
				break;
			case uint16_t(CueReferenceType::Synth):
				if (Mark(Live.Synths, Index))
				{
					for (const ReferenceItem& Ref : UnpackReferenceItems(Synths->GetBytes(Index, "ReferenceItems")))
					{
						Push(Ref.first, Ref.second);
					}
				}
				break;
			case uint16_t(CueReferenceType::Sequence):
				if (Mark(Live.Sequences, Index))
				{
					for (uint16_t Track : SequenceTracks(Index))
					{
						Push(KindTrack, Track);
					}
				}
				break;
			case KindCue:
				if (Index < Cues.RowCount() && Index < CueAlive.size() && !CueAlive[Index])
				{
					Live.Blocked = CueName(Index) + " is played by " + (Root < Cues.RowCount() ? CueName(Root) : std::string("another cue"));
				}
				break;
			case KindTrack:
				if (Mark(Live.Tracks, Index))
				{
					Push(KindEvent, Tracks->GetUnsigned(Index, "EventIndex"));
				}
				break;
			case KindEvent:
				if (Mark(Live.Events, Index))
				{
					for (const AcbCommand& Command : DecodeCommands(Events->GetBytes(Index, "Command")))
					{
						if (Command.Op == AcbOp::ReferenceItem && Command.Params.size() >= 4)
						{
							const uint16_t Type = ReadBE16(Command.Params.data());
							if (Type != uint16_t(CueReferenceType::Synth) && Type != uint16_t(CueReferenceType::Sequence))
							{
								Live.Unknown = "track event " + std::to_string(Index) + " refers to item type " + std::to_string(Type);
							}
							Push(Type, ReadBE16(Command.Params.data() + 2));
						}
					}
				}
				break;
			default:
				Live.Unknown = "reference type " + std::to_string(Item.Type) + " isn't understood";
				break;
			}
		}
		return Live;
	}

	void AcbDocument::DeleteCue(size_t CueIndex)
	{
		DeleteCues({ CueIndex });
	}

	void AcbDocument::DeleteCues(std::vector<size_t> CueIndices)
	{
		UtfTable& Cues = Require(HeaderCue);
		UtfTable& Names = Require(HeaderCueName);
		std::sort(CueIndices.begin(), CueIndices.end());
		CueIndices.erase(std::unique(CueIndices.begin(), CueIndices.end()), CueIndices.end());
		std::vector<char> Alive(Cues.RowCount(), 1);
		for (size_t Index : CueIndices)
		{
			if (Index >= Cues.RowCount())
			{
				throw AcbError("Cue index out of range");
			}
			Alive[Index] = 0;
		}
		const LiveRows Live = MarkLive(Alive);
		if (!Live.Blocked.empty())
		{
			throw AcbError("Can't delete: " + Live.Blocked + ". Delete that cue too, or change it first.");
		}

		std::vector<int64_t> NewIndex(Cues.RowCount(), -1);
		int64_t Next = 0;
		for (size_t Row = 0; Row < Cues.RowCount(); ++Row)
		{
			if (Alive[Row])
			{
				NewIndex[Row] = Next++;
			}
		}
		for (size_t Row = Names.RowCount(); Row-- > 0;)
		{
			const uint64_t Index = Names.GetUnsigned(Row, "CueIndex");
			if (Index < NewIndex.size() && NewIndex[size_t(Index)] < 0)
			{
				Names.RemoveRow(Row);
			}
			else if (Index < NewIndex.size())
			{
				Names.SetUnsigned(Row, "CueIndex", uint64_t(NewIndex[size_t(Index)]));
			}
		}
		if (UtfTable* Synths = FindTable(HeaderSynth))
		{
			for (size_t Row = 0; Row < Synths->RowCount(); ++Row)
			{
				const std::vector<ReferenceItem> Items = UnpackReferenceItems(Synths->GetBytes(Row, "ReferenceItems"));
				std::vector<ReferenceItem> Kept;
				bool bChanged = false;
				for (const ReferenceItem& Item : Items)
				{
					if (Item.first == 7 && Item.second < NewIndex.size())
					{
						bChanged = true;
						if (NewIndex[Item.second] >= 0)
						{
							Kept.emplace_back(Item.first, uint16_t(NewIndex[Item.second]));
						}
						continue;
					}
					Kept.push_back(Item);
				}
				if (bChanged)
				{
					Synths->SetBytes(Row, "ReferenceItems", PackReferenceItems(Kept));
				}
			}
		}
		for (auto It = CueIndices.rbegin(); It != CueIndices.rend(); ++It)
		{
			Cues.RemoveRow(*It);
		}
	}

	CleanupResult AcbDocument::RemoveUnusedData()
	{
		CleanupResult Result;
		UtfTable& Cues = Require(HeaderCue);
		UtfTable* Sequences = FindTable(HeaderSequence);
		UtfTable* Tracks = FindTable(HeaderTrack);
		UtfTable* Events = FindTable(HeaderTrackEvent);
		UtfTable* Synths = FindTable(HeaderSynth);
		UtfTable* Waveforms = FindTable(HeaderWaveform);
		UtfTable* Extensions = FindTable(HeaderWaveformExtension);
		UtfTable* ActionTracks = FindTable("ActionTrackTable");
		if (!Synths || !Waveforms)
		{
			Result.Skipped = "no Synth or Waveform table";
			return Result;
		}
		const LiveRows Live = MarkLive(std::vector<char>(Cues.RowCount(), 1));
		if (!Live.Unknown.empty())
		{
			Result.Skipped = Live.Unknown;
			return Result;
		}

		auto MakeMap = [](const std::vector<char>& Flags, size_t& Removed)
		{
			std::vector<int64_t> Map(Flags.size(), -1);
			int64_t Next = 0;
			for (size_t Index = 0; Index < Flags.size(); ++Index)
			{
				if (Flags[Index])
				{
					Map[Index] = Next++;
				}
				else
				{
					++Removed;
				}
			}
			return Map;
		};
		auto Remap = [](const std::vector<int64_t>& Map, uint64_t Index) -> uint64_t
		{
			return Index < Map.size() && Map[size_t(Index)] >= 0 ? uint64_t(Map[size_t(Index)]) : Index;
		};
		const std::vector<int64_t> SequenceMap = MakeMap(Live.Sequences, Result.Sequences);
		const std::vector<int64_t> TrackMap = MakeMap(Live.Tracks, Result.Tracks);
		const std::vector<int64_t> EventMap = MakeMap(Live.Events, Result.Events);
		const std::vector<int64_t> SynthMap = MakeMap(Live.Synths, Result.Synths);
		const std::vector<int64_t> WaveformMap = MakeMap(Live.Waveforms, Result.Waveforms);

		std::vector<char> ExtensionLive(Extensions ? Extensions->RowCount() : 0, 0);
		std::set<uint64_t> MemoryIds;
		std::set<uint64_t> StreamIds;
		for (size_t Row = 0; Row < Waveforms->RowCount(); ++Row)
		{
			if (!Live.Waveforms[Row])
			{
				continue;
			}
			const uint64_t Streaming = Waveforms->GetUnsigned(Row, "Streaming");
			if (Streaming != 1)
			{
				MemoryIds.insert(Waveforms->GetUnsigned(Row, "MemoryAwbId"));
			}
			if (Streaming != 0 && Waveforms->HasColumn("StreamAwbId"))
			{
				StreamIds.insert(Waveforms->GetUnsigned(Row, "StreamAwbId"));
			}
			if (Extensions && Waveforms->HasColumn("ExtensionData"))
			{
				const uint64_t Extension = Waveforms->GetUnsigned(Row, "ExtensionData");
				if (Extension < ExtensionLive.size())
				{
					ExtensionLive[size_t(Extension)] = 1;
				}
			}
		}
		const std::vector<int64_t> ExtensionMap = MakeMap(ExtensionLive, Result.Extensions);

		for (size_t Row = 0; Row < Cues.RowCount(); ++Row)
		{
			const uint64_t Type = Cues.GetUnsigned(Row, "ReferenceType");
			const uint64_t Index = Cues.GetUnsigned(Row, "ReferenceIndex");
			if (Type == uint8_t(CueReferenceType::Sequence)) Cues.SetUnsigned(Row, "ReferenceIndex", Remap(SequenceMap, Index));
			if (Type == uint8_t(CueReferenceType::Synth)) Cues.SetUnsigned(Row, "ReferenceIndex", Remap(SynthMap, Index));
			if (Type == uint8_t(CueReferenceType::Waveform)) Cues.SetUnsigned(Row, "ReferenceIndex", Remap(WaveformMap, Index));
		}
		auto RemapItems = [&](const std::vector<ReferenceItem>& Items)
		{
			std::vector<ReferenceItem> Out;
			for (const ReferenceItem& Item : Items)
			{
				uint64_t Index = Item.second;
				if (Item.first == uint16_t(CueReferenceType::Waveform)) Index = Remap(WaveformMap, Index);
				if (Item.first == uint16_t(CueReferenceType::Synth)) Index = Remap(SynthMap, Index);
				if (Item.first == uint16_t(CueReferenceType::Sequence)) Index = Remap(SequenceMap, Index);
				Out.emplace_back(Item.first, uint16_t(Index));
			}
			return Out;
		};
		for (size_t Row = 0; Row < Synths->RowCount(); ++Row)
		{
			const std::vector<uint8_t> Old = Synths->GetBytes(Row, "ReferenceItems");
			const std::vector<uint8_t> New = PackReferenceItems(RemapItems(UnpackReferenceItems(Old)));
			if (New != Old)
			{
				Synths->SetBytes(Row, "ReferenceItems", New);
			}
		}
		for (size_t Row = 0; Events && Row < Events->RowCount(); ++Row)
		{
			std::vector<AcbCommand> Commands = DecodeCommands(Events->GetBytes(Row, "Command"));
			bool bChanged = false;
			for (AcbCommand& Command : Commands)
			{
				if (Command.Op != AcbOp::ReferenceItem || Command.Params.size() < 4)
				{
					continue;
				}
				const uint16_t Type = ReadBE16(Command.Params.data());
				const uint64_t Old = ReadBE16(Command.Params.data() + 2);
				const uint64_t New = Type == uint16_t(CueReferenceType::Synth) ? Remap(SynthMap, Old) : Remap(SequenceMap, Old);
				if (New != Old)
				{
					Command.Params[2] = uint8_t(New >> 8);
					Command.Params[3] = uint8_t(New);
					bChanged = true;
				}
			}
			if (bChanged)
			{
				Events->SetBytes(Row, "Command", EncodeCommands(Commands));
			}
		}
		for (size_t Row = 0; Sequences && Row < Sequences->RowCount(); ++Row)
		{
			std::vector<uint16_t> TrackList = SequenceTracks(Row);
			bool bChanged = false;
			for (uint16_t& Track : TrackList)
			{
				const uint16_t New = uint16_t(Remap(TrackMap, Track));
				bChanged |= New != Track;
				Track = New;
			}
			if (bChanged)
			{
				Sequences->SetBytes(Row, "TrackIndex", PackU16Array(TrackList));
			}
			if (Sequences->HasColumn("ControlWorkArea2") && Sequences->GetUnsigned(Row, "ControlWorkArea2") == Row)
			{
				Sequences->SetUnsigned(Row, "ControlWorkArea2", Remap(SequenceMap, Row));
			}
			if (Sequences->HasColumn("ControlWorkArea1") && Sequences->GetUnsigned(Row, "ControlWorkArea1") == Row)
			{
				Sequences->SetUnsigned(Row, "ControlWorkArea1", Remap(SequenceMap, Row));
			}
		}
		for (size_t Row = 0; Tracks && Row < Tracks->RowCount(); ++Row)
		{
			const uint64_t Event = Tracks->GetUnsigned(Row, "EventIndex");
			if (Remap(EventMap, Event) != Event)
			{
				Tracks->SetUnsigned(Row, "EventIndex", Remap(EventMap, Event));
			}
		}
		for (size_t Row = 0; ActionTracks && Row < ActionTracks->RowCount(); ++Row)
		{
			const uint64_t Event = ActionTracks->GetUnsigned(Row, "EventIndex");
			if (Remap(EventMap, Event) != Event)
			{
				ActionTracks->SetUnsigned(Row, "EventIndex", Remap(EventMap, Event));
			}
		}
		for (size_t Row = 0; Extensions && Row < Waveforms->RowCount() && Waveforms->HasColumn("ExtensionData"); ++Row)
		{
			const uint64_t Extension = Waveforms->GetUnsigned(Row, "ExtensionData");
			if (Remap(ExtensionMap, Extension) != Extension)
			{
				Waveforms->SetUnsigned(Row, "ExtensionData", Remap(ExtensionMap, Extension));
			}
		}

		auto RemoveRows = [](UtfTable* Table, const std::vector<int64_t>& Map)
		{
			for (size_t Row = Map.size(); Table && Row-- > 0;)
			{
				if (Map[Row] < 0)
				{
					Table->RemoveRow(Row);
				}
			}
		};
		RemoveRows(Sequences, SequenceMap);
		RemoveRows(Tracks, TrackMap);
		RemoveRows(Events, EventMap);
		RemoveRows(Synths, SynthMap);
		RemoveRows(Waveforms, WaveformMap);
		RemoveRows(Extensions, ExtensionMap);

		auto Prune = [&Result](Afs2Archive* Archive, const std::set<uint64_t>& Keep, size_t& Count)
		{
			if (!Archive)
			{
				return false;
			}
			std::vector<uint32_t> Drop;
			for (const Afs2Entry& Entry : Archive->Entries)
			{
				if (!Keep.count(Entry.Id))
				{
					Drop.push_back(Entry.Id);
					Result.AudioBytes += Entry.Size;
				}
			}
			for (uint32_t Id : Drop)
			{
				Archive->RemoveEntry(Id);
			}
			Count += Drop.size();
			return !Drop.empty();
		};
		if (Prune(MemoryAwb(), MemoryIds, Result.MemoryAudio))
		{
			bMemoryAwbModified = true;
		}
		if (Prune(StreamAwbArchive.get(), StreamIds, Result.StreamAudio))
		{
			bStreamAwbModified = true;
		}
		return Result;
	}

	void AcbDocument::MergeAwbIntoAcb(bool bConvertStreamingWaveforms)
	{
		if (!StreamAwbArchive)
		{
			throw AcbError("No streaming AWB loaded");
		}
		if (!HeaderTable->HasColumn(HeaderAwbFile))
		{
			throw AcbError("This ACB has no AwbFile column");
		}
		Afs2Archive* Existing = MemoryAwb();
		if (Existing && !Existing->Entries.empty())
		{
			throw AcbError("The ACB already has an embedded AWB -- already merged?");
		}
		UtfData& Cell = HeaderTable->GetData(0, HeaderAwbFile);
		if (!Existing && !Cell.IsEmpty())
		{
			throw AcbError("The ACB's AwbFile cell holds data that is not an AWB");
		}
		Cell = UtfData();
		Cell.Awb = StreamAwbArchive;

		if (bConvertStreamingWaveforms)
		{
			std::set<uint64_t> MergedIds;
			for (const Afs2Entry& Entry : StreamAwbArchive->Entries)
			{
				MergedIds.insert(Entry.Id);
			}
			UtfTable& Waveforms = Require(HeaderWaveform);
			for (size_t Row = 0; Row < Waveforms.RowCount(); ++Row)
			{
				if (Waveforms.GetUnsigned(Row, "Streaming") != 1)
				{
					continue;
				}
				const uint64_t StreamId = Waveforms.GetUnsigned(Row, "StreamAwbId");
				if (!MergedIds.count(StreamId))
				{
					continue;
				}
				Waveforms.SetUnsigned(Row, "MemoryAwbId", StreamId);
				Waveforms.SetUnsigned(Row, "StreamAwbId", 0xFFFF);
				Waveforms.SetUnsigned(Row, "Streaming", 0);
			}
		}
		StreamAwbArchive.reset();
		bStreamAwbModified = false;
		bMemoryAwbModified = true;
	}

	std::string AcbDocument::SanitizeCueName(const std::string& Name)
	{
		std::string Result = Name;
		for (char& C : Result)
		{
			const unsigned char U8 = static_cast<unsigned char>(C);
			if (!(std::isalnum(U8) && U8 < 0x80) && C != '_')
			{
				C = '_';
			}
		}
		return Result;
	}

	bool AcbDocument::IsModified() const
	{
		return HeaderTable->IsDirty() || bStreamAwbModified || bMemoryAwbModified;
	}

	uint64_t AcbDocument::EstimateAcbSize(bool bForcePerRow) const
	{
		UtfWriteOptions Options;
		Options.DataAlignment = 32;
		Options.EndAlignment = 32;
		Options.ForcePerRow = bForcePerRow;
		return HeaderTable->ComputeSize(Options);
	}

	uint64_t AcbDocument::EstimateAwbSize() const
	{
		return StreamAwbArchive ? StreamAwbArchive->ComputeSize() : 0;
	}

	void AcbDocument::UpdateStreamAwbInfo(const std::vector<uint8_t>& AwbHeader, const std::array<uint8_t, 16>& AwbMd5)
	{
		if (UtfTable* Copy = FindTable(HeaderStreamAwbHeader))
		{
			if (Copy->RowCount() == 1 && Copy->HasColumn("Header"))
			{
				std::vector<uint8_t> Bytes = AwbHeader;
				Bytes.push_back(0);
				Bytes.push_back(0);
				Copy->SetBytes(0, "Header", Bytes);
			}
		}
		if (HeaderTable->HasColumn(HeaderStreamAwbTocWork))
		{
			const UtfData& Work = HeaderTable->Get(0, HeaderStreamAwbTocWork).Data;
			const uint64_t OldSize = Work.IsEmpty() ? 0 : UtfDataWriteSize(Work);
			if (OldSize > 0 && OldSize == uint64_t(StreamAwbLoadedCount) * TocWorkBytesPerEntry && StreamAwbArchive)
			{
				HeaderTable->SetBytes(0, HeaderStreamAwbTocWork,
					std::vector<uint8_t>(StreamAwbArchive->Entries.size() * TocWorkBytesPerEntry, 0));
			}
		}
		if (UtfTable* Hash = FindTable(HeaderStreamAwbHash))
		{
			if (Hash->RowCount() >= 1 && Hash->HasColumn("Hash"))
			{
				Hash->SetBytes(0, "Hash", std::vector<uint8_t>(AwbMd5.begin(), AwbMd5.end()));
			}
		}
	}

	void AcbDocument::Save(const AcbSaveOptions& Options)
	{
		if (Options.AcbPath.empty())
		{
			throw AcbError("No output path for the ACB");
		}
		if (StreamAwbArchive && bStreamAwbModified && Options.AwbPath.empty())
		{
			throw AcbError("The streaming AWB changed -- choose where to save it, or merge it into the ACB");
		}

		std::set<std::string> Sources = { NormalizePath(AcbPathLoaded) };
		if (!AwbPathLoaded.empty())
		{
			Sources.insert(NormalizePath(AwbPathLoaded));
		}
		for (const Afs2Archive* Archive : { StreamAwbArchive.get(), MemoryAwb() })
		{
			if (Archive)
			{
				for (const Afs2Entry& Entry : Archive->Entries)
				{
					if (!Entry.SourcePath.empty())
					{
						Sources.insert(NormalizePath(Entry.SourcePath));
					}
				}
			}
		}
		const bool bWriteAwb = StreamAwbArchive && !Options.AwbPath.empty();
		if (Sources.count(NormalizePath(Options.AcbPath)) || (bWriteAwb && Sources.count(NormalizePath(Options.AwbPath))))
		{
			throw AcbError("Save to a different file than the ones being read (the source ACB/AWB or an added audio file)");
		}

		UtfWriteOptions WriteOptions;
		WriteOptions.DataAlignment = 32;
		WriteOptions.EndAlignment = 32;
		WriteOptions.ForcePerRow = Options.bForcePerRow;

		Progress Tracker;
		Tracker.Callback = Options.OnProgress;
		Tracker.User = Options.ProgressUser;
		Tracker.Total = (bWriteAwb ? StreamAwbArchive->ComputeSize() : 0) + EstimateAcbSize(Options.bForcePerRow);
		WriteOptions.ProgressSink = &Tracker;

		const std::string AwbTemp = Options.AwbPath + ".tmp";
		const std::string AcbTemp = Options.AcbPath + ".tmp";
		try
		{
			if (bWriteAwb)
			{
				std::array<uint8_t, 16> Digest;
				{
					std::unique_ptr<WriteStream> File = OpenFileWrite(AwbTemp);
					HashingWriteStream Hashing(*File);
					StreamAwbArchive->Write(Hashing, &Tracker);
					Digest = Hashing.Finish();
				}
				if (bStreamAwbModified && Options.bUpdateStreamAwbInfo)
				{
					UpdateStreamAwbInfo(StreamAwbArchive->BuildHeader(), Digest);
				}
			}
			{
				std::unique_ptr<WriteStream> File = OpenFileWrite(AcbTemp);
				HeaderTable->Write(*File, WriteOptions);
			}
		}
		catch (...)
		{
			DeleteFileIfExists(AwbTemp);
			DeleteFileIfExists(AcbTemp);
			throw;
		}
		if (bWriteAwb && !MoveFileReplace(AwbTemp, Options.AwbPath))
		{
			throw AcbError("Could not replace " + Options.AwbPath + " (is it open in another program?)");
		}
		if (!MoveFileReplace(AcbTemp, Options.AcbPath))
		{
			throw AcbError("Could not replace " + Options.AcbPath + " (is it open in another program?)");
		}
	}

	std::optional<size_t> AcbDocument::SlotEvent(size_t CueIndex, const Slot& Where) const
	{
		if (!Where.TrackPosition)
		{
			return std::nullopt;
		}
		const std::vector<uint16_t> Tracks = SequenceTracks(CueRefIndex(CueIndex));
		const size_t Track = Tracks.at(*Where.TrackPosition);
		return size_t(Require(HeaderTrack).GetUnsigned(Track, "EventIndex"));
	}

	size_t AcbDocument::SlotWaveform(size_t CueIndex, const Slot& Where) const
	{
		const std::vector<ReferenceItem> Items = UnpackReferenceItems(Require(HeaderSynth).GetBytes(SlotSynth(CueIndex, Where), "ReferenceItems"));
		return Items.at(Where.ItemPosition).second;
	}

	size_t AcbDocument::SlotSynth(size_t CueIndex, const Slot& Where) const
	{
		size_t Synth = 0;
		if (Where.TrackPosition)
		{
			const std::optional<size_t> Event = SlotEvent(CueIndex, Where);
			const std::optional<size_t> EventSynth = Event ? SynthOfEvent(*Event) : std::nullopt;
			if (!EventSynth)
			{
				throw AcbError("Track has no synth");
			}
			Synth = *EventSynth;
		}
		else
		{
			Synth = CueRefIndex(CueIndex);
		}
		return Synth;
	}

	bool AcbDocument::SynthIgnoresLoop(size_t SynthIndex) const
	{
		const UtfTable* Commands = FindTable(HeaderSynthCommand);
		const uint64_t Row = Require(HeaderSynth).GetUnsigned(SynthIndex, "CommandIndex");
		if (!Commands || Row >= Commands->RowCount())
		{
			return false;
		}
		const std::optional<uint64_t> Value = FindCommandValue(Commands->GetBytes(size_t(Row), "Command"), AcbOp::IgnoreLoop);
		return Value && *Value != 0;
	}

	void AcbDocument::SetSynthIgnoreLoop(size_t SynthIndex, bool bIgnore)
	{
		UtfTable& Synths = Require(HeaderSynth);
		UtfTable* Commands = FindTable(HeaderSynthCommand);
		const uint64_t Current = Synths.GetUnsigned(SynthIndex, "CommandIndex");
		std::vector<AcbCommand> List;
		if (Commands && Current < Commands->RowCount())
		{
			List = DecodeCommands(Commands->GetBytes(size_t(Current), "Command"));
		}
		List.erase(std::remove_if(List.begin(), List.end(), [](const AcbCommand& Command) { return Command.Op == AcbOp::IgnoreLoop; }), List.end());
		if (bIgnore)
		{
			AcbCommand Command;
			Command.Op = AcbOp::IgnoreLoop;
			Command.Params = { 1 };
			List.push_back(Command);
		}
		const std::vector<uint8_t> Wanted = EncodeCommands(List);
		if (List.empty() && (!Commands || Current >= Commands->RowCount()))
		{
			return;
		}
		if (!Commands)
		{
			throw AcbError("This ACB has no SynthCommand table, so it can't switch looping off per cue");
		}
		for (size_t Row = 0; Row < Commands->RowCount(); ++Row)
		{
			if (Commands->GetBytes(Row, "Command") == Wanted)
			{
				Synths.SetUnsigned(SynthIndex, "CommandIndex", Row);
				return;
			}
		}
		CheckU16(Commands->RowCount(), "SynthCommand");
		const size_t NewRow = Commands->AddRow({ { "Command", CellB(Wanted) } });
		Synths.SetUnsigned(SynthIndex, "CommandIndex", NewRow);
	}

	size_t AcbDocument::WaveformReferenceCount(size_t WaveformIndex) const
	{
		const UtfTable& Synths = Require(HeaderSynth);
		size_t Count = 0;
		for (size_t Row = 0; Row < Synths.RowCount(); ++Row)
		{
			for (const ReferenceItem& Item : UnpackReferenceItems(Synths.GetBytes(Row, "ReferenceItems")))
			{
				if (Item.first == ReferenceItemWaveform && Item.second == WaveformIndex)
				{
					++Count;
				}
			}
		}
		return Count;
	}

	SlotTiming AcbDocument::GetSlotTiming(size_t CueIndex, size_t SlotIndex) const
	{
		const std::vector<Slot> Slots = SlotsOf(CueIndex);
		if (SlotIndex >= Slots.size())
		{
			throw AcbError("Waveform " + std::to_string(SlotIndex) + " out of range -- cue '" + CueName(CueIndex) + "' has "
				+ std::to_string(Slots.size()));
		}
		const Slot& Where = Slots[SlotIndex];
		SlotTiming Timing;
		Timing.WaveformIndex = SlotWaveform(CueIndex, Where);
		const UtfTable& Waveforms = Require(HeaderWaveform);
		Timing.SamplingRate = uint32_t(Waveforms.GetUnsigned(Timing.WaveformIndex, "SamplingRate"));
		Timing.NumSamples = uint32_t(Waveforms.GetUnsigned(Timing.WaveformIndex, "NumSamples"));
		Timing.bWaveformLoops = Waveforms.GetUnsigned(Timing.WaveformIndex, "LoopFlag") == 2;
		const UtfTable* Extensions = FindTable(HeaderWaveformExtension);
		if (Extensions && Waveforms.HasColumn("ExtensionData"))
		{
			const uint64_t Extension = Waveforms.GetUnsigned(Timing.WaveformIndex, "ExtensionData");
			if (Extension < Extensions->RowCount())
			{
				Timing.LoopStart = uint32_t(Extensions->GetUnsigned(size_t(Extension), "LoopStart"));
				Timing.LoopEnd = uint32_t(Extensions->GetUnsigned(size_t(Extension), "LoopEnd"));
			}
		}
		if (!Timing.bWaveformLoops && Timing.LoopEnd == 0)
		{
			Timing.LoopEnd = Timing.NumSamples;
		}
		Timing.bIgnoreLoop = SynthIgnoresLoop(SlotSynth(CueIndex, Where));
		Timing.bLoop = Timing.bWaveformLoops && !Timing.bIgnoreLoop;
		if (const std::optional<size_t> Event = SlotEvent(CueIndex, Where))
		{
			Timing.bCanSetStartOffset = true;
			for (const AcbCommand& Command : DecodeCommands(Require(HeaderTrackEvent).GetBytes(*Event, "Command")))
			{
				if (Command.Op == AcbOp::StartOffset && Command.Params.size() >= 4)
				{
					Timing.StartOffsetMs = ReadBE32(Command.Params.data());
					break;
				}
			}
		}
		return Timing;
	}

	size_t AcbDocument::CountCuesUsingWaveform(size_t WaveformIndex) const
	{
		size_t Users = 0;
		for (size_t Cue = 0; Cue < CueCount(); ++Cue)
		{
			const std::vector<size_t> Used = ResolveChain(Cue).WaveformIndices;
			if (std::find(Used.begin(), Used.end(), WaveformIndex) != Used.end())
			{
				++Users;
			}
		}
		return Users;
	}

	void AcbDocument::SetSlotTiming(size_t CueIndex, size_t SlotIndex, std::optional<uint32_t> StartMs, bool bLoop, uint32_t LoopStart, uint32_t LoopEnd)
	{
		const SlotTiming Current = GetSlotTiming(CueIndex, SlotIndex);
		CheckSlotStartOffset(Current, CueIndex, StartMs);
		CheckSlotLoop(Current, bLoop, LoopStart, LoopEnd);
		SetSlotStartOffset(CueIndex, SlotIndex, StartMs);
		SetSlotLoop(CueIndex, SlotIndex, bLoop, LoopStart, LoopEnd);
	}

	void AcbDocument::CheckSlotStartOffset(const SlotTiming& Current, size_t CueIndex, std::optional<uint32_t> StartMs) const
	{
		if (StartMs && !Current.bCanSetStartOffset)
		{
			throw AcbError("Cue '" + CueName(CueIndex) + "' plays a synth directly, so it has no track event to hold a start point");
		}
		if (StartMs && Current.SamplingRate > 0 && uint64_t(*StartMs) * Current.SamplingRate / 1000 >= Current.NumSamples)
		{
			throw AcbError("Start point " + std::to_string(*StartMs) + " ms is past the end of the audio ("
				+ std::to_string(uint64_t(Current.NumSamples) * 1000 / Current.SamplingRate) + " ms)");
		}
	}

	void AcbDocument::SetSlotStartOffset(size_t CueIndex, size_t SlotIndex, std::optional<uint32_t> StartMs)
	{
		const SlotTiming Current = GetSlotTiming(CueIndex, SlotIndex);
		CheckSlotStartOffset(Current, CueIndex, StartMs);
		if (Current.StartOffsetMs == StartMs)
		{
			return;
		}
		const Slot Where = SlotsOf(CueIndex).at(SlotIndex);
		MakeSlotSynthPrivate(CueIndex, Where);
		const size_t Event = *SlotEvent(CueIndex, Where);
		UtfTable& Events = Require(HeaderTrackEvent);
		std::vector<AcbCommand> Commands = DecodeCommands(Events.GetBytes(Event, "Command"));
		auto Existing = std::find_if(Commands.begin(), Commands.end(), [](const AcbCommand& Command) { return Command.Op == AcbOp::StartOffset; });
		if (!StartMs)
		{
			if (Existing != Commands.end())
			{
				Commands.erase(Existing);
			}
		}
		else
		{
			std::vector<uint8_t> Params;
			PutBE32(Params, *StartMs);
			if (Existing != Commands.end() && Existing->Params.size() == 6)
			{
				Params.push_back(Existing->Params[4]);
				Params.push_back(Existing->Params[5]);
			}
			else
			{
				Params.push_back(0);
				Params.push_back(0);
			}
			if (Existing != Commands.end())
			{
				Existing->Params = Params;
			}
			else
			{
				AcbCommand Command;
				Command.Op = AcbOp::StartOffset;
				Command.Params = Params;
				Commands.insert(Commands.begin(), Command);
			}
		}
		Events.SetBytes(Event, "Command", EncodeCommands(Commands));
	}

	void AcbDocument::CheckSlotLoop(const SlotTiming& Current, bool bLoop, uint32_t LoopStart, uint32_t LoopEnd) const
	{
		const UtfTable& Waveforms = Require(HeaderWaveform);
		const UtfTable* Extensions = FindTable(HeaderWaveformExtension);
		const bool bRegionChanges = bLoop && !(Current.bWaveformLoops && Current.LoopStart == LoopStart && Current.LoopEnd == LoopEnd);
		if (bLoop)
		{
			if (LoopEnd <= LoopStart)
			{
				throw AcbError("Loop end must be after loop start");
			}
			if (LoopEnd > Current.NumSamples)
			{
				throw AcbError("Loop end " + std::to_string(LoopEnd) + " is past the end of the audio (" + std::to_string(Current.NumSamples)
					+ " samples)");
			}
			if (bRegionChanges)
			{
				if (!Extensions || !Waveforms.HasColumn("ExtensionData"))
				{
					throw AcbError("This ACB has no WaveformExtensionData table, so it can't store loop points");
				}
				CheckU16(Extensions->RowCount(), "WaveformExtensionData");
				CheckU16(Waveforms.RowCount(), "Waveform");
			}
		}
		const bool bSwitchOff = !bLoop && Current.bWaveformLoops && !Current.bIgnoreLoop;
		if (bSwitchOff && !FindTable(HeaderSynthCommand))
		{
			throw AcbError("This ACB has no SynthCommand table, so it can't switch looping off per cue");
		}
	}

	void AcbDocument::SetSlotLoop(size_t CueIndex, size_t SlotIndex, bool bLoop, uint32_t LoopStart, uint32_t LoopEnd)
	{
		const SlotTiming Current = GetSlotTiming(CueIndex, SlotIndex);
		CheckSlotLoop(Current, bLoop, LoopStart, LoopEnd);
		UtfTable& Waveforms = Require(HeaderWaveform);
		UtfTable* Extensions = FindTable(HeaderWaveformExtension);
		const bool bRegionChanges = bLoop && !(Current.bWaveformLoops && Current.LoopStart == LoopStart && Current.LoopEnd == LoopEnd);
		if (Current.bLoop == bLoop && !bRegionChanges)
		{
			return;
		}

		const Slot Where = SlotsOf(CueIndex).at(SlotIndex);
		const size_t Synth = MakeSlotSynthPrivate(CueIndex, Where);

		if (bRegionChanges)
		{
			UtfTable& Synths = Require(HeaderSynth);
			std::vector<ReferenceItem> Items = UnpackReferenceItems(Synths.GetBytes(Synth, "ReferenceItems"));
			size_t Waveform = Items.at(Where.ItemPosition).second;
			if (WaveformReferenceCount(Waveform) > 1)
			{
				Waveforms.InsertRow(Waveforms.RowCount(), Waveforms.Rows[Waveform]);
				const size_t NewWaveform = CheckU16(Waveforms.RowCount() - 1, "Waveform");
				Waveforms.SetUnsigned(NewWaveform, "ExtensionData", 0xFFFF);
				Items.at(Where.ItemPosition).second = uint16_t(NewWaveform);
				Synths.SetBytes(Synth, "ReferenceItems", PackReferenceItems(Items));
				Waveform = NewWaveform;
			}
			uint64_t Extension = Waveforms.GetUnsigned(Waveform, "ExtensionData");
			bool bSharedExtension = false;
			if (Extension < Extensions->RowCount())
			{
				for (size_t Row = 0; Row < Waveforms.RowCount(); ++Row)
				{
					if (Row != Waveform && Waveforms.GetUnsigned(Row, "ExtensionData") == Extension)
					{
						bSharedExtension = true;
						break;
					}
				}
			}
			if (Extension >= Extensions->RowCount() || bSharedExtension)
			{
				Extension = Extensions->AddRow({ { "LoopStart", CellU(LoopStart) }, { "LoopEnd", CellU(LoopEnd) } });
				Waveforms.SetUnsigned(Waveform, "ExtensionData", Extension);
			}
			else
			{
				Extensions->SetUnsigned(size_t(Extension), "LoopStart", LoopStart);
				Extensions->SetUnsigned(size_t(Extension), "LoopEnd", LoopEnd);
			}
			Waveforms.SetUnsigned(Waveform, "LoopFlag", 2);
		}
		if (bLoop || Current.bWaveformLoops)
		{
			SetSynthIgnoreLoop(Synth, !bLoop);
		}

		bool bAnyLoop = false;
		uint32_t Longest = 0;
		const size_t SlotCount = SlotsOf(CueIndex).size();
		for (size_t Index = 0; Index < SlotCount; ++Index)
		{
			const SlotTiming Timing = GetSlotTiming(CueIndex, Index);
			bAnyLoop |= Timing.bLoop;
			Longest = std::max(Longest, ComputeLengthMs(Timing.WaveformIndex));
		}
		Require(HeaderCue).SetUnsigned(CueIndex, "Length", bAnyLoop ? 0xFFFFFFFFull : uint64_t(Longest));
	}
}
