#include "Afs2.h"

#include <algorithm>
#include <cstring>

namespace AcbCore
{
	namespace
	{
		uint64_t ReadLEBytes(const uint8_t* P, size_t Count)
		{
			uint64_t Value = 0;
			for (size_t Index = 0; Index < Count; ++Index)
			{
				Value |= uint64_t(P[Index]) << (8 * Index);
			}
			return Value;
		}

		void PutLEBytes(std::vector<uint8_t>& V, uint64_t Value, size_t Count)
		{
			for (size_t Index = 0; Index < Count; ++Index)
			{
				V.push_back(uint8_t(Value >> (8 * Index)));
			}
		}

		uint64_t HeaderSize(size_t Count, size_t IdSize, size_t OffsetSize)
		{
			return 16 + uint64_t(Count) * IdSize + uint64_t(Count + 1) * OffsetSize;
		}
	}

	std::shared_ptr<Afs2Archive> Afs2Archive::Parse(const std::shared_ptr<ReadStream>& Stream, uint64_t Base, uint64_t Size)
	{
		if (Size < 16 || Base + Size > Stream->Size())
		{
			return nullptr;
		}
		uint8_t Fixed[16];
		Stream->ReadAt(Base, Fixed, 16);
		if (std::memcmp(Fixed, "AFS2", 4) != 0)
		{
			return nullptr;
		}
		auto Archive = std::make_shared<Afs2Archive>();
		Archive->Version = Fixed[4];
		Archive->OffsetSize = Fixed[5];
		Archive->IdSize = ReadLE16(Fixed + 6);
		const uint32_t Count = ReadLE32(Fixed + 8);
		Archive->Alignment = ReadLE16(Fixed + 12);
		Archive->Subkey = ReadLE16(Fixed + 14);
		if ((Archive->OffsetSize != 2 && Archive->OffsetSize != 4 && Archive->OffsetSize != 8)
			|| (Archive->IdSize != 2 && Archive->IdSize != 4) || Archive->Alignment == 0)
		{
			return nullptr;
		}
		const uint64_t TableSize = HeaderSize(Count, Archive->IdSize, Archive->OffsetSize);
		if (TableSize > Size || TableSize > (64ull << 20))
		{
			return nullptr;
		}
		std::vector<uint8_t> Table = Stream->ReadVector(Base, size_t(TableSize));
		const uint8_t* Ids = Table.data() + 16;
		const uint8_t* Offsets = Ids + size_t(Count) * Archive->IdSize;

		Archive->Entries.reserve(Count);
		for (uint32_t Index = 0; Index < Count; ++Index)
		{
			const uint64_t Start = AlignUp(ReadLEBytes(Offsets + size_t(Index) * Archive->OffsetSize, Archive->OffsetSize), Archive->Alignment);
			const uint64_t End = ReadLEBytes(Offsets + size_t(Index + 1) * Archive->OffsetSize, Archive->OffsetSize);
			if (End > Size || Start > End)
			{
				return nullptr;
			}
			Afs2Entry Entry;
			Entry.Id = uint32_t(ReadLEBytes(Ids + size_t(Index) * Archive->IdSize, Archive->IdSize));
			Entry.Source = Stream;
			Entry.Offset = Base + Start;
			Entry.Size = End - Start;
			Archive->Entries.push_back(std::move(Entry));
		}
		return Archive;
	}

	std::shared_ptr<Afs2Archive> Afs2Archive::Open(const std::string& Utf8Path)
	{
		std::shared_ptr<ReadStream> Stream = OpenFileRead(Utf8Path);
		auto Archive = Parse(Stream, 0, Stream->Size());
		if (!Archive)
		{
			throw AcbError(Utf8Path + " is not a valid AWB (AFS2) file");
		}
		return Archive;
	}

	uint32_t Afs2Archive::NextId() const
	{
		uint32_t Next = 0;
		for (const Afs2Entry& Entry : Entries)
		{
			Next = std::max(Next, Entry.Id + 1);
		}
		return Next;
	}

	int Afs2Archive::IndexOf(uint32_t Id) const
	{
		for (size_t Index = 0; Index < Entries.size(); ++Index)
		{
			if (Entries[Index].Id == Id)
			{
				return int(Index);
			}
		}
		return -1;
	}

	const Afs2Entry* Afs2Archive::Find(uint32_t Id) const
	{
		const int Index = IndexOf(Id);
		return Index >= 0 ? &Entries[size_t(Index)] : nullptr;
	}

	uint32_t Afs2Archive::AddFile(const std::string& Utf8Path, std::optional<uint32_t> Id)
	{
		const uint32_t NewId = Id ? *Id : NextId();
		if (IndexOf(NewId) >= 0)
		{
			throw AcbError("AWB entry id " + std::to_string(NewId) + " already in use");
		}
		if (IdSize == 2 && NewId > 0xFFFF)
		{
			throw AcbError("AWB entry id " + std::to_string(NewId) + " does not fit in 16 bits");
		}
		Afs2Entry Entry;
		Entry.Id = NewId;
		Entry.Source = OpenFileRead(Utf8Path);
		Entry.Offset = 0;
		Entry.Size = Entry.Source->Size();
		Entry.SourcePath = Utf8Path;
		auto Position = std::find_if(Entries.begin(), Entries.end(), [NewId](const Afs2Entry& Existing) { return Existing.Id > NewId; });
		Entries.insert(Position, std::move(Entry));
		return NewId;
	}

	void Afs2Archive::RemoveEntry(uint32_t Id)
	{
		const int Index = IndexOf(Id);
		if (Index < 0)
		{
			throw AcbError("No AWB entry with id " + std::to_string(Id));
		}
		Entries.erase(Entries.begin() + Index);
	}

	uint8_t Afs2Archive::EffectiveOffsetSize() const
	{
		uint64_t Position = HeaderSize(Entries.size(), IdSize, OffsetSize);
		for (const Afs2Entry& Entry : Entries)
		{
			Position = AlignUp(Position, Alignment) + Entry.Size;
		}
		if (OffsetSize == 2 && Position > 0xFFFF)
		{
			return 4;
		}
		if (OffsetSize == 4 && Position > 0xFFFFFFFFull)
		{
			return 8;
		}
		return OffsetSize;
	}

	std::vector<uint8_t> Afs2Archive::BuildHeader() const
	{
		const uint8_t Width = EffectiveOffsetSize();
		std::vector<uint8_t> Header;
		Header.insert(Header.end(), { 'A', 'F', 'S', '2' });
		Header.push_back(Version);
		Header.push_back(Width);
		PutLE16(Header, IdSize);
		PutLE32(Header, uint32_t(Entries.size()));
		PutLE16(Header, Alignment);
		PutLE16(Header, Subkey);
		for (const Afs2Entry& Entry : Entries)
		{
			PutLEBytes(Header, Entry.Id, IdSize);
		}
		uint64_t Position = HeaderSize(Entries.size(), IdSize, Width);
		PutLEBytes(Header, Position, Width);
		for (const Afs2Entry& Entry : Entries)
		{
			Position = AlignUp(Position, Alignment) + Entry.Size;
			PutLEBytes(Header, Position, Width);
		}
		return Header;
	}

	uint64_t Afs2Archive::ComputeSize() const
	{
		uint64_t Position = HeaderSize(Entries.size(), IdSize, EffectiveOffsetSize());
		for (const Afs2Entry& Entry : Entries)
		{
			Position = AlignUp(Position, Alignment) + Entry.Size;
		}
		return Position;
	}

	void Afs2Archive::Write(WriteStream& Out, Progress* ProgressSink) const
	{
		const std::vector<uint8_t> Header = BuildHeader();
		Out.Write(Header);
		uint64_t Position = Header.size();
		for (const Afs2Entry& Entry : Entries)
		{
			const uint64_t Start = AlignUp(Position, Alignment);
			Out.WriteZeros(Start - Position);
			if (Entry.Source->Size() < Entry.Offset + Entry.Size)
			{
				throw AcbError("AWB entry " + std::to_string(Entry.Id) + " source changed size since it was added");
			}
			uint64_t Offset = Entry.Offset;
			uint64_t Remaining = Entry.Size;
			while (Remaining > 0)
			{
				const uint64_t Chunk = std::min<uint64_t>(Remaining, 16u << 20);
				Out.CopyFrom(*Entry.Source, Offset, Chunk);
				Offset += Chunk;
				Remaining -= Chunk;
				if (ProgressSink)
				{
					ProgressSink->Advance(Chunk);
				}
			}
			Position = Start + Entry.Size;
		}
	}
}
