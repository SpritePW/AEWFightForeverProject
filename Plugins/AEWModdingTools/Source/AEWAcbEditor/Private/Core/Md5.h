#pragma once

#include "AcbIO.h"

#include <array>

namespace AcbCore
{
	class Md5
	{
	public:
		Md5();
		void Update(const void* Data, size_t Count);
		std::array<uint8_t, 16> Finish();

	private:
		void Transform(const uint8_t Block[64]);

		uint32_t State[4];
		uint64_t Length = 0;
		uint8_t Buffer[64];
		size_t BufferUsed = 0;
	};

	class HashingWriteStream final : public WriteStream
	{
	public:
		explicit HashingWriteStream(WriteStream& InInner) : Inner(InInner) {}
		void Write(const void* Data, size_t Count) override
		{
			Hash.Update(Data, Count);
			Inner.Write(Data, Count);
		}
		uint64_t Tell() const override { return Inner.Tell(); }
		std::array<uint8_t, 16> Finish() { return Hash.Finish(); }

		using WriteStream::Write;

	private:
		WriteStream& Inner;
		Md5 Hash;
	};
}
