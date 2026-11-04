#pragma once

#include "AcbIO.h"

#include <optional>
#include <vector>

namespace AcbCore
{
	namespace AcbOp
	{
		constexpr uint16_t Null = 0x0000;
		constexpr uint16_t Category = 65;
		constexpr uint16_t ReferenceItem = 2000;
		constexpr uint16_t StartOffset = 997;
		constexpr uint16_t IgnoreLoop = 117;
		constexpr uint16_t Volume = 87;
	}

	struct AcbCommand
	{
		uint16_t Op = 0;
		std::vector<uint8_t> Params;

		uint64_t ValueBE() const;
	};

	std::vector<AcbCommand> DecodeCommands(const std::vector<uint8_t>& Bytes);
	std::vector<uint8_t> EncodeCommands(const std::vector<AcbCommand>& Commands);
	std::optional<uint64_t> FindCommandValue(const std::vector<uint8_t>& Bytes, uint16_t Op);
}
