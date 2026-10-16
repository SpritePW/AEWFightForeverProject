#pragma once
#include "CoreMinimal.h"

struct FUsmValue {
	bool bIsNumber = false;
	int64 Int = 0;
	double Float = 0.0;
	FString String;
};

typedef TMap<FString, FUsmValue> FUsmRow;

struct FUsmTable {
	FString Name;
	TArray<FUsmRow> Rows;
};

struct FUsmStreamHeader {
	int32 Channel = 0;
	FUsmTable Table;
};

struct FUsmInfo {
	TArray<FUsmTable> DirectoryTables;
	TArray<FUsmStreamHeader> VideoHeaders;
	TArray<FUsmStreamHeader> AlphaHeaders;
	TArray<FUsmStreamHeader> AudioHeaders;
	int32 NumSubtitleChannels = 0;
	int64 LargestChunkSize = 0;
	int64 FileSize = 0;

	static int64 GetInt(const FUsmRow& Row, const TCHAR* Key, int64 Default = 0);
};

bool ReadUsmHeader(const FString& Filename, FUsmInfo& OutInfo, FString& OutError);
