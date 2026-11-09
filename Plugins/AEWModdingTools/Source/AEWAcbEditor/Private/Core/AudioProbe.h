#pragma once

#include "AcbIO.h"

#include <optional>
#include <string>

namespace AcbCore
{
	enum class EncodeType : uint8_t
	{
		ADX = 0,
		HCA = 2,
		HCA_MX = 6,
		VAG = 7,
		ATRAC3 = 8,
		BCWAV = 9
	};

	struct AudioInfo
	{
		uint32_t NumChannels = 0;
		uint32_t SamplingRate = 0;
		uint32_t NumSamples = 0;
		std::optional<uint32_t> LoopStart;
		std::optional<uint32_t> LoopEnd;
	};

	std::optional<EncodeType> DetectEncodeType(const std::string& Utf8Path);

	AudioInfo ProbeHca(const std::string& Utf8Path);
	AudioInfo ProbeAdx(const std::string& Utf8Path);
	AudioInfo ProbeAudioFile(const std::string& Utf8Path, EncodeType Type);
}
