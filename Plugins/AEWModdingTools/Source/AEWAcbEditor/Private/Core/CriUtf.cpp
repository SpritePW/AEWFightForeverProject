#include "CriUtf.h"
#include "Afs2.h"

#include <cstring>
#include <functional>

namespace AcbCore
{
	namespace
	{
		const uint8_t UtfMagic[4] = { '@', 'U', 'T', 'F' };

		uint64_t TypeMask(UtfType Type)
		{
			switch (UtfTypeSize(Type))
			{
			case 1: return 0xFFull;
			case 2: return 0xFFFFull;
			case 4: return 0xFFFFFFFFull;
			default: return ~0ull;
			}
		}

		UtfWriteOptions NestedWriteOptions()
		{
			UtfWriteOptions Options;
			Options.DataAlignment = 1;
			Options.EndAlignment = 4;
			return Options;
		}

		uint64_t ReadBEBytes(const uint8_t* P, size_t Count)
		{
			uint64_t Value = 0;
			for (size_t Index = 0; Index < Count; ++Index)
			{
				Value = (Value << 8) | P[Index];
			}
			return Value;
		}

		void PutBEBytes(std::vector<uint8_t>& V, uint64_t Value, size_t Count)
		{
			for (size_t Index = 0; Index < Count; ++Index)
			{
				V.push_back(uint8_t(Value >> (8 * (Count - 1 - Index))));
			}
		}

		std::shared_ptr<UtfTable> ParseTable(const uint8_t* Head, size_t HeadSize, uint64_t TotalSize,
			const std::function<UtfData(uint64_t, uint64_t)>& FetchBlob)
		{
			if (HeadSize < 0x20 || std::memcmp(Head, UtfMagic, 4) != 0)
			{
				throw AcbError("Not a @UTF table");
			}
			const uint8_t* Base = Head + 8;
			const uint64_t BodySize = TotalSize - 8;
			const uint16_t RowsOffset = ReadBE16(Base + 2);
			const uint32_t StringsOffset = ReadBE32(Base + 4);
			const uint32_t DataOffset = ReadBE32(Base + 8);
			const int32_t NameOffset = int32_t(ReadBE32(Base + 12));
			const uint16_t NumColumns = ReadBE16(Base + 16);
			const uint16_t RowLength = ReadBE16(Base + 18);
			const uint32_t NumRows = ReadBE32(Base + 20);

			if (StringsOffset > BodySize || DataOffset > BodySize || StringsOffset > DataOffset
				|| uint64_t(RowsOffset) + uint64_t(RowLength) * NumRows > StringsOffset
				|| 8 + uint64_t(DataOffset) > HeadSize)
			{
				throw AcbError("Corrupt @UTF table header");
			}

			auto ReadString = [&](int64_t Offset) -> std::string
			{
				if (Offset < 0)
				{
					return std::string();
				}
				const uint64_t Start = uint64_t(StringsOffset) + uint64_t(Offset);
				if (Start >= DataOffset)
				{
					throw AcbError("@UTF string offset out of range");
				}
				const uint8_t* P = Base + Start;
				const uint8_t* End = Base + DataOffset;
				const uint8_t* Stop = static_cast<const uint8_t*>(std::memchr(P, 0, size_t(End - P)));
				if (!Stop)
				{
					throw AcbError("Unterminated @UTF string");
				}
				return std::string(reinterpret_cast<const char*>(P), size_t(Stop - P));
			};

			auto ReadTyped = [&](const uint8_t* P, UtfType Type) -> UtfValue
			{
				UtfValue Value;
				Value.Type = Type;
				switch (Type)
				{
				case UtfType::String:
					Value.Text = ReadString(int32_t(ReadBE32(P)));
					break;
				case UtfType::Data:
				{
					const uint32_t Offset = ReadBE32(P);
					const uint32_t Size = ReadBE32(P + 4);
					if (Size > 0)
					{
						if (uint64_t(DataOffset) + Offset + Size > BodySize)
						{
							throw AcbError("@UTF data cell out of range");
						}
						Value.Data = FetchBlob(uint64_t(DataOffset) + Offset, Size);
					}
					break;
				}
				case UtfType::Guid:
					std::memcpy(Value.Guid, P, 16);
					break;
				default:
					Value.Bits = ReadBEBytes(P, UtfTypeSize(Type));
					break;
				}
				return Value;
			};

			auto Table = std::make_shared<UtfTable>();
			Table->Unknown0 = Base[0];
			Table->Encoding = Base[1];
			Table->Name = ReadString(NameOffset);

			const uint8_t* Pos = Base + 0x18;
			const uint8_t* SchemaEnd = Base + RowsOffset;
			std::vector<size_t> RowColumnOffsets;
			size_t RowCursor = 0;
			for (uint16_t ColumnIndex = 0; ColumnIndex < NumColumns; ++ColumnIndex)
			{
				if (Pos + 5 > SchemaEnd)
				{
					throw AcbError("Corrupt @UTF schema");
				}
				const uint8_t Flags = Pos[0];
				UtfColumn Column;
				Column.Type = UtfType(Flags & 0x0F);
				if (uint8_t(Column.Type) > uint8_t(UtfType::Guid))
				{
					throw AcbError("Unknown @UTF column type");
				}
				const uint8_t StorageBits = Flags & 0xF0;
				if (StorageBits != 0x10 && StorageBits != 0x30 && StorageBits != 0x50)
				{
					throw AcbError("Unsupported @UTF column storage flags");
				}
				Column.Storage = UtfStorage(StorageBits);
				Column.Name = ReadString(int32_t(ReadBE32(Pos + 1)));
				Pos += 5;
				if (Column.Storage == UtfStorage::Constant)
				{
					const size_t Size = UtfTypeSize(Column.Type);
					if (Pos + Size > SchemaEnd)
					{
						throw AcbError("Corrupt @UTF schema constant");
					}
					Column.Constant = ReadTyped(Pos, Column.Type);
					Pos += Size;
				}
				else
				{
					Column.Constant = UtfValue::Default(Column.Type);
				}
				RowColumnOffsets.push_back(RowCursor);
				if (Column.Storage == UtfStorage::PerRow)
				{
					RowCursor += UtfTypeSize(Column.Type);
				}
				Table->Columns.push_back(std::move(Column));
			}
			if (RowCursor > RowLength)
			{
				if (NumRows > 1 || uint64_t(RowsOffset) + RowCursor > StringsOffset)
				{
					throw AcbError("@UTF row length mismatch");
				}
			}

			Table->Rows.reserve(NumRows);
			for (uint32_t RowIndex = 0; RowIndex < NumRows; ++RowIndex)
			{
				const uint8_t* RowStart = Base + RowsOffset + size_t(RowIndex) * RowLength;
				std::vector<UtfValue> Row;
				Row.reserve(NumColumns);
				for (size_t ColumnIndex = 0; ColumnIndex < Table->Columns.size(); ++ColumnIndex)
				{
					const UtfColumn& Column = Table->Columns[ColumnIndex];
					if (Column.Storage == UtfStorage::PerRow)
					{
						Row.push_back(ReadTyped(RowStart + RowColumnOffsets[ColumnIndex], Column.Type));
					}
					else
					{
						Row.push_back(Column.Constant);
					}
				}
				Table->Rows.push_back(std::move(Row));
			}
			return Table;
		}

		bool StartsWith(const std::vector<uint8_t>& Bytes, const char* Magic)
		{
			return Bytes.size() >= 4 && std::memcmp(Bytes.data(), Magic, 4) == 0;
		}

		UtfData MakeInlineData(std::vector<uint8_t> Bytes, const UtfParseOptions& Options)
		{
			UtfData Data;
			if (Options.ParseNestedTables && UtfTable::LooksLikeTable(Bytes.data(), Bytes.size()))
			{
				try
				{
					Data.Table = UtfTable::Parse(Bytes.data(), Bytes.size(), Options);
					return Data;
				}
				catch (const AcbError&)
				{
					Data.Table.reset();
				}
			}
			if (Options.ParseAwb && StartsWith(Bytes, "AFS2"))
			{
				auto Stream = std::make_shared<MemoryReadStream>(Bytes);
				if (auto Awb = Afs2Archive::Parse(Stream, 0, Bytes.size()))
				{
					Data.Awb = Awb;
					return Data;
				}
			}
			Data.Bytes = std::move(Bytes);
			return Data;
		}
	}

	const char* UtfTypeName(UtfType Type)
	{
		static const char* Names[] = { "UINT8", "SINT8", "UINT16", "SINT16", "UINT32", "SINT32", "UINT64", "SINT64",
			"FLOAT", "DOUBLE", "STRING", "VLDATA", "GUID" };
		const uint8_t Index = uint8_t(Type);
		return Index < sizeof(Names) / sizeof(Names[0]) ? Names[Index] : "UNKNOWN";
	}

	size_t UtfTypeSize(UtfType Type)
	{
		switch (Type)
		{
		case UtfType::U8: case UtfType::S8: return 1;
		case UtfType::U16: case UtfType::S16: return 2;
		case UtfType::U32: case UtfType::S32: case UtfType::F32: case UtfType::String: return 4;
		case UtfType::U64: case UtfType::S64: case UtfType::F64: case UtfType::Data: return 8;
		case UtfType::Guid: return 16;
		}
		return 0;
	}

	bool UtfTypeIsInteger(UtfType Type)
	{
		return uint8_t(Type) <= uint8_t(UtfType::S64);
	}

	bool UtfData::IsEmpty() const
	{
		return Bytes.empty() && !Table && !Awb && SourceSize == 0;
	}

	std::vector<uint8_t> UtfData::ToBytes() const
	{
		if (Table)
		{
			return Table->IsDirty() || Table->OriginalBytes().empty() ? Table->Serialize(NestedWriteOptions()) : Table->OriginalBytes();
		}
		if (Awb)
		{
			MemoryWriteStream Out;
			Awb->Write(Out, nullptr);
			return Out.Data();
		}
		if (Source)
		{
			if (SourceSize > (256ull << 20))
			{
				throw AcbError("Blob too large to load into memory");
			}
			return Source->ReadVector(SourceOffset, size_t(SourceSize));
		}
		return Bytes;
	}

	uint64_t UtfDataWriteSize(const UtfData& Data)
	{
		if (Data.Table)
		{
			return Data.Table->IsDirty() || Data.Table->OriginalBytes().empty()
				? Data.Table->ComputeSize(NestedWriteOptions())
				: Data.Table->OriginalBytes().size();
		}
		if (Data.Awb)
		{
			return Data.Awb->ComputeSize();
		}
		if (Data.Source)
		{
			return Data.SourceSize;
		}
		return Data.Bytes.size();
	}

	void UtfDataWrite(const UtfData& Data, WriteStream& Out, Progress* ProgressSink)
	{
		if (Data.Table)
		{
			if (Data.Table->IsDirty() || Data.Table->OriginalBytes().empty())
			{
				Data.Table->Write(Out, NestedWriteOptions());
			}
			else
			{
				Out.Write(Data.Table->OriginalBytes());
			}
			if (ProgressSink)
			{
				ProgressSink->Advance(UtfDataWriteSize(Data));
			}
			return;
		}
		if (Data.Awb)
		{
			Data.Awb->Write(Out, ProgressSink);
			return;
		}
		if (Data.Source)
		{
			uint64_t Offset = Data.SourceOffset;
			uint64_t Remaining = Data.SourceSize;
			while (Remaining > 0)
			{
				const uint64_t Chunk = Remaining < (16u << 20) ? Remaining : (16u << 20);
				Out.CopyFrom(*Data.Source, Offset, Chunk);
				Offset += Chunk;
				Remaining -= Chunk;
				if (ProgressSink)
				{
					ProgressSink->Advance(Chunk);
				}
			}
			return;
		}
		Out.Write(Data.Bytes);
		if (ProgressSink)
		{
			ProgressSink->Advance(Data.Bytes.size());
		}
	}

	UtfValue UtfValue::Integer(UtfType InType, uint64_t Value)
	{
		UtfValue Result;
		Result.Type = InType;
		Result.Bits = Value & TypeMask(InType);
		return Result;
	}

	UtfValue UtfValue::Float(double Value)
	{
		UtfValue Result;
		Result.Type = UtfType::F32;
		Result.SetDouble(Value);
		return Result;
	}

	UtfValue UtfValue::Double(double Value)
	{
		UtfValue Result;
		Result.Type = UtfType::F64;
		Result.SetDouble(Value);
		return Result;
	}

	UtfValue UtfValue::String(const std::string& Value)
	{
		UtfValue Result;
		Result.Type = UtfType::String;
		Result.Text = Value;
		return Result;
	}

	UtfValue UtfValue::Blob(const std::vector<uint8_t>& Bytes)
	{
		UtfValue Result;
		Result.Type = UtfType::Data;
		Result.Data.Bytes = Bytes;
		return Result;
	}

	UtfValue UtfValue::Default(UtfType InType)
	{
		UtfValue Result;
		Result.Type = InType;
		return Result;
	}

	uint64_t UtfValue::AsUnsigned() const
	{
		if (Type == UtfType::F32 || Type == UtfType::F64)
		{
			const double D = AsDouble();
			return D <= 0 ? 0 : uint64_t(D);
		}
		return Bits;
	}

	int64_t UtfValue::AsSigned() const
	{
		switch (Type)
		{
		case UtfType::S8: return int64_t(int8_t(uint8_t(Bits)));
		case UtfType::S16: return int64_t(int16_t(uint16_t(Bits)));
		case UtfType::S32: return int64_t(int32_t(uint32_t(Bits)));
		case UtfType::F32: case UtfType::F64: return int64_t(AsDouble());
		default: return int64_t(Bits);
		}
	}

	double UtfValue::AsDouble() const
	{
		if (Type == UtfType::F32)
		{
			const uint32_t Raw = uint32_t(Bits);
			float F;
			std::memcpy(&F, &Raw, 4);
			return F;
		}
		if (Type == UtfType::F64)
		{
			double D;
			std::memcpy(&D, &Bits, 8);
			return D;
		}
		switch (Type)
		{
		case UtfType::S8: case UtfType::S16: case UtfType::S32: case UtfType::S64: return double(AsSigned());
		default: return double(Bits);
		}
	}

	void UtfValue::SetUnsigned(uint64_t Value)
	{
		if (Type == UtfType::F32 || Type == UtfType::F64)
		{
			SetDouble(double(Value));
			return;
		}
		Bits = Value & TypeMask(Type);
	}

	void UtfValue::SetDouble(double Value)
	{
		if (Type == UtfType::F32)
		{
			const float F = float(Value);
			uint32_t Raw;
			std::memcpy(&Raw, &F, 4);
			Bits = Raw;
		}
		else if (Type == UtfType::F64)
		{
			std::memcpy(&Bits, &Value, 8);
		}
		else
		{
			Bits = uint64_t(int64_t(Value)) & TypeMask(Type);
		}
	}

	bool UtfValue::SameAs(const UtfValue& Other) const
	{
		if (Type != Other.Type)
		{
			return false;
		}
		switch (Type)
		{
		case UtfType::String:
			return Text == Other.Text;
		case UtfType::Guid:
			return std::memcmp(Guid, Other.Guid, 16) == 0;
		case UtfType::Data:
		{
			if (Data.IsEmpty() || Other.Data.IsEmpty())
			{
				return Data.IsEmpty() && Other.Data.IsEmpty();
			}
			if (Data.Awb || Other.Data.Awb || Data.Source || Other.Data.Source)
			{
				return false;
			}
			return Data.ToBytes() == Other.Data.ToBytes();
		}
		default:
			return (Bits & TypeMask(Type)) == (Other.Bits & TypeMask(Type));
		}
	}

	bool UtfTable::LooksLikeTable(const uint8_t* Data, size_t Size)
	{
		return Size >= 0x20 && std::memcmp(Data, UtfMagic, 4) == 0 && uint64_t(ReadBE32(Data + 4)) + 8 <= Size;
	}

	std::shared_ptr<UtfTable> UtfTable::Parse(const uint8_t* Data, size_t Size, const UtfParseOptions& Options)
	{
		if (!LooksLikeTable(Data, Size))
		{
			throw AcbError("Not a @UTF table");
		}
		const uint64_t Total = uint64_t(ReadBE32(Data + 4)) + 8;
		auto Table = ParseTable(Data, size_t(Total), Total, [&](uint64_t BodyOffset, uint64_t BlobSize)
		{
			const uint8_t* P = Data + 8 + BodyOffset;
			return MakeInlineData(std::vector<uint8_t>(P, P + BlobSize), Options);
		});
		Table->Original.assign(Data, Data + Total);
		return Table;
	}

	std::shared_ptr<UtfTable> UtfTable::ParseStream(const std::shared_ptr<ReadStream>& Stream, uint64_t Offset, const UtfParseOptions& Options)
	{
		uint8_t Prefix[0x20];
		if (Stream->Size() < Offset + sizeof(Prefix))
		{
			throw AcbError("File too small to be an ACB");
		}
		Stream->ReadAt(Offset, Prefix, sizeof(Prefix));
		if (std::memcmp(Prefix, UtfMagic, 4) != 0)
		{
			throw AcbError("Not a @UTF table (bad magic)");
		}
		const uint64_t Total = uint64_t(ReadBE32(Prefix + 4)) + 8;
		if (Offset + Total > Stream->Size())
		{
			throw AcbError("@UTF table is truncated");
		}
		const uint64_t HeadSize = 8 + uint64_t(ReadBE32(Prefix + 16));
		if (HeadSize > Total || HeadSize > (512ull << 20))
		{
			throw AcbError("Corrupt @UTF table header");
		}
		std::vector<uint8_t> Head = Stream->ReadVector(Offset, size_t(HeadSize));
		return ParseTable(Head.data(), Head.size(), Total, [&](uint64_t BodyOffset, uint64_t BlobSize)
		{
			const uint64_t Absolute = Offset + 8 + BodyOffset;
			if (BlobSize <= Options.InlineBlobLimit)
			{
				return MakeInlineData(Stream->ReadVector(Absolute, size_t(BlobSize)), Options);
			}
			UtfData Data;
			if (Options.ParseAwb)
			{
				if (auto Awb = Afs2Archive::Parse(Stream, Absolute, BlobSize))
				{
					Data.Awb = Awb;
					return Data;
				}
			}
			Data.Source = Stream;
			Data.SourceOffset = Absolute;
			Data.SourceSize = BlobSize;
			return Data;
		});
	}

	int UtfTable::ColumnIndex(const std::string& ColumnName) const
	{
		for (size_t Index = 0; Index < Columns.size(); ++Index)
		{
			if (Columns[Index].Name == ColumnName)
			{
				return int(Index);
			}
		}
		return -1;
	}

	size_t UtfTable::RequireColumn(const std::string& ColumnName) const
	{
		const int Index = ColumnIndex(ColumnName);
		if (Index < 0)
		{
			throw AcbError("Table '" + Name + "' has no column '" + ColumnName + "'");
		}
		return size_t(Index);
	}

	const UtfValue& UtfTable::Get(size_t Row, const std::string& ColumnName) const
	{
		if (Row >= Rows.size())
		{
			throw AcbError("Row " + std::to_string(Row) + " out of range in table '" + Name + "'");
		}
		return Rows[Row][RequireColumn(ColumnName)];
	}

	uint64_t UtfTable::GetUnsigned(size_t Row, const std::string& ColumnName) const
	{
		return Get(Row, ColumnName).AsUnsigned();
	}

	std::string UtfTable::GetString(size_t Row, const std::string& ColumnName) const
	{
		return Get(Row, ColumnName).Text;
	}

	std::vector<uint8_t> UtfTable::GetBytes(size_t Row, const std::string& ColumnName) const
	{
		return Get(Row, ColumnName).Data.ToBytes();
	}

	UtfData& UtfTable::GetData(size_t Row, const std::string& ColumnName)
	{
		const size_t Column = RequireColumn(ColumnName);
		if (Row >= Rows.size())
		{
			throw AcbError("Row out of range in table '" + Name + "'");
		}
		MarkDirty();
		return Rows[Row][Column].Data;
	}

	void UtfTable::SetValue(size_t Row, const std::string& ColumnName, const UtfValue& Value)
	{
		const size_t Column = RequireColumn(ColumnName);
		if (Row >= Rows.size())
		{
			throw AcbError("Row out of range in table '" + Name + "'");
		}
		UtfValue& Target = Rows[Row][Column];
		switch (Target.Type)
		{
		case UtfType::String:
			Target.Text = Value.Text;
			break;
		case UtfType::Data:
			Target.Data = Value.Data;
			break;
		case UtfType::Guid:
			std::memcpy(Target.Guid, Value.Guid, 16);
			break;
		case UtfType::F32:
		case UtfType::F64:
			Target.SetDouble(Value.AsDouble());
			break;
		default:
			Target.SetUnsigned(Value.Type == UtfType::F32 || Value.Type == UtfType::F64 ? uint64_t(Value.AsSigned()) : Value.Bits);
			break;
		}
		MarkDirty();
	}

	void UtfTable::SetUnsigned(size_t Row, const std::string& ColumnName, uint64_t Value)
	{
		SetValue(Row, ColumnName, UtfValue::Integer(UtfType::U64, Value));
	}

	void UtfTable::SetString(size_t Row, const std::string& ColumnName, const std::string& Value)
	{
		SetValue(Row, ColumnName, UtfValue::String(Value));
	}

	void UtfTable::SetBytes(size_t Row, const std::string& ColumnName, const std::vector<uint8_t>& Value)
	{
		SetValue(Row, ColumnName, UtfValue::Blob(Value));
	}

	std::vector<UtfValue> UtfTable::MakeDefaultRow() const
	{
		std::vector<UtfValue> Row;
		Row.reserve(Columns.size());
		for (const UtfColumn& Column : Columns)
		{
			Row.push_back(UtfValue::Default(Column.Type));
		}
		return Row;
	}

	size_t UtfTable::AddRow(const std::map<std::string, UtfValue>& Overrides)
	{
		Rows.push_back(MakeDefaultRow());
		const size_t Index = Rows.size() - 1;
		for (const auto& Pair : Overrides)
		{
			if (HasColumn(Pair.first))
			{
				SetValue(Index, Pair.first, Pair.second);
			}
		}
		MarkDirty();
		return Index;
	}

	void UtfTable::InsertRow(size_t Index, std::vector<UtfValue> Row)
	{
		if (Index > Rows.size() || Row.size() != Columns.size())
		{
			throw AcbError("Bad row insert in table '" + Name + "'");
		}
		Rows.insert(Rows.begin() + std::ptrdiff_t(Index), std::move(Row));
		MarkDirty();
	}

	void UtfTable::RemoveRow(size_t Index)
	{
		if (Index >= Rows.size())
		{
			throw AcbError("Row out of range in table '" + Name + "'");
		}
		Rows.erase(Rows.begin() + std::ptrdiff_t(Index));
		MarkDirty();
	}

	bool UtfTable::IsDirty() const
	{
		if (bDirty)
		{
			return true;
		}
		for (const auto& Row : Rows)
		{
			for (const UtfValue& Value : Row)
			{
				if (Value.Type == UtfType::Data && Value.Data.Table && Value.Data.Table->IsDirty())
				{
					return true;
				}
			}
		}
		return false;
	}

	std::vector<uint8_t> UtfTable::Serialize(const UtfWriteOptions& Options) const
	{
		MemoryWriteStream Out;
		Write(Out, Options);
		return std::move(Out.Data());
	}

	void UtfTable::Write(WriteStream& Out, const UtfWriteOptions& Options) const
	{
		Encode(&Out, nullptr, Options);
	}

	uint64_t UtfTable::ComputeSize(const UtfWriteOptions& Options) const
	{
		uint64_t Size = 0;
		Encode(nullptr, &Size, Options);
		return Size;
	}

	void UtfTable::Encode(WriteStream* Out, uint64_t* OutSize, const UtfWriteOptions& Options) const
	{
		std::vector<UtfStorage> Storage(Columns.size());
		std::vector<const UtfValue*> ConstantValue(Columns.size(), nullptr);
		for (size_t Column = 0; Column < Columns.size(); ++Column)
		{
			const UtfColumn& Def = Columns[Column];
			UtfStorage Chosen = UtfStorage::PerRow;
			if (!Options.ForcePerRow && Def.Storage != UtfStorage::PerRow)
			{
				const UtfValue& Reference = Rows.empty() ? Def.Constant : Rows[0][Column];
				bool bAllSame = true;
				for (const auto& Row : Rows)
				{
					if (!Row[Column].SameAs(Reference))
					{
						bAllSame = false;
						break;
					}
				}
				if (bAllSame)
				{
					if (Def.Storage == UtfStorage::Constant)
					{
						Chosen = UtfStorage::Constant;
						ConstantValue[Column] = &Reference;
					}
					else if (Reference.SameAs(UtfValue::Default(Def.Type)))
					{
						Chosen = UtfStorage::None;
					}
				}
			}
			Storage[Column] = Chosen;
		}

		std::vector<uint8_t> Pool;
		auto AddString = [&Pool](const std::string& Text) -> uint32_t
		{
			const uint32_t Offset = uint32_t(Pool.size());
			Pool.insert(Pool.end(), Text.begin(), Text.end());
			Pool.push_back(0);
			return Offset;
		};
		const uint32_t TableNameOffset = AddString(Name);

		struct BlobSlot
		{
			const UtfData* Data;
			uint64_t Offset;
			uint64_t Size;
		};
		std::vector<BlobSlot> Blobs;
		uint64_t DataCursor = 0;
		auto PlaceBlob = [&](const UtfData& Data, std::vector<uint8_t>& Dest)
		{
			const uint64_t Size = Data.IsEmpty() ? 0 : UtfDataWriteSize(Data);
			uint64_t Offset = 0;
			if (Size > 0)
			{
				Offset = Blobs.empty() ? 0
					: Options.DataAlignment > 1 ? AlignUp(DataCursor + 1, Options.DataAlignment) : DataCursor;
				DataCursor = Offset + Size;
				Blobs.push_back({ &Data, Offset, Size });
			}
			if (Offset > 0xFFFFFFFFull || Size > 0xFFFFFFFFull)
			{
				throw AcbError("Table '" + Name + "' is too large (VLDATA offsets are 32-bit)");
			}
			PutBE32(Dest, uint32_t(Offset));
			PutBE32(Dest, uint32_t(Size));
		};
		auto PutTyped = [&](const UtfValue& Value, UtfType Type, std::vector<uint8_t>& Dest)
		{
			switch (Type)
			{
			case UtfType::String:
				PutBE32(Dest, AddString(Value.Text));
				break;
			case UtfType::Data:
				PlaceBlob(Value.Data, Dest);
				break;
			case UtfType::Guid:
				Dest.insert(Dest.end(), Value.Guid, Value.Guid + 16);
				break;
			default:
				PutBEBytes(Dest, Value.Bits, UtfTypeSize(Type));
				break;
			}
		};

		std::vector<uint8_t> Schema;
		std::vector<uint32_t> NameOffsets;
		size_t RowLength = 0;
		for (size_t Column = 0; Column < Columns.size(); ++Column)
		{
			const UtfColumn& Def = Columns[Column];
			Schema.push_back(uint8_t(uint8_t(Storage[Column]) | uint8_t(Def.Type)));
			PutBE32(Schema, AddString(Def.Name));
			if (Storage[Column] == UtfStorage::Constant)
			{
				PutTyped(*ConstantValue[Column], Def.Type, Schema);
			}
			else if (Storage[Column] == UtfStorage::PerRow)
			{
				RowLength += UtfTypeSize(Def.Type);
			}
		}
		if (RowLength > 0xFFFF)
		{
			throw AcbError("Row too wide in table '" + Name + "'");
		}

		std::vector<uint8_t> RowBytes;
		RowBytes.reserve(RowLength * Rows.size());
		for (const auto& Row : Rows)
		{
			for (size_t Column = 0; Column < Columns.size(); ++Column)
			{
				if (Storage[Column] == UtfStorage::PerRow)
				{
					PutTyped(Row[Column], Columns[Column].Type, RowBytes);
				}
			}
		}

		const uint64_t RowsOffset = 0x18 + Schema.size();
		if (RowsOffset > 0xFFFF)
		{
			throw AcbError("Schema too large in table '" + Name + "'");
		}
		const uint64_t StringsOffset = RowsOffset + RowBytes.size();
		uint64_t DataOffset = StringsOffset + Pool.size();
		if (Options.DataAlignment > 1 && !Blobs.empty())
		{
			DataOffset = AlignUp(8 + DataOffset, Options.DataAlignment) - 8;
		}
		const uint64_t PoolPadding = DataOffset - StringsOffset - Pool.size();
		const uint64_t BodyEnd = DataOffset + DataCursor;
		const uint64_t Total = Options.DataAlignment > 1 ? AlignUp(8 + BodyEnd + 1, Options.EndAlignment) : AlignUp(8 + BodyEnd, Options.EndAlignment);
		if (Total - 8 > 0xFFFFFFFFull)
		{
			throw AcbError("Table '" + Name + "' is larger than 4 GiB");
		}
		if (OutSize)
		{
			*OutSize = Total;
		}
		if (!Out)
		{
			return;
		}

		std::vector<uint8_t> Header;
		Header.insert(Header.end(), UtfMagic, UtfMagic + 4);
		PutBE32(Header, uint32_t(Total - 8));
		Header.push_back(Unknown0);
		Header.push_back(Encoding);
		PutBE16(Header, uint16_t(RowsOffset));
		PutBE32(Header, uint32_t(StringsOffset));
		PutBE32(Header, uint32_t(DataOffset));
		PutBE32(Header, TableNameOffset);
		PutBE16(Header, uint16_t(Columns.size()));
		PutBE16(Header, uint16_t(RowLength));
		PutBE32(Header, uint32_t(Rows.size()));
		Out->Write(Header);
		Out->Write(Schema);
		Out->Write(RowBytes);
		Out->Write(Pool);
		Out->WriteZeros(PoolPadding);

		uint64_t Written = 0;
		for (const BlobSlot& Blob : Blobs)
		{
			Out->WriteZeros(Blob.Offset - Written);
			const uint64_t Before = Out->Tell();
			UtfDataWrite(*Blob.Data, *Out, Options.ProgressSink);
			if (Out->Tell() - Before != Blob.Size)
			{
				throw AcbError("Internal error: blob size changed while writing table '" + Name + "'");
			}
			Written = Blob.Offset + Blob.Size;
		}
		Out->WriteZeros(Total - 8 - DataOffset - Written);
	}
}
