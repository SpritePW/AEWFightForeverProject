#include "UsmHeaderReader.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"

namespace UsmHeaderReaderPrivate {
	uint16 BE16(const uint8* P) { return (uint16(P[0]) << 8) | P[1]; }
	uint32 BE32(const uint8* P) { return (uint32(P[0]) << 24) | (uint32(P[1]) << 16) | (uint32(P[2]) << 8) | P[3]; }
	uint64 BE64(const uint8* P) { return (uint64(BE32(P)) << 32) | BE32(P + 4); }

	FString ReadCString(const uint8* Base, int32 Size, uint32 Offset) {
		if (Offset >= (uint32)Size) {
			return FString();
		}
		const uint8* Start = Base + Offset;
		int32 Len = 0;
		while (Offset + Len < (uint32)Size && Start[Len] != 0) {
			++Len;
		}
		FUTF8ToTCHAR Converted((const ANSICHAR*)Start, Len);
		return FString(Converted.Length(), Converted.Get());
	}

	int32 ReadValue(uint8 Type, const uint8* P, const uint8* Strings, int32 StringsSize, FUsmValue& Out) {
		switch (Type) {
		case 0x0: Out.bIsNumber = true; Out.Int = P[0]; return 1;
		case 0x1: Out.bIsNumber = true; Out.Int = (int8)P[0]; return 1;
		case 0x2: Out.bIsNumber = true; Out.Int = BE16(P); return 2;
		case 0x3: Out.bIsNumber = true; Out.Int = (int16)BE16(P); return 2;
		case 0x4: Out.bIsNumber = true; Out.Int = BE32(P); return 4;
		case 0x5: Out.bIsNumber = true; Out.Int = (int32)BE32(P); return 4;
		case 0x6: Out.bIsNumber = true; Out.Int = (int64)BE64(P); return 8;
		case 0x7: Out.bIsNumber = true; Out.Int = (int64)BE64(P); return 8;
		case 0x8: {
			uint32 Bits = BE32(P);
			float F;
			FMemory::Memcpy(&F, &Bits, 4);
			Out.bIsNumber = true; Out.Float = F; Out.Int = (int64)F;
			return 4;
		}
		case 0xA: Out.String = ReadCString(Strings, StringsSize, BE32(P)); return 4;
		case 0xB: return 8;
		default: return 0;
		}
	}

	bool ParseUtfTable(const uint8* Data, int32 Size, FUsmTable& Out) {
		if (Size < 32 || FMemory::Memcmp(Data, "@UTF", 4) != 0) {
			return false;
		}
		const uint32 TableSize = BE32(Data + 4);
		const uint8* Base = Data + 8;
		const int32 BaseSize = FMath::Min<int32>((int32)TableSize, Size - 8);
		if (BaseSize < 24) {
			return false;
		}
		const uint16 RowsOffset = BE16(Base + 2);
		const uint32 StringsOffset = BE32(Base + 4);
		const uint32 NameOffset = BE32(Base + 12);
		const uint16 NumColumns = BE16(Base + 16);
		const uint16 RowWidth = BE16(Base + 18);
		const uint32 NumRows = BE32(Base + 20);
		if (StringsOffset > (uint32)BaseSize || RowsOffset > BaseSize) {
			return false;
		}
		const uint8* Strings = Base + StringsOffset;
		const int32 StringsSize = BaseSize - StringsOffset;
		Out.Name = ReadCString(Strings, StringsSize, NameOffset);

		struct FColumn { uint8 Flags; FString Name; FUsmValue Constant; };
		TArray<FColumn> Columns;
		const uint8* P = Base + 24;
		for (uint16 C = 0; C < NumColumns; ++C) {
			if (P + 5 > Base + BaseSize) {
				return false;
			}
			FColumn Col;
			Col.Flags = P[0];
			Col.Name = ReadCString(Strings, StringsSize, BE32(P + 1));
			P += 5;
			const uint8 Storage = Col.Flags & 0xF0;
			if (Storage == 0x30 || Storage == 0x70) {
				const int32 Used = ReadValue(Col.Flags & 0x0F, P, Strings, StringsSize, Col.Constant);
				if (Used == 0) {
					return false;
				}
				P += Used;
			}
			Columns.Add(MoveTemp(Col));
		}

		for (uint32 R = 0; R < NumRows; ++R) {
			const uint8* RowP = Base + RowsOffset + R * RowWidth;
			FUsmRow Row;
			for (const FColumn& Col : Columns) {
				const uint8 Storage = Col.Flags & 0xF0;
				if (Storage == 0x50) {
					if (RowP >= Base + BaseSize) {
						return false;
					}
					FUsmValue V;
					const int32 Used = ReadValue(Col.Flags & 0x0F, RowP, Strings, StringsSize, V);
					if (Used == 0) {
						return false;
					}
					RowP += Used;
					Row.Add(Col.Name, V);
				} else if (Storage == 0x30 || Storage == 0x70) {
					Row.Add(Col.Name, Col.Constant);
				} else {
					FUsmValue Zero;
					Zero.bIsNumber = true;
					Row.Add(Col.Name, Zero);
				}
			}
			Out.Rows.Add(MoveTemp(Row));
		}
		return true;
	}
}

using namespace UsmHeaderReaderPrivate;

int64 FUsmInfo::GetInt(const FUsmRow& Row, const TCHAR* Key, int64 Default) {
	const FUsmValue* V = Row.Find(Key);
	return (V && V->bIsNumber) ? V->Int : Default;
}

bool ReadUsmHeader(const FString& Filename, FUsmInfo& OutInfo, FString& OutError) {
	TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*Filename));
	if (!Ar) {
		OutError = FString::Printf(TEXT("Can't open %s"), *Filename);
		return false;
	}
	const int64 FileSize = Ar->TotalSize();
	OutInfo.FileSize = FileSize;
	TSet<int32> SubtitleChannels;

	int64 Pos = 0;
	bool bSawCrid = false;
	while (Pos + 16 <= FileSize) {
		uint8 Head[16];
		Ar->Seek(Pos);
		Ar->Serialize(Head, 16);
		const uint32 ChunkSize = BE32(Head + 4);
		const int64 ChunkTotal = 8 + (int64)ChunkSize;
		if (ChunkSize < 8 || Pos + ChunkTotal > FileSize) {
			break;
		}
		OutInfo.LargestChunkSize = FMath::Max(OutInfo.LargestChunkSize, ChunkTotal);

		const uint8 PayloadOffset = Head[9];
		const uint16 Padding = BE16(Head + 10);
		const uint8 Channel = Head[12];
		const uint8 DataType = Head[15];
		const int64 PayloadSize = (int64)ChunkSize - PayloadOffset - Padding;

		const bool bCrid = FMemory::Memcmp(Head, "CRID", 4) == 0;
		const bool bSfv = FMemory::Memcmp(Head, "@SFV", 4) == 0;
		const bool bAlp = FMemory::Memcmp(Head, "@ALP", 4) == 0;
		const bool bSfa = FMemory::Memcmp(Head, "@SFA", 4) == 0;
		const bool bSbt = FMemory::Memcmp(Head, "@SBT", 4) == 0;
		bSawCrid |= bCrid;
		if (bSbt) {
			SubtitleChannels.Add(Channel);
		}

		if ((bCrid || bSfv || bAlp || bSfa) && DataType == 1 && PayloadSize > 0 && PayloadSize < 16 * 1024 * 1024) {
			TArray<uint8> Payload;
			Payload.SetNumUninitialized((int32)PayloadSize);
			Ar->Seek(Pos + 8 + PayloadOffset);
			Ar->Serialize(Payload.GetData(), Payload.Num());
			FUsmTable Table;
			if (ParseUtfTable(Payload.GetData(), Payload.Num(), Table)) {
				if (bCrid) {
					OutInfo.DirectoryTables.Add(MoveTemp(Table));
				} else {
					FUsmStreamHeader Header;
					Header.Channel = Channel;
					Header.Table = MoveTemp(Table);
					(bSfv ? OutInfo.VideoHeaders : bAlp ? OutInfo.AlphaHeaders : OutInfo.AudioHeaders).Add(MoveTemp(Header));
				}
			}
		}
		Pos += ChunkTotal;
	}
	OutInfo.NumSubtitleChannels = SubtitleChannels.Num();

	if (!bSawCrid || OutInfo.VideoHeaders.Num() == 0) {
		OutError = FString::Printf(TEXT("%s doesn't look like a CRI .usm movie (no CRID/@SFV header found)"), *Filename);
		return false;
	}
	return true;
}
