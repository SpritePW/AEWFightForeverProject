#include "AcbEditorSession.h"
#include "AcbEditorSettings.h"
#include "AcbDocument.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "AEWAcbEditor"

DEFINE_LOG_CATEGORY(LogAEWAcbEditor);

namespace AcbEditorSessionLocal
{
	struct FSaveProgress
	{
		FScopedSlowTask* Task = nullptr;
		float Reported = 0.f;
	};

	bool OnSaveProgress(void* User, uint64_t Done, uint64_t Total)
	{
		FSaveProgress* Progress = static_cast<FSaveProgress*>(User);
		if (Total > 0)
		{
			const float Target = 100.f * float(double(Done) / double(Total));
			if (Target > Progress->Reported)
			{
				Progress->Task->EnterProgressFrame(Target - Progress->Reported);
				Progress->Reported = Target;
			}
		}
		return !Progress->Task->ShouldCancel();
	}

	FString StorageName(uint8 Streaming)
	{
		switch (Streaming)
		{
		case 0: return TEXT("Embedded");
		case 1: return TEXT("Stream");
		default: return TEXT("Prefetch+stream");
		}
	}

	FString TypeName(AcbCore::CueReferenceType Type)
	{
		switch (Type)
		{
		case AcbCore::CueReferenceType::Waveform: return TEXT("Waveform");
		case AcbCore::CueReferenceType::Synth: return TEXT("Synth");
		case AcbCore::CueReferenceType::Sequence: return TEXT("Sequence");
		default: return FString::Printf(TEXT("Type %d"), int32(Type));
		}
	}
}

FAcbEditorSession::FAcbEditorSession() = default;
FAcbEditorSession::~FAcbEditorSession() = default;

std::string FAcbEditorSession::ToUtf8(const FString& Text)
{
	FTCHARToUTF8 Converted(*Text);
	return std::string(Converted.Get(), size_t(Converted.Length()));
}

FString FAcbEditorSession::FromUtf8(const std::string& Text)
{
	return FString(UTF8_TO_TCHAR(Text.c_str()));
}

FString FAcbEditorSession::FindSiblingAwb(const FString& AcbPath)
{
	const FString Candidate = FPaths::ChangeExtension(AcbPath, TEXT("awb"));
	return FPaths::FileExists(Candidate) ? Candidate : FString();
}

bool FAcbEditorSession::IsOpen() const
{
	return Document != nullptr;
}

AcbCore::AcbDocument& FAcbEditorSession::Doc() const
{
	check(Document);
	return *Document;
}

bool FAcbEditorSession::Open(const FString& AcbPath, const FString& AwbPath, FString& OutError)
{
	try
	{
		std::unique_ptr<AcbCore::AcbDocument> NewDocument = AcbCore::AcbDocument::Load(ToUtf8(AcbPath), ToUtf8(AwbPath));
		Document = std::move(NewDocument);
		LoadedAcb = AcbPath;
		LoadedAwb = AwbPath;
		return true;
	}
	catch (const std::exception& Error)
	{
		OutError = FromUtf8(Error.what());
		return false;
	}
}

void FAcbEditorSession::Close()
{
	Document.reset();
	LoadedAcb.Reset();
	LoadedAwb.Reset();
}

bool FAcbEditorSession::Edit(TFunctionRef<void(AcbCore::AcbDocument&)> Operation, FString& OutError)
{
	if (!Document)
	{
		OutError = TEXT("No ACB is open.");
		return false;
	}
	try
	{
		Operation(*Document);
		return true;
	}
	catch (const std::exception& Error)
	{
		OutError = FromUtf8(Error.what());
		return false;
	}
}

FString FAcbEditorSession::GetCueSheetName() const
{
	if (!Document)
	{
		return FString();
	}
	const FString Name = FromUtf8(Document->CueSheetName());
	return Name.IsEmpty() ? FPaths::GetBaseFilename(LoadedAcb) : Name;
}

TArray<FString> FAcbEditorSession::GetCategories() const
{
	TArray<FString> Result;
	if (Document)
	{
		for (const std::string& Label : Document->CategoryLabels())
		{
			Result.Add(FromUtf8(Label));
		}
	}
	return Result;
}

int32 FAcbEditorSession::FindCue(const FString& Name) const
{
	if (!Document)
	{
		return INDEX_NONE;
	}
	const std::optional<size_t> Found = Document->FindCueByName(ToUtf8(Name));
	return Found ? int32(*Found) : INDEX_NONE;
}

TArray<FString> FAcbEditorSession::DescribeSlots(int32 CueIndex) const
{
	TArray<FString> Lines;
	if (!Document || CueIndex < 0 || size_t(CueIndex) >= Document->CueCount())
	{
		return Lines;
	}
	const AcbCore::CueChain Chain = Document->ResolveChain(size_t(CueIndex));
	for (size_t Slot = 0; Slot < Chain.WaveformIndices.size(); ++Slot)
	{
		const size_t Index = Chain.WaveformIndices[Slot];
		if (Index >= Document->WaveformCount())
		{
			continue;
		}
		const AcbCore::WaveformInfo Wave = Document->Waveform(Index);
		const double Seconds = Wave.SamplingRate ? double(Wave.NumSamples) / double(Wave.SamplingRate) : 0.0;
		bool bLoop = Wave.bLoop;
		try
		{
			bLoop = Document->GetSlotTiming(size_t(CueIndex), Slot).bLoop;
		}
		catch (const std::exception&)
		{
		}
		Lines.Add(FString::Printf(TEXT("%d: %s #%u, %.1fs%s"), int32(Slot), *AcbEditorSessionLocal::StorageName(Wave.Streaming),
			Wave.AwbId, Seconds, bLoop ? TEXT(", loop") : TEXT("")));
	}
	return Lines;
}

TArray<TSharedPtr<FAcbCueRow>> FAcbEditorSession::BuildRows() const
{
	TArray<TSharedPtr<FAcbCueRow>> Rows;
	if (!Document)
	{
		return Rows;
	}
	try
	{
		TMap<int32, FString> Names;
		if (const AcbCore::UtfTable* NameTable = Document->FindTable("CueNameTable"))
		{
			for (size_t Row = 0; Row < NameTable->RowCount(); ++Row)
			{
				Names.Add(int32(NameTable->GetUnsigned(Row, "CueIndex")), FromUtf8(NameTable->GetString(Row, "CueName")));
			}
		}
		const size_t Count = Document->CueCount();
		Rows.Reserve(int32(Count));
		for (size_t Index = 0; Index < Count; ++Index)
		{
			TSharedPtr<FAcbCueRow> Row = MakeShared<FAcbCueRow>();
			Row->Index = int32(Index);
			Row->Id = Document->CueId(Index);
			if (const FString* Name = Names.Find(int32(Index)))
			{
				Row->Name = *Name;
			}
			Row->Type = AcbEditorSessionLocal::TypeName(Document->CueRefType(Index));
			const uint32 Length = Document->CueLength(Index);
			Row->Length = Length == 0xFFFFFFFFu ? FString(TEXT("loop")) : FString::Printf(TEXT("%d:%02d"), int32(Length / 60000), int32((Length / 1000) % 60));

			TArray<FString> Categories;
			for (const std::string& Category : Document->CueCategories(Index))
			{
				Categories.Add(FromUtf8(Category));
			}
			Row->Categories = FString::Join(Categories, TEXT(" + "));

			const AcbCore::CueChain Chain = Document->ResolveChain(Index);
			Row->NumWaveforms = int32(Chain.WaveformIndices.size());
			if (Chain.SequenceType && Chain.Tracks.size() > 1)
			{
				Row->PlayMode = UTF8_TO_TCHAR(AcbCore::SequenceTypeName(*Chain.SequenceType));
			}
			if (Chain.WaveformIndex && *Chain.WaveformIndex < Document->WaveformCount())
			{
				const AcbCore::WaveformInfo Wave = Document->Waveform(*Chain.WaveformIndex);
				Row->Audio = FString::Printf(TEXT("%s #%u"), *AcbEditorSessionLocal::StorageName(Wave.Streaming), Wave.AwbId);
				Row->bLoop = Wave.bLoop;
				try
				{
					Row->bLoop = Document->GetSlotTiming(Index, 0).bLoop;
				}
				catch (const std::exception&)
				{
				}
			}
			Rows.Add(Row);
		}
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogAEWAcbEditor, Error, TEXT("Reading cues failed: %s"), *FromUtf8(Error.what()));
	}
	return Rows;
}

FString FAcbEditorSession::Describe() const
{
	if (!Document)
	{
		return TEXT("No ACB open.");
	}
	FString Text = FString::Printf(TEXT("%s - %d cues, %d waveforms"),
		*FPaths::GetCleanFilename(LoadedAcb), int32(Document->CueCount()), int32(Document->WaveformCount()));
	if (const AcbCore::Afs2Archive* Memory = Document->MemoryAwb())
	{
		Text += FString::Printf(TEXT(", embedded AWB %d entries"), int32(Memory->Entries.size()));
	}
	if (const AcbCore::Afs2Archive* Stream = Document->StreamAwb())
	{
		Text += FString::Printf(TEXT(", streaming AWB %s (%d entries)"), *FPaths::GetCleanFilename(LoadedAwb), int32(Stream->Entries.size()));
	}
	else if (Document->UsesStreamingAwb())
	{
		Text += TEXT(", streaming AWB not loaded");
	}
	if (Document->IsModified())
	{
		Text += TEXT("  [modified]");
	}
	return Text;
}

uint64 FAcbEditorSession::EstimateOutputAcbSize(const UAcbEditorSettings& Settings) const
{
	if (!Document)
	{
		return 0;
	}
	try
	{
		return Document->EstimateAcbSize(Settings.bForcePerRow);
	}
	catch (const std::exception&)
	{
		return 0;
	}
}

FString FAcbEditorSession::GetVGAudioCli()
{
	const FString Configured = GetDefault<UAcbEditorSettings>()->VGAudioCliPath.FilePath;
	if (!Configured.IsEmpty() && FPaths::FileExists(Configured))
	{
		return FPaths::ConvertRelativePathToFull(Configured);
	}
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AEWAcbEditor")))
	{
		const FString Bundled = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("ThirdParty"), TEXT("VGAudio"), TEXT("VGAudioCli.exe")));
		if (FPaths::FileExists(Bundled))
		{
			return Bundled;
		}
	}
	return FString();
}

bool FAcbEditorSession::DecodeSlotAudio(int32 CueIndex, int32 Slot, FString& OutWav, FString& OutError, bool bOnlyIfDecoded) const
{
	if (!Document || CueIndex < 0 || Slot < 0 || size_t(CueIndex) >= Document->CueCount())
	{
		OutError = TEXT("No cue to decode.");
		return false;
	}
	const FString Exe = GetVGAudioCli();
	if (Exe.IsEmpty())
	{
		OutError = TEXT("Preview needs VGAudioCli.exe (the plugin's ThirdParty/VGAudio folder, or set its path in the settings).");
		return false;
	}
	FString AudioFile;
	try
	{
		const AcbCore::SlotTiming Timing = Document->GetSlotTiming(size_t(CueIndex), size_t(Slot));
		const AcbCore::WaveformInfo Info = Document->Waveform(Timing.WaveformIndex);
		const TCHAR* Extension = Info.Encode == 0 ? TEXT("adx") : (Info.Encode == 2 || Info.Encode == 6) ? TEXT("hca") : nullptr;
		if (!Extension)
		{
			OutError = FString::Printf(TEXT("Preview can't decode this audio (encode type %d; only HCA and ADX)."), int32(Info.Encode));
			return false;
		}
		const AcbCore::Afs2Archive* Archive = Info.Streaming != 0 && Document->StreamAwb() ? Document->StreamAwb() : Document->MemoryAwb();
		const AcbCore::Afs2Entry* Entry = Archive ? Archive->Find(Info.AwbId) : nullptr;
		if (!Entry || !Entry->Source)
		{
			OutError = FString::Printf(TEXT("Audio entry %u isn't in the %s AWB."), Info.AwbId, Info.Streaming != 0 ? TEXT("streaming") : TEXT("embedded"));
			return false;
		}
		const FString Key = FString::Printf(TEXT("%s|%s|%llu|%llu"), *LoadedAcb, *FromUtf8(Entry->SourcePath), Entry->Offset, Entry->Size);
		const FString Dir = FPaths::Combine(GetDefault<UAcbEditorSettings>()->GetOutputDirectory(), TEXT(".preview"), TEXT("audio"));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Base = FPaths::Combine(Dir, FString::Printf(TEXT("%s_%u_%08x"), *GetCueSheetName(), Info.AwbId, FCrc::StrCrc32(*Key)));
		AudioFile = Base + TEXT(".") + Extension;
		OutWav = Base + TEXT(".wav");
		if (FPaths::FileExists(OutWav))
		{
			return true;
		}
		if (bOnlyIfDecoded)
		{
			return false;
		}
		const std::vector<uint8_t> Bytes = Entry->Source->ReadVector(Entry->Offset, size_t(Entry->Size));
		if (!FFileHelper::SaveArrayToFile(TArrayView<const uint8>(Bytes.data(), int32(Bytes.size())), *AudioFile))
		{
			OutError = TEXT("Could not write ") + AudioFile;
			return false;
		}
	}
	catch (const std::exception& Error)
	{
		OutError = FromUtf8(Error.what());
		return false;
	}

	FScopedSlowTask Task(1.f, LOCTEXT("DecodingPreview", "Decoding audio for preview..."));
	Task.MakeDialog();
	int32 ReturnCode = -1;
	FString StdOut;
	FString StdErr;
	FPlatformProcess::ExecProcess(*Exe, *FString::Printf(TEXT("-i \"%s\" -o \"%s\""), *AudioFile, *OutWav), &ReturnCode, &StdOut, &StdErr);
	IFileManager::Get().Delete(*AudioFile);
	if (ReturnCode != 0 || !FPaths::FileExists(OutWav))
	{
		IFileManager::Get().Delete(*OutWav);
		OutError = FString::Printf(TEXT("VGAudioCli could not decode the audio (code %d): %s %s"), ReturnCode, *StdOut.TrimStartAndEnd(), *StdErr.TrimStartAndEnd());
		return false;
	}
	return true;
}

bool FAcbEditorSession::PrepareAudio(const FString& InPath, FString& OutPath, FString& OutError) const
{
	const FString Extension = FPaths::GetExtension(InPath).ToLower();
	if (Extension != TEXT("wav"))
	{
		OutPath = InPath;
		return true;
	}
	const UAcbEditorSettings* Settings = GetDefault<UAcbEditorSettings>();
	const FString Exe = GetVGAudioCli();
	if (Exe.IsEmpty())
	{
		OutError = TEXT("WAV files need VGAudioCli.exe (shipped in the plugin's ThirdParty/VGAudio folder, or set its path in the settings) -- or add an .hca instead.");
		return false;
	}
	const FString CacheDir = FPaths::Combine(Settings->GetOutputDirectory(), TEXT("hca_cache"));
	IFileManager::Get().MakeDirectory(*CacheDir, true);
	OutPath = FPaths::Combine(CacheDir, FPaths::GetBaseFilename(InPath) + TEXT(".hca"));

	IFileManager& Files = IFileManager::Get();
	if (Files.FileExists(*OutPath) && Files.GetTimeStamp(*OutPath) >= Files.GetTimeStamp(*InPath))
	{
		return true;
	}
	int32 ReturnCode = -1;
	FString StdOut;
	FString StdErr;
	const FString Params = FString::Printf(TEXT("-i \"%s\" -o \"%s\""), *InPath, *OutPath);
	FPlatformProcess::ExecProcess(*Exe, *Params, &ReturnCode, &StdOut, &StdErr);
	if (ReturnCode != 0 || !Files.FileExists(*OutPath))
	{
		OutError = FString::Printf(TEXT("VGAudioCli failed on %s (code %d): %s %s"), *FPaths::GetCleanFilename(InPath), ReturnCode, *StdOut, *StdErr);
		return false;
	}
	return true;
}

AcbCore::TrackOptions FAcbEditorSession::MakeTrackOptions(bool bLoop)
{
	AcbCore::TrackOptions Options;
	Options.bLoop = bLoop;
	switch (GetDefault<UAcbEditorSettings>()->NewAudioStorage)
	{
	case EAcbNewAudioStorage::Streaming: Options.Streaming = true; break;
	case EAcbNewAudioStorage::Embedded: Options.Streaming = false; break;
	default: break;
	}
	return Options;
}

FString FAcbEditorSession::ResolveCueName(const FString& Pattern, const FString& BaseName)
{
	const FString Trimmed = Pattern.TrimStartAndEnd();
	return Trimmed.Contains(TEXT("{name}")) ? Trimmed.Replace(TEXT("{name}"), *BaseName, ESearchCase::IgnoreCase) : Trimmed + BaseName;
}

bool FAcbEditorSession::AddTrack(const FString& AudioFile, const TArray<FAcbCueTemplate>& Cues,
	int32 TemplateCue, bool bLoop, FString& OutMessage, TArray<FString>* OutNames)
{
	if (AudioFile.IsEmpty())
	{
		OutMessage = TEXT("No audio file given.");
		return false;
	}
	if (Cues.Num() == 0)
	{
		OutMessage = TEXT("No cues to create -- add at least one cue row.");
		return false;
	}
	const TArray<FString> Categories = GetCategories();

	std::vector<AcbCore::CueSpec> Specs;
	TArray<FString> Names;
	TArray<FString> Notes;
	for (const FAcbCueTemplate& Cue : Cues)
	{
		FString Name = Cue.Pattern.TrimStartAndEnd();
		if (Name.IsEmpty() || Name.EndsWith(TEXT("_")))
		{
			Name += FromUtf8(AcbCore::AcbDocument::SanitizeCueName(ToUtf8(FPaths::GetBaseFilename(AudioFile))));
		}
		if (Name.IsEmpty())
		{
			OutMessage = TEXT("A cue row has no name.");
			return false;
		}
		if (FromUtf8(AcbCore::AcbDocument::SanitizeCueName(ToUtf8(Name))) != Name)
		{
			OutMessage = FString::Printf(TEXT("Cue name '%s': use only letters, digits and _."), *Name);
			return false;
		}
		if (Names.Contains(Name) || FindCue(Name) != INDEX_NONE)
		{
			OutMessage = FString::Printf(TEXT("A cue named '%s' already exists."), *Name);
			return false;
		}
		if (!Cue.Category.IsEmpty() && !Categories.Contains(Cue.Category))
		{
			OutMessage = FString::Printf(TEXT("Cue '%s': this ACB has no category '%s'."), *Name, *Cue.Category);
			return false;
		}
		Names.Add(Name);
		AcbCore::CueSpec Spec;
		Spec.Name = ToUtf8(Name);
		Spec.Category = ToUtf8(Cue.Category);
		int32 Template = TemplateCue;
		if (!Cue.Template.IsEmpty())
		{
			const int32 Found = FindCue(Cue.Template);
			if (Found != INDEX_NONE)
			{
				Template = Found;
			}
			else
			{
				Notes.Add(FString::Printf(TEXT("%s: no cue '%s' to copy settings from, used %s"), *Name, *Cue.Template,
					TemplateCue != INDEX_NONE ? TEXT("'Copy from'") : TEXT("the ACB's defaults")));
			}
		}
		if (Template != INDEX_NONE)
		{
			Spec.TemplateCue = size_t(Template);
			TArray<FString> TemplateCategories;
			for (const std::string& Label : Document->CueCategories(size_t(Template)))
			{
				TemplateCategories.Add(FromUtf8(Label));
			}
			if (Cue.Category.Equals(FString::Join(TemplateCategories, TEXT(" + ")), ESearchCase::IgnoreCase))
			{
				Spec.Category.clear();
			}
		}
		Specs.push_back(Spec);
	}

	FString Prepared;
	if (!PrepareAudio(AudioFile, Prepared, OutMessage))
	{
		return false;
	}
	const std::vector<std::string> Paths = { ToUtf8(Prepared) };
	const AcbCore::TrackOptions Options = MakeTrackOptions(bLoop);
	FString Error;
	if (!Edit([&](AcbCore::AcbDocument& D) { D.AddTrackCues(Paths, Specs, Options); }, Error))
	{
		OutMessage = Error;
		return false;
	}
	OutMessage = FString::Printf(TEXT("Added %s -> %s"), *FPaths::GetCleanFilename(AudioFile), *FString::Join(Names, TEXT(", ")));
	if (OutNames)
	{
		*OutNames = Names;
	}
	if (Notes.Num() > 0)
	{
		OutMessage += TEXT(". Note: ") + FString::Join(Notes, TEXT("; "));
	}
	return true;
}

bool FAcbEditorSession::AddCueFromAudio(int32 SourceIndex, const FString& Name, const FString& Category, FString& OutMessage)
{
	const std::string NewName = ToUtf8(Name.TrimStartAndEnd());
	const std::string NewCategory = ToUtf8(Category);
	uint32_t Id = 0;
	FString Error;
	if (!Edit([&](AcbCore::AcbDocument& D) { Id = D.AddCueForCueAudio(size_t(SourceIndex), NewName, NewCategory); }, Error))
	{
		OutMessage = Error;
		return false;
	}
	OutMessage = FString::Printf(TEXT("Added cue %s (id %u)"), *Name, Id);
	return true;
}

bool FAcbEditorSession::PointCuesAtAudio(const TArray<int32>& TargetIndices, int32 SourceIndex, FString& OutMessage)
{
	FString Error;
	const bool bOk = Edit([&](AcbCore::AcbDocument& D)
	{
		for (int32 Target : TargetIndices)
		{
			if (Target == SourceIndex)
			{
				throw AcbCore::AcbError("A cue can't take its own audio");
			}
		}
		for (int32 Target : TargetIndices)
		{
			D.UseCueAudio(size_t(Target), size_t(SourceIndex));
		}
	}, Error);
	OutMessage = bOk ? FString::Printf(TEXT("%d cue(s) now play the source cue's audio."), TargetIndices.Num()) : Error;
	return bOk;
}

bool FAcbEditorSession::AddVariations(int32 CueIndex, const TArray<FString>& AudioFiles, int32 PlayMode, FString& OutMessage)
{
	std::vector<std::string> Paths;
	for (const FString& File : AudioFiles)
	{
		FString Prepared;
		if (!PrepareAudio(File, Prepared, OutMessage))
		{
			return false;
		}
		Paths.push_back(ToUtf8(Prepared));
	}
	const bool bLoop = Document && CueIndex >= 0 && Document->ResolveChain(size_t(CueIndex)).WaveformIndex
		? Document->Waveform(*Document->ResolveChain(size_t(CueIndex)).WaveformIndex).bLoop : false;
	const AcbCore::TrackOptions Options = MakeTrackOptions(bLoop);
	FString Error;
	const bool bOk = Edit([&](AcbCore::AcbDocument& D)
	{
		for (size_t Index = 0; Index < Paths.size(); ++Index)
		{
			const std::optional<AcbCore::SequenceType> Type = PlayMode != INDEX_NONE && Index == 0
				? std::optional<AcbCore::SequenceType>(AcbCore::SequenceType(PlayMode)) : std::nullopt;
			D.AddWaveformToCue(size_t(CueIndex), Paths[Index], Options, Type);
		}
	}, Error);
	OutMessage = bOk ? FString::Printf(TEXT("Added %d track(s)."), AudioFiles.Num()) : Error;
	return bOk;
}

bool FAcbEditorSession::ReplaceVariation(int32 CueIndex, int32 Slot, const FString& AudioFile, FString& OutMessage)
{
	if (!Document)
	{
		OutMessage = TEXT("No ACB is open.");
		return false;
	}
	FString Prepared;
	if (!PrepareAudio(AudioFile, Prepared, OutMessage))
	{
		return false;
	}
	const std::string Path = ToUtf8(Prepared);
	const AcbCore::CueChain Chain = Document->ResolveChain(size_t(CueIndex));
	const bool bLoop = Slot >= 0 && size_t(Slot) < Chain.WaveformIndices.size()
		? Document->Waveform(Chain.WaveformIndices[size_t(Slot)]).bLoop : false;
	const AcbCore::TrackOptions Options = MakeTrackOptions(bLoop);
	FString Error;
	const bool bOk = Edit([&](AcbCore::AcbDocument& D) { D.ReplaceWaveformOnCue(size_t(CueIndex), Path, size_t(Slot), Options); }, Error);
	OutMessage = bOk ? FString::Printf(TEXT("Replaced track %d with %s."), Slot, *FPaths::GetCleanFilename(AudioFile)) : Error;
	return bOk;
}

bool FAcbEditorSession::RemoveVariation(int32 CueIndex, int32 Slot, FString& OutMessage)
{
	FString Error;
	const bool bOk = Edit([&](AcbCore::AcbDocument& D) { D.RemoveWaveformFromCue(size_t(CueIndex), size_t(Slot)); }, Error);
	OutMessage = bOk ? FString::Printf(TEXT("Removed track %d."), Slot) : Error;
	return bOk;
}

bool FAcbEditorSession::SetPlayMode(int32 CueIndex, uint8 PlayMode, FString& OutMessage)
{
	FString Error;
	const bool bOk = Edit([&](AcbCore::AcbDocument& D) { D.SetCueSequenceType(size_t(CueIndex), AcbCore::SequenceType(PlayMode)); }, Error);
	OutMessage = bOk ? FString::Printf(TEXT("Play mode: %s."), UTF8_TO_TCHAR(AcbCore::SequenceTypeName(PlayMode))) : Error;
	return bOk;
}

FAcbSlotTiming FAcbEditorSession::GetSlotTiming(int32 CueIndex, int32 Slot) const
{
	FAcbSlotTiming Out;
	if (!Document || CueIndex < 0 || Slot < 0 || size_t(CueIndex) >= Document->CueCount())
	{
		return Out;
	}
	try
	{
		const AcbCore::SlotTiming Timing = Document->GetSlotTiming(size_t(CueIndex), size_t(Slot));
		Out.SamplingRate = int32(Timing.SamplingRate);
		Out.NumSamples = Timing.NumSamples;
		Out.bLoop = Timing.bLoop;
		Out.bWaveformLoops = Timing.bWaveformLoops;
		Out.LoopStart = Timing.LoopStart;
		Out.LoopEnd = Timing.LoopEnd;
		Out.bCanSetStart = Timing.bCanSetStartOffset;
		Out.bHasStart = Timing.StartOffsetMs.has_value();
		Out.StartMs = Timing.StartOffsetMs ? int64(*Timing.StartOffsetMs) : 0;
		Out.SharedBy = int32(Document->CountCuesUsingWaveform(Timing.WaveformIndex));
		Out.bValid = true;
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogAEWAcbEditor, Warning, TEXT("Reading start/loop points failed: %s"), *FromUtf8(Error.what()));
	}
	return Out;
}

bool FAcbEditorSession::SetSlotTiming(int32 CueIndex, int32 Slot, TOptional<int64> StartMs, bool bLoop, int64 LoopStart, int64 LoopEnd,
	FString& OutMessage)
{
	if (CueIndex < 0 || Slot < 0)
	{
		OutMessage = TEXT("Select a cue and a track.");
		return false;
	}
	if ((StartMs.IsSet() && (StartMs.GetValue() < 0 || StartMs.GetValue() > int64(MAX_uint32)))
		|| (bLoop && (LoopStart < 0 || LoopEnd < 0 || LoopStart > int64(MAX_uint32) || LoopEnd > int64(MAX_uint32))))
	{
		OutMessage = TEXT("Start and loop points must be positive and inside the audio.");
		return false;
	}
	const std::optional<uint32_t> Start = StartMs.IsSet() ? std::optional<uint32_t>(uint32_t(StartMs.GetValue())) : std::nullopt;
	FString Error;
	const bool bOk = Edit([&](AcbCore::AcbDocument& D)
	{
		D.SetSlotTiming(size_t(CueIndex), size_t(Slot), Start, bLoop, uint32_t(LoopStart), uint32_t(LoopEnd));
	}, Error);
	if (!bOk)
	{
		OutMessage = Error;
		return false;
	}
	const FAcbSlotTiming Now = GetSlotTiming(CueIndex, Slot);
	const double Rate = Now.SamplingRate > 0 ? double(Now.SamplingRate) : 1.0;
	const FString StartText = Now.bHasStart ? FString::Printf(TEXT("starts at %.3fs"), double(Now.StartMs) / 1000.0) : FString(TEXT("starts at the beginning"));
	const FString LoopText = Now.bLoop
		? FString::Printf(TEXT("loops %.3fs-%.3fs (samples %lld-%lld)"), double(Now.LoopStart) / Rate, double(Now.LoopEnd) / Rate, Now.LoopStart, Now.LoopEnd)
		: FString(TEXT("doesn't loop"));
	OutMessage = FString::Printf(TEXT("%s, track %d: %s, %s."), *FromUtf8(Document->CueName(size_t(CueIndex))), Slot, *StartText, *LoopText);
	return true;
}

FAcbSaveResult FAcbEditorSession::Save(const UAcbEditorSettings& Settings)
{
	FAcbSaveResult Result;
	if (!Document)
	{
		Result.Error = TEXT("No ACB is open.");
		return Result;
	}

	const FString Name = GetCueSheetName();
	const FString OutputDir = Settings.GetOutputDirectory();
	const FString StagingDir = FPaths::Combine(OutputDir, TEXT(".staging"));
	IFileManager& Files = IFileManager::Get();
	Files.MakeDirectory(*StagingDir, true);

	const bool bWritesAwb = Document->StreamAwb() != nullptr;
	const FString StagedAcb = FPaths::Combine(StagingDir, Name + TEXT(".acb"));
	const FString StagedAwb = bWritesAwb ? FPaths::Combine(StagingDir, Name + TEXT(".awb")) : FString();
	Result.AcbPath = FPaths::Combine(OutputDir, Name + TEXT(".acb"));
	Result.AwbPath = bWritesAwb ? FPaths::Combine(OutputDir, Name + TEXT(".awb")) : FString();

	{
		FScopedSlowTask Task(100.f, FText::Format(LOCTEXT("Saving", "Saving {0}..."), FText::FromString(Name + TEXT(".acb"))));
		Task.MakeDialog(true);
		AcbEditorSessionLocal::FSaveProgress Progress;
		Progress.Task = &Task;

		AcbCore::AcbSaveOptions Options;
		Options.AcbPath = ToUtf8(StagedAcb);
		Options.AwbPath = ToUtf8(StagedAwb);
		Options.bForcePerRow = Settings.bForcePerRow;
		Options.OnProgress = &AcbEditorSessionLocal::OnSaveProgress;
		Options.ProgressUser = &Progress;
		try
		{
			if (Settings.bRemoveUnusedData)
			{
				const AcbCore::CleanupResult Removed = Document->RemoveUnusedData();
				if (!Removed.Skipped.empty())
				{
					Result.Cleanup = TEXT("Unused data not removed: ") + FromUtf8(Removed.Skipped);
				}
				else if (Removed.Any())
				{
					Result.Cleanup = FString::Printf(TEXT("Removed unused data: %llu sequence(s), %llu track(s), %llu event(s), %llu synth(s), %llu waveform(s), %llu audio file(s) (%.1f MB)."),
						uint64(Removed.Sequences), uint64(Removed.Tracks), uint64(Removed.Events), uint64(Removed.Synths), uint64(Removed.Waveforms),
						uint64(Removed.MemoryAudio + Removed.StreamAudio), double(Removed.AudioBytes) / (1024.0 * 1024.0));
				}
			}
			Document->Save(Options);
		}
		catch (const std::exception& Error)
		{
			Result.Error = FromUtf8(Error.what());
			return Result;
		}
	}

	Close();
	bool bMoved = Files.Move(*Result.AcbPath, *StagedAcb, true, true);
	if (bMoved && bWritesAwb)
	{
		bMoved = Files.Move(*Result.AwbPath, *StagedAwb, true, true);
	}
	if (!bMoved)
	{
		Result.Error = FString::Printf(TEXT("Saved to %s but could not move it to %s (file in use?)."), *StagingDir, *OutputDir);
		FString Ignored;
		Open(StagedAcb, StagedAwb, Ignored);
		return Result;
	}

	FString ReopenError;
	if (!Open(Result.AcbPath, Result.AwbPath, ReopenError))
	{
		Result.Error = TEXT("Saved, but reopening the result failed: ") + ReopenError;
		return Result;
	}
	Result.bOk = true;
	return Result;
}

#undef LOCTEXT_NAMESPACE
