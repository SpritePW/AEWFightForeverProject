#include "AudioProbe.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace AcbCore
{
	namespace
	{
		std::vector<uint8_t> ReadPrefix(const std::string& Path, size_t Count)
		{
			std::shared_ptr<ReadStream> Stream = OpenFileRead(Path);
			const size_t Available = size_t(std::min<uint64_t>(Stream->Size(), Count));
			return Stream->ReadVector(0, Available);
		}

		bool ChunkIs(const uint8_t* P, const char* Id)
		{
			for (int Index = 0; Index < 4; ++Index)
			{
				if ((P[Index] & 0x7F) != uint8_t(Id[Index]))
				{
					return false;
				}
			}
			return true;
		}

		long FindChunk(const std::vector<uint8_t>& Header, const char* Id)
		{
			for (size_t Index = 0; Index + 4 <= Header.size(); ++Index)
			{
				if (ChunkIs(Header.data() + Index, Id))
				{
					return long(Index);
				}
			}
			return -1;
		}

		uint32_t ReadBE24(const uint8_t* P)
		{
			return (uint32_t(P[0]) << 16) | (uint32_t(P[1]) << 8) | uint32_t(P[2]);
		}
	}

	std::optional<EncodeType> DetectEncodeType(const std::string& Utf8Path)
	{
		const std::vector<uint8_t> Head = ReadPrefix(Utf8Path, 4);
		if (Head.size() >= 4 && ChunkIs(Head.data(), "HCA\0"))
		{
			return EncodeType::HCA;
		}
		if (Head.size() >= 2 && Head[0] == 0x80 && Head[1] == 0x00)
		{
			return EncodeType::ADX;
		}
		return std::nullopt;
	}

	AudioInfo ProbeHca(const std::string& Utf8Path)
	{
		std::vector<uint8_t> Header = ReadPrefix(Utf8Path, 8);
		if (Header.size() < 8 || !ChunkIs(Header.data(), "HCA\0"))
		{
			throw AcbError(Utf8Path + ": not a valid HCA file (bad magic)");
		}
		const uint16_t HeaderSize = ReadBE16(Header.data() + 6);
		Header = ReadPrefix(Utf8Path, HeaderSize > 8 ? HeaderSize : 1024);

		const long Fmt = FindChunk(Header, "fmt\0");
		if (Fmt < 0 || size_t(Fmt) + 16 > Header.size())
		{
			throw AcbError(Utf8Path + ": no 'fmt' chunk in HCA header");
		}
		const uint8_t* F = Header.data() + Fmt;
		AudioInfo Info;
		Info.NumChannels = F[4];
		Info.SamplingRate = ReadBE24(F + 5);
		const uint32_t BlockCount = ReadBE32(F + 8);
		const uint32_t EncoderDelay = ReadBE16(F + 12);
		const uint32_t EncoderPadding = ReadBE16(F + 14);
		const int64_t Samples = int64_t(BlockCount) * 1024 - EncoderDelay - EncoderPadding;
		Info.NumSamples = uint32_t(std::max<int64_t>(Samples, 0));

		const long Loop = FindChunk(Header, "loop");
		if (Loop >= 0 && size_t(Loop) + 16 <= Header.size())
		{
			const uint8_t* L = Header.data() + Loop;
			const int64_t StartFrame = ReadBE32(L + 4);
			const int64_t EndFrame = ReadBE32(L + 8);
			const int64_t StartDelay = ReadBE16(L + 12);
			const int64_t EndPadding = ReadBE16(L + 14);
			int64_t Start = StartFrame * 1024 + StartDelay - EncoderDelay;
			int64_t End = (EndFrame + 1) * 1024 - EndPadding - EncoderDelay;
			Start = std::max<int64_t>(Start, 0);
			End = std::min<int64_t>(std::max<int64_t>(End, Start), Info.NumSamples);
			Info.LoopStart = uint32_t(Start);
			Info.LoopEnd = uint32_t(End);
		}
		return Info;
	}

	AudioInfo ProbeAdx(const std::string& Utf8Path)
	{
		const std::vector<uint8_t> Header = ReadPrefix(Utf8Path, 20);
		if (Header.size() < 20 || Header[0] != 0x80 || Header[1] != 0x00)
		{
			throw AcbError(Utf8Path + ": not a valid ADX file");
		}
		AudioInfo Info;
		Info.NumChannels = Header[7];
		Info.SamplingRate = ReadBE32(Header.data() + 8);
		Info.NumSamples = ReadBE32(Header.data() + 12);
		return Info;
	}

	AudioInfo ProbeAudioFile(const std::string& Utf8Path, EncodeType Type)
	{
		if (Type == EncodeType::HCA)
		{
			return ProbeHca(Utf8Path);
		}
		if (Type == EncodeType::ADX)
		{
			return ProbeAdx(Utf8Path);
		}
		throw AcbError("Can't probe this encode type -- pass channels/sample rate/sample count manually");
	}
}
