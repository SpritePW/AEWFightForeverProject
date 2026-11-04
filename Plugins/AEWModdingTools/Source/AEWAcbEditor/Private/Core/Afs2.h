#pragma once

#include "AcbIO.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AcbCore
{
	struct Afs2Entry
	{
		uint32_t Id = 0;
		std::shared_ptr<ReadStream> Source;
		uint64_t Offset = 0;
		uint64_t Size = 0;
		std::string SourcePath;
	};

	class Afs2Archive
	{
	public:
		uint8_t Version = 2;
		uint8_t OffsetSize = 4;
		uint16_t IdSize = 2;
		uint16_t Alignment = 32;
		uint16_t Subkey = 0;
		std::vector<Afs2Entry> Entries;

		static std::shared_ptr<Afs2Archive> Parse(const std::shared_ptr<ReadStream>& Stream, uint64_t Base, uint64_t Size);
		static std::shared_ptr<Afs2Archive> Open(const std::string& Utf8Path);

		uint32_t NextId() const;
		int IndexOf(uint32_t Id) const;
		const Afs2Entry* Find(uint32_t Id) const;
		uint32_t AddFile(const std::string& Utf8Path, std::optional<uint32_t> Id = std::nullopt);
		void RemoveEntry(uint32_t Id);

		std::vector<uint8_t> BuildHeader() const;
		uint64_t ComputeSize() const;
		void Write(WriteStream& Out, Progress* ProgressSink) const;

	private:
		uint8_t EffectiveOffsetSize() const;
	};
}
