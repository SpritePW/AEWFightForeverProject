#pragma once

#include "AcbIO.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace AcbCore
{
	class Afs2Archive;
	class UtfTable;

	enum class UtfType : uint8_t
	{
		U8 = 0x0, S8 = 0x1, U16 = 0x2, S16 = 0x3, U32 = 0x4, S32 = 0x5, U64 = 0x6, S64 = 0x7,
		F32 = 0x8, F64 = 0x9, String = 0xA, Data = 0xB, Guid = 0xC
	};

	enum class UtfStorage : uint8_t
	{
		None = 0x10,
		Constant = 0x30,
		PerRow = 0x50
	};

	const char* UtfTypeName(UtfType Type);
	size_t UtfTypeSize(UtfType Type);
	bool UtfTypeIsInteger(UtfType Type);

	struct UtfData
	{
		std::vector<uint8_t> Bytes;
		std::shared_ptr<UtfTable> Table;
		std::shared_ptr<Afs2Archive> Awb;
		std::shared_ptr<ReadStream> Source;
		uint64_t SourceOffset = 0;
		uint64_t SourceSize = 0;

		bool IsEmpty() const;
		std::vector<uint8_t> ToBytes() const;
	};

	struct UtfValue
	{
		UtfType Type = UtfType::U8;
		uint64_t Bits = 0;
		std::string Text;
		UtfData Data;
		uint8_t Guid[16] = {};

		static UtfValue Integer(UtfType InType, uint64_t Value);
		static UtfValue Float(double Value);
		static UtfValue Double(double Value);
		static UtfValue String(const std::string& Value);
		static UtfValue Blob(const std::vector<uint8_t>& Bytes);
		static UtfValue Default(UtfType InType);

		uint64_t AsUnsigned() const;
		int64_t AsSigned() const;
		double AsDouble() const;
		void SetUnsigned(uint64_t Value);
		void SetDouble(double Value);

		bool SameAs(const UtfValue& Other) const;
	};

	struct UtfColumn
	{
		std::string Name;
		UtfType Type = UtfType::U8;
		UtfStorage Storage = UtfStorage::PerRow;
		UtfValue Constant;
	};

	struct UtfParseOptions
	{
		uint64_t InlineBlobLimit = 64ull << 20;
		bool ParseNestedTables = true;
		bool ParseAwb = true;
	};

	struct UtfWriteOptions
	{
		uint32_t DataAlignment = 1;
		uint32_t EndAlignment = 4;
		bool ForcePerRow = false;
		Progress* ProgressSink = nullptr;
	};

	class UtfTable
	{
	public:
		std::string Name;
		uint8_t Unknown0 = 0;
		uint8_t Encoding = 1;
		std::vector<UtfColumn> Columns;
		std::vector<std::vector<UtfValue>> Rows;

		static std::shared_ptr<UtfTable> Parse(const uint8_t* Data, size_t Size, const UtfParseOptions& Options = UtfParseOptions());
		static std::shared_ptr<UtfTable> ParseStream(const std::shared_ptr<ReadStream>& Stream, uint64_t Offset, const UtfParseOptions& Options = UtfParseOptions());
		static bool LooksLikeTable(const uint8_t* Data, size_t Size);

		int ColumnIndex(const std::string& ColumnName) const;
		bool HasColumn(const std::string& ColumnName) const { return ColumnIndex(ColumnName) >= 0; }
		size_t RowCount() const { return Rows.size(); }

		const UtfValue& Get(size_t Row, const std::string& ColumnName) const;
		uint64_t GetUnsigned(size_t Row, const std::string& ColumnName) const;
		std::string GetString(size_t Row, const std::string& ColumnName) const;
		std::vector<uint8_t> GetBytes(size_t Row, const std::string& ColumnName) const;
		UtfData& GetData(size_t Row, const std::string& ColumnName);

		void SetUnsigned(size_t Row, const std::string& ColumnName, uint64_t Value);
		void SetString(size_t Row, const std::string& ColumnName, const std::string& Value);
		void SetBytes(size_t Row, const std::string& ColumnName, const std::vector<uint8_t>& Value);
		void SetValue(size_t Row, const std::string& ColumnName, const UtfValue& Value);

		size_t AddRow(const std::map<std::string, UtfValue>& Overrides);
		void InsertRow(size_t Index, std::vector<UtfValue> Row);
		void RemoveRow(size_t Index);
		std::vector<UtfValue> MakeDefaultRow() const;

		void MarkDirty() { bDirty = true; }
		bool IsDirty() const;
		const std::vector<uint8_t>& OriginalBytes() const { return Original; }

		std::vector<uint8_t> Serialize(const UtfWriteOptions& Options) const;
		void Write(WriteStream& Out, const UtfWriteOptions& Options) const;
		uint64_t ComputeSize(const UtfWriteOptions& Options) const;

	private:
		bool bDirty = false;
		std::vector<uint8_t> Original;

		size_t RequireColumn(const std::string& ColumnName) const;
		void Encode(WriteStream* Out, uint64_t* OutSize, const UtfWriteOptions& Options) const;
	};

	uint64_t UtfDataWriteSize(const UtfData& Data);
	void UtfDataWrite(const UtfData& Data, WriteStream& Out, Progress* ProgressSink);
}
