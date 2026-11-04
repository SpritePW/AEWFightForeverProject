#include "AcbCommands.h"

namespace AcbCore
{
	uint64_t AcbCommand::ValueBE() const
	{
		uint64_t Value = 0;
		for (size_t Index = 0; Index < Params.size() && Index < 8; ++Index)
		{
			Value = (Value << 8) | Params[Index];
		}
		return Value;
	}

	std::vector<AcbCommand> DecodeCommands(const std::vector<uint8_t>& Bytes)
	{
		std::vector<AcbCommand> Commands;
		size_t Pos = 0;
		while (Bytes.size() - Pos >= 3)
		{
			AcbCommand Command;
			Command.Op = ReadBE16(Bytes.data() + Pos);
			const size_t Length = Bytes[Pos + 2];
			Pos += 3;
			if (Pos + Length > Bytes.size())
			{
				throw AcbError("Truncated ACB command stream");
			}
			Command.Params.assign(Bytes.begin() + std::ptrdiff_t(Pos), Bytes.begin() + std::ptrdiff_t(Pos + Length));
			Pos += Length;
			Commands.push_back(std::move(Command));
		}
		return Commands;
	}

	std::vector<uint8_t> EncodeCommands(const std::vector<AcbCommand>& Commands)
	{
		std::vector<uint8_t> Bytes;
		for (const AcbCommand& Command : Commands)
		{
			if (Command.Params.size() > 255)
			{
				throw AcbError("ACB command parameters are limited to 255 bytes");
			}
			PutBE16(Bytes, Command.Op);
			Bytes.push_back(uint8_t(Command.Params.size()));
			Bytes.insert(Bytes.end(), Command.Params.begin(), Command.Params.end());
		}
		return Bytes;
	}

	std::optional<uint64_t> FindCommandValue(const std::vector<uint8_t>& Bytes, uint16_t Op)
	{
		for (const AcbCommand& Command : DecodeCommands(Bytes))
		{
			if (Command.Op == Op)
			{
				return Command.ValueBE();
			}
		}
		return std::nullopt;
	}
}
