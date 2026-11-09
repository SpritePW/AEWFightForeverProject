#include "AcbIO.h"

#include <algorithm>
#include <cstring>

#if defined(ACBCORE_WITH_UE)
#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#else
#include <cstdio>
#endif

namespace AcbCore
{
	void WriteStream::WriteZeros(uint64_t Count)
	{
		static const uint8_t Zeros[4096] = {};
		while (Count > 0)
		{
			const size_t Chunk = size_t(std::min<uint64_t>(Count, sizeof(Zeros)));
			Write(Zeros, Chunk);
			Count -= Chunk;
		}
	}

	void WriteStream::CopyFrom(ReadStream& Source, uint64_t Offset, uint64_t Count)
	{
		std::vector<uint8_t> Buffer(size_t(std::min<uint64_t>(Count, 4u << 20)));
		while (Count > 0)
		{
			const size_t Chunk = size_t(std::min<uint64_t>(Count, Buffer.size()));
			Source.ReadAt(Offset, Buffer.data(), Chunk);
			Write(Buffer.data(), Chunk);
			Offset += Chunk;
			Count -= Chunk;
		}
	}

	void MemoryReadStream::ReadAt(uint64_t Offset, void* Dest, size_t Count)
	{
		if (Offset > Bytes.size() || Count > Bytes.size() - Offset)
		{
			throw AcbError("Read past the end of a memory buffer");
		}
		if (Count > 0)
		{
			std::memcpy(Dest, Bytes.data() + Offset, Count);
		}
	}

	void MemoryWriteStream::Write(const void* Data, size_t Count)
	{
		const uint8_t* P = static_cast<const uint8_t*>(Data);
		Bytes.insert(Bytes.end(), P, P + Count);
	}

	void Progress::Advance(uint64_t Bytes)
	{
		Done += Bytes;
		if (Callback && !Callback(User, Done, Total))
		{
			throw AcbError("Cancelled");
		}
	}

#if defined(ACBCORE_WITH_UE)

	namespace
	{
		FString ToFString(const std::string& Utf8)
		{
			return FString(UTF8_TO_TCHAR(Utf8.c_str()));
		}

		class UeFileReadStream final : public ReadStream
		{
		public:
			UeFileReadStream(IFileHandle* InHandle, const std::string& InPath)
				: Handle(InHandle), Path(InPath), CachedSize(uint64_t(InHandle->Size()))
			{
			}
			uint64_t Size() const override { return CachedSize; }
			void ReadAt(uint64_t Offset, void* Dest, size_t Count) override
			{
				if (Offset > CachedSize || Count > CachedSize - Offset)
				{
					throw AcbError("Read past the end of " + Path);
				}
				if (Count == 0)
				{
					return;
				}
				if (!Handle->Seek(int64(Offset)) || !Handle->Read(static_cast<uint8*>(Dest), int64(Count)))
				{
					throw AcbError("Failed to read " + Path);
				}
			}

		private:
			TUniquePtr<IFileHandle> Handle;
			std::string Path;
			uint64_t CachedSize;
		};

		class UeFileWriteStream final : public WriteStream
		{
		public:
			UeFileWriteStream(IFileHandle* InHandle, const std::string& InPath) : Handle(InHandle), Path(InPath) {}
			void Write(const void* Data, size_t Count) override
			{
				if (Count == 0)
				{
					return;
				}
				if (!Handle->Write(static_cast<const uint8*>(Data), int64(Count)))
				{
					throw AcbError("Failed to write " + Path + " (disk full?)");
				}
				Position += Count;
			}
			uint64_t Tell() const override { return Position; }

			using WriteStream::Write;

		private:
			TUniquePtr<IFileHandle> Handle;
			std::string Path;
			uint64_t Position = 0;
		};
	}

	std::shared_ptr<ReadStream> OpenFileRead(const std::string& Utf8Path)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* Handle = PlatformFile.OpenRead(*ToFString(Utf8Path));
		if (!Handle)
		{
			throw AcbError("Cannot open " + Utf8Path);
		}
		return std::make_shared<UeFileReadStream>(Handle, Utf8Path);
	}

	std::unique_ptr<WriteStream> OpenFileWrite(const std::string& Utf8Path)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString Path = ToFString(Utf8Path);
		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(Path));
		IFileHandle* Handle = PlatformFile.OpenWrite(*Path, false, false);
		if (!Handle)
		{
			throw AcbError("Cannot create " + Utf8Path);
		}
		return std::make_unique<UeFileWriteStream>(Handle, Utf8Path);
	}

	bool FileExists(const std::string& Utf8Path)
	{
		return FPlatformFileManager::Get().GetPlatformFile().FileExists(*ToFString(Utf8Path));
	}

	bool DeleteFileIfExists(const std::string& Utf8Path)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString Path = ToFString(Utf8Path);
		return !PlatformFile.FileExists(*Path) || PlatformFile.DeleteFile(*Path);
	}

	bool MoveFileReplace(const std::string& FromUtf8, const std::string& ToUtf8)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString To = ToFString(ToUtf8);
		if (PlatformFile.FileExists(*To) && !PlatformFile.DeleteFile(*To))
		{
			return false;
		}
		return PlatformFile.MoveFile(*To, *ToFString(FromUtf8));
	}

#else

	namespace
	{
		int SeekFile(FILE* File, uint64_t Offset)
		{
#if defined(_MSC_VER)
			return _fseeki64(File, int64_t(Offset), SEEK_SET);
#else
			return fseeko(File, off_t(Offset), SEEK_SET);
#endif
		}

		class StdFileReadStream final : public ReadStream
		{
		public:
			StdFileReadStream(FILE* InFile, const std::string& InPath) : File(InFile), Path(InPath)
			{
#if defined(_MSC_VER)
				_fseeki64(File, 0, SEEK_END);
				CachedSize = uint64_t(_ftelli64(File));
#else
				fseeko(File, 0, SEEK_END);
				CachedSize = uint64_t(ftello(File));
#endif
			}
			~StdFileReadStream() override { fclose(File); }
			uint64_t Size() const override { return CachedSize; }
			void ReadAt(uint64_t Offset, void* Dest, size_t Count) override
			{
				if (Offset > CachedSize || Count > CachedSize - Offset)
				{
					throw AcbError("Read past the end of " + Path);
				}
				if (Count == 0)
				{
					return;
				}
				if (SeekFile(File, Offset) != 0 || fread(Dest, 1, Count, File) != Count)
				{
					throw AcbError("Failed to read " + Path);
				}
			}

		private:
			FILE* File;
			std::string Path;
			uint64_t CachedSize = 0;
		};

		class StdFileWriteStream final : public WriteStream
		{
		public:
			StdFileWriteStream(FILE* InFile, const std::string& InPath) : File(InFile), Path(InPath) {}
			~StdFileWriteStream() override { fclose(File); }
			void Write(const void* Data, size_t Count) override
			{
				if (Count && fwrite(Data, 1, Count, File) != Count)
				{
					throw AcbError("Failed to write " + Path);
				}
				Position += Count;
			}
			uint64_t Tell() const override { return Position; }

			using WriteStream::Write;

		private:
			FILE* File;
			std::string Path;
			uint64_t Position = 0;
		};
	}

	std::shared_ptr<ReadStream> OpenFileRead(const std::string& Utf8Path)
	{
		FILE* File = fopen(Utf8Path.c_str(), "rb");
		if (!File)
		{
			throw AcbError("Cannot open " + Utf8Path);
		}
		return std::make_shared<StdFileReadStream>(File, Utf8Path);
	}

	std::unique_ptr<WriteStream> OpenFileWrite(const std::string& Utf8Path)
	{
		FILE* File = fopen(Utf8Path.c_str(), "wb");
		if (!File)
		{
			throw AcbError("Cannot create " + Utf8Path);
		}
		return std::make_unique<StdFileWriteStream>(File, Utf8Path);
	}

	bool FileExists(const std::string& Utf8Path)
	{
		FILE* File = fopen(Utf8Path.c_str(), "rb");
		if (File)
		{
			fclose(File);
			return true;
		}
		return false;
	}

	bool DeleteFileIfExists(const std::string& Utf8Path)
	{
		return !FileExists(Utf8Path) || std::remove(Utf8Path.c_str()) == 0;
	}

	bool MoveFileReplace(const std::string& FromUtf8, const std::string& ToUtf8)
	{
		DeleteFileIfExists(ToUtf8);
		return std::rename(FromUtf8.c_str(), ToUtf8.c_str()) == 0;
	}

#endif
}
