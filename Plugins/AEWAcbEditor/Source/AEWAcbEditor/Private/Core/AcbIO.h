#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace AcbCore
{
	class AcbError : public std::runtime_error
	{
	public:
		explicit AcbError(const std::string& Message) : std::runtime_error(Message) {}
	};

	class ReadStream
	{
	public:
		virtual ~ReadStream() = default;
		virtual uint64_t Size() const = 0;
		virtual void ReadAt(uint64_t Offset, void* Dest, size_t Count) = 0;

		std::vector<uint8_t> ReadVector(uint64_t Offset, size_t Count)
		{
			std::vector<uint8_t> Out(Count);
			if (Count > 0)
			{
				ReadAt(Offset, Out.data(), Count);
			}
			return Out;
		}
	};

	class WriteStream
	{
	public:
		virtual ~WriteStream() = default;
		virtual void Write(const void* Data, size_t Count) = 0;
		virtual uint64_t Tell() const = 0;

		void Write(const std::vector<uint8_t>& Bytes)
		{
			if (!Bytes.empty())
			{
				Write(Bytes.data(), Bytes.size());
			}
		}
		void WriteZeros(uint64_t Count);
		void CopyFrom(ReadStream& Source, uint64_t Offset, uint64_t Count);
	};

	class MemoryReadStream final : public ReadStream
	{
	public:
		explicit MemoryReadStream(std::vector<uint8_t> InBytes) : Bytes(std::move(InBytes)) {}
		uint64_t Size() const override { return Bytes.size(); }
		void ReadAt(uint64_t Offset, void* Dest, size_t Count) override;
		const std::vector<uint8_t>& Data() const { return Bytes; }

	private:
		std::vector<uint8_t> Bytes;
	};

	class MemoryWriteStream final : public WriteStream
	{
	public:
		void Write(const void* Data, size_t Count) override;
		uint64_t Tell() const override { return Bytes.size(); }
		std::vector<uint8_t>& Data() { return Bytes; }

		using WriteStream::Write;

	private:
		std::vector<uint8_t> Bytes;
	};

	class CountingWriteStream final : public WriteStream
	{
	public:
		void Write(const void*, size_t Count) override { Position += Count; }
		uint64_t Tell() const override { return Position; }

		using WriteStream::Write;

	private:
		uint64_t Position = 0;
	};

	std::shared_ptr<ReadStream> OpenFileRead(const std::string& Utf8Path);
	std::unique_ptr<WriteStream> OpenFileWrite(const std::string& Utf8Path);
	bool FileExists(const std::string& Utf8Path);
	bool DeleteFileIfExists(const std::string& Utf8Path);
	bool MoveFileReplace(const std::string& FromUtf8, const std::string& ToUtf8);

	using ProgressCallback = bool (*)(void* User, uint64_t Done, uint64_t Total);

	struct Progress
	{
		ProgressCallback Callback = nullptr;
		void* User = nullptr;
		uint64_t Total = 0;
		uint64_t Done = 0;

		void Advance(uint64_t Bytes);
	};

	inline uint16_t ReadBE16(const uint8_t* P) { return uint16_t((P[0] << 8) | P[1]); }
	inline uint32_t ReadBE32(const uint8_t* P) { return (uint32_t(P[0]) << 24) | (uint32_t(P[1]) << 16) | (uint32_t(P[2]) << 8) | uint32_t(P[3]); }
	inline uint64_t ReadBE64(const uint8_t* P) { return (uint64_t(ReadBE32(P)) << 32) | ReadBE32(P + 4); }
	inline uint16_t ReadLE16(const uint8_t* P) { return uint16_t(P[0] | (P[1] << 8)); }
	inline uint32_t ReadLE32(const uint8_t* P) { return uint32_t(P[0]) | (uint32_t(P[1]) << 8) | (uint32_t(P[2]) << 16) | (uint32_t(P[3]) << 24); }

	inline void PutBE16(std::vector<uint8_t>& V, uint16_t X) { V.push_back(uint8_t(X >> 8)); V.push_back(uint8_t(X)); }
	inline void PutBE32(std::vector<uint8_t>& V, uint32_t X) { for (int S = 24; S >= 0; S -= 8) V.push_back(uint8_t(X >> S)); }
	inline void PutBE64(std::vector<uint8_t>& V, uint64_t X) { for (int S = 56; S >= 0; S -= 8) V.push_back(uint8_t(X >> S)); }
	inline void PutLE16(std::vector<uint8_t>& V, uint16_t X) { V.push_back(uint8_t(X)); V.push_back(uint8_t(X >> 8)); }
	inline void PutLE32(std::vector<uint8_t>& V, uint32_t X) { for (int S = 0; S <= 24; S += 8) V.push_back(uint8_t(X >> S)); }
	inline void SetBE32(uint8_t* P, uint32_t X) { P[0] = uint8_t(X >> 24); P[1] = uint8_t(X >> 16); P[2] = uint8_t(X >> 8); P[3] = uint8_t(X); }

	inline uint64_t AlignUp(uint64_t Value, uint64_t Alignment)
	{
		return Alignment <= 1 ? Value : (Value + Alignment - 1) / Alignment * Alignment;
	}
}
