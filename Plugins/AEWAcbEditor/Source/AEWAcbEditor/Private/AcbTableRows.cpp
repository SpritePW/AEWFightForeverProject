#include "AcbTableRows.h"
#include "AcbEditorSession.h"
#include "AcbEditorSettings.h"

#include "DataTableEditorUtils.h"
#include "Engine/CompositeDataTable.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "SoundAtomCue.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace AcbTableRowsLocal
{
	using namespace AcbTableRows;

	UDataTable* Load(const FSoftObjectPath& Path)
	{
		return Path.IsValid() ? Cast<UDataTable>(Path.TryLoad()) : nullptr;
	}

	FString Authored(const UStruct* Struct, const FProperty* Property)
	{
		return Struct->GetAuthoredNameForField(Property);
	}

	bool IsAlnum(TCHAR C)
	{
		return FChar::IsAlnum(C);
	}

	bool IsCueClass(const UClass* Class)
	{
		return Class && Class->IsChildOf(USoundAtomCue::StaticClass());
	}

	FString AssetName(const FSoftObjectPath& Path)
	{
		return Path.GetAssetName();
	}

	void CollectCues(const UStruct* Struct, const void* Data, TArray<FString>& Out);

	void CollectValue(FProperty* Property, const void* Value, TArray<FString>& Out)
	{
		if (FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			const FSoftObjectPath Path = static_cast<const FSoftObjectPtr*>(Value)->ToSoftObjectPath();
			if (!Path.IsNull() && IsCueClass(Soft->PropertyClass))
			{
				Out.AddUnique(AssetName(Path));
			}
		}
		else if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
		{
			if (const UObject* Cue = Object->GetObjectPropertyValue(Value))
			{
				if (IsCueClass(Cue->GetClass()))
				{
					Out.AddUnique(Cue->GetName());
				}
			}
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			CollectCues(StructProperty->Struct, Value, Out);
		}
		else if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(Array, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				CollectValue(Array->Inner, Helper.GetRawPtr(Index), Out);
			}
		}
	}

	void SourceTables(const UDataTable* Table, TArray<const UDataTable*>& Out)
	{
		if (!Table || Out.Contains(Table))
		{
			return;
		}
		if (const UCompositeDataTable* Composite = Cast<UCompositeDataTable>(Table))
		{
			if (FArrayProperty* Parents = FindFProperty<FArrayProperty>(UCompositeDataTable::StaticClass(), TEXT("ParentTables")))
			{
				FScriptArrayHelper Helper(Parents, Parents->ContainerPtrToValuePtr<void>(Composite));
				FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(Parents->Inner);
				for (int32 Index = 0; Inner && Index < Helper.Num(); ++Index)
				{
					SourceTables(Cast<UDataTable>(Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index))), Out);
				}
				return;
			}
		}
		Out.Add(Table);
	}

	void CollectCues(const UStruct* Struct, const void* Data, TArray<FString>& Out)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				CollectValue(*It, It->ContainerPtrToValuePtr<void>(Data, Index), Out);
			}
		}
	}

	UEnum* GetEnum(FProperty* Property, FNumericProperty*& OutUnderlying)
	{
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			OutUnderlying = EnumProperty->GetUnderlyingProperty();
			return EnumProperty->GetEnum();
		}
		if (FByteProperty* Byte = CastField<FByteProperty>(Property))
		{
			OutUnderlying = Byte;
			return Byte->Enum;
		}
		return nullptr;
	}

	FString ShortEnumName(const UEnum* Enum, int64 Value)
	{
		return Enum->GetNameStringByValue(Value);
	}

	bool SetEnumByName(FProperty* Property, void* Value, const FString& Name)
	{
		FNumericProperty* Underlying = nullptr;
		UEnum* Enum = GetEnum(Property, Underlying);
		if (!Enum || !Underlying)
		{
			return false;
		}
		FString Short = Name;
		int32 Colon = INDEX_NONE;
		if (Short.FindLastChar(TEXT(':'), Colon))
		{
			Short = Short.Mid(Colon + 1);
		}
		int64 EnumValue = Enum->GetValueByNameString(Short);
		if (EnumValue == INDEX_NONE)
		{
			EnumValue = Enum->GetValueByNameString(Enum->GetName() + TEXT("::") + Short);
		}
		if (EnumValue == INDEX_NONE)
		{
			return false;
		}
		Underlying->SetIntPropertyValue(Value, EnumValue);
		return true;
	}

	bool IsEditableField(FProperty* Property)
	{
		FNumericProperty* Underlying = nullptr;
		return CastField<FTextProperty>(Property) || CastField<FStrProperty>(Property) || CastField<FNameProperty>(Property)
			|| GetEnum(Property, Underlying) != nullptr;
	}

	FString ExportField(FProperty* Property, const void* Value)
	{
		if (FTextProperty* Text = CastField<FTextProperty>(Property))
		{
			return static_cast<const FText*>(Value)->ToString();
		}
		FNumericProperty* Underlying = nullptr;
		if (UEnum* Enum = GetEnum(Property, Underlying))
		{
			return ShortEnumName(Enum, Underlying->GetSignedIntPropertyValue(Value));
		}
		FString Out;
		Property->ExportTextItem(Out, Value, nullptr, nullptr, PPF_None);
		return Out;
	}

	bool ParseNumber(const FName& Name, int64& Out)
	{
		const FString Text = Name.ToString();
		if (Text.IsEmpty() || !Text.IsNumeric())
		{
			return false;
		}
		Out = FCString::Atoi64(*Text);
		return true;
	}

	FProperty* FindUnlockField(const UScriptStruct* Struct)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!CastField<FIntProperty>(*It))
			{
				continue;
			}
			const FString Name = Authored(Struct, *It);
			if (Name.StartsWith(TEXT("Unlock"), ESearchCase::IgnoreCase) && Name.EndsWith(TEXT("Id"), ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	FProperty* FindIntField(const UScriptStruct* Struct, const TCHAR* AuthoredName)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (CastField<FIntProperty>(*It) && Authored(Struct, *It).Equals(AuthoredName, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	FProperty* FindWrestlerIdField(const UScriptStruct* Struct)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FNumericProperty* Underlying = nullptr;
			const UEnum* Enum = GetEnum(*It, Underlying);
			if (Enum && Enum->GetName().StartsWith(TEXT("EWrestlerID")))
			{
				return *It;
			}
		}
		return nullptr;
	}

	int64 GetEnumValue(FProperty* Property, const void* Container)
	{
		FNumericProperty* Underlying = nullptr;
		GetEnum(Property, Underlying);
		return Underlying ? Underlying->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Container)) : 0;
	}

	void MakeUnlocked(const UScriptStruct* Struct, void* Row)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FString Name = Authored(Struct, *It);
			void* Value = It->ContainerPtrToValuePtr<void>(Row);
			if (FBoolProperty* Bool = CastField<FBoolProperty>(*It))
			{
				if (Name == TEXT("DefaultUnlocked") || Name == TEXT("DefaultStockStatus"))
				{
					Bool->SetPropertyValue(Value, true);
				}
				else if (Name == TEXT("ShopItem") || Name == TEXT("CrateItem") || Name == TEXT("FreeB_PassItem") || Name == TEXT("PremiumB_PassItem")
					|| Name == TEXT("ChallengeItem") || Name == TEXT("HideUnlockCondition") || Name == TEXT("NeedAdditionalCondition"))
				{
					Bool->SetPropertyValue(Value, false);
				}
			}
			else if (Name == TEXT("ItemPrice"))
			{
				if (FNumericProperty* Numeric = CastField<FNumericProperty>(*It))
				{
					Numeric->SetIntPropertyValue(Value, int64(0));
				}
			}
			else if (Name == TEXT("DlcItemName") || Name == TEXT("BulkBuyingStr") || Name == TEXT("BulkBuyingItemIdList"))
			{
				It->ClearValue(Value);
			}
		}
	}

	FName FindRowByInt(const UDataTable* Table, FProperty* Field, int64 Value)
	{
		FIntProperty* Int = CastField<FIntProperty>(Field);
		if (!Table || !Int)
		{
			return NAME_None;
		}
		for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
		{
			if (Int->GetPropertyValue_InContainer(Row.Value) == Value)
			{
				return Row.Key;
			}
		}
		return NAME_None;
	}

	int64 Allocate(int64 First, int64 Last, const TSet<int64>& Used)
	{
		if (First > 0)
		{
			for (int64 Value = First; Value <= FMath::Max(First, Last); ++Value)
			{
				if (!Used.Contains(Value))
				{
					return Value;
				}
			}
			return -1;
		}
		int64 Max = 0;
		for (int64 Value : Used)
		{
			Max = FMath::Max(Max, Value);
		}
		return Max + 1;
	}

	FName UniqueName(const FString& Wanted, std::initializer_list<const UDataTable*> Tables)
	{
		auto Taken = [&Tables](const FName& Name)
		{
			for (const UDataTable* Table : Tables)
			{
				if (Table && Table->GetRowMap().Contains(Name))
				{
					return true;
				}
			}
			return false;
		};
		FName Name(*Wanted);
		for (int32 Suffix = 2; Taken(Name); ++Suffix)
		{
			Name = FName(*FString::Printf(TEXT("%s_%d"), *Wanted, Suffix));
		}
		return Name;
	}

	struct FRewrite
	{
		FString CueFolder;
		const FPlannedRow* Plan = nullptr;
		TMap<FString, FText> TextMap;
		TArray<FString>* Log = nullptr;
		TArray<FString> Missing;
		bool bLinked = false;
	};

	FString MappedName(const FRewrite& Context, const FString& OldName)
	{
		for (const TPair<FString, FString>& Pair : Context.Plan->CueMap)
		{
			if (Pair.Key.Equals(OldName, ESearchCase::IgnoreCase))
			{
				return Pair.Value;
			}
		}
		return Substitute(OldName, Context.Plan->From, Context.Plan->To);
	}

	UObject* LoadCue(const FString& Folder, const FString& Name)
	{
		const FSoftObjectPath Path(Folder / Name + TEXT(".") + Name);
		return Path.TryLoad();
	}

	UObject* FindNewCue(FRewrite& Context, const UObject* OldCue, const FString& NewName)
	{
		UObject* Found = LoadCue(Context.CueFolder, NewName);
		if (!Found && OldCue)
		{
			Found = LoadCue(FPackageName::GetLongPackagePath(OldCue->GetOutermost()->GetName()), NewName);
		}
		if (!Found)
		{
			Context.Missing.AddUnique(NewName);
		}
		return Found;
	}

	void RewriteStruct(const UStruct* Struct, void* Data, FRewrite& Context, bool bTopLevel);

	void RewriteValue(const UStruct* Owner, FProperty* Property, void* Value, FRewrite& Context, bool bTopLevel)
	{
		const FPlannedRow& Plan = *Context.Plan;
		if (bTopLevel)
		{
			if (const FString* User = Plan.Values.Find(Property->GetFName()))
			{
				if (!User->IsEmpty())
				{
					if (FTextProperty* Text = CastField<FTextProperty>(Property))
					{
						*static_cast<FText*>(Value) = FText::FromString(*User);
						return;
					}
					if (FStrProperty* Str = CastField<FStrProperty>(Property))
					{
						*static_cast<FString*>(Value) = *User;
						return;
					}
					if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
					{
						*static_cast<FName*>(Value) = FName(**User);
						return;
					}
					if (SetEnumByName(Property, Value, *User))
					{
						return;
					}
					if (Property->ImportText(**User, Value, PPF_None, nullptr))
					{
						return;
					}
					Context.Log->Add(FString::Printf(TEXT("warning: '%s' isn't a valid %s; the copied value is used."), **User, *Authored(Owner, Property)));
				}
			}
		}

		if (FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			FSoftObjectPtr& Ptr = *static_cast<FSoftObjectPtr*>(Value);
			const FSoftObjectPath Path = Ptr.ToSoftObjectPath();
			if (!Path.IsNull() && IsCueClass(Soft->PropertyClass))
			{
				const FString NewName = MappedName(Context, AssetName(Path));
				if (NewName != AssetName(Path))
				{
					UObject* NewCue = FindNewCue(Context, Path.ResolveObject(), NewName);
					Ptr = NewCue ? FSoftObjectPtr(FSoftObjectPath(NewCue)) : FSoftObjectPtr();
				}
			}
			return;
		}
		if (FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
		{
			UObject* Old = Object->GetObjectPropertyValue(Value);
			if (Old && IsCueClass(Old->GetClass()))
			{
				const FString NewName = MappedName(Context, Old->GetName());
				if (NewName != Old->GetName())
				{
					Object->SetObjectPropertyValue(Value, FindNewCue(Context, Old, NewName));
				}
			}
			return;
		}
		if (FStrProperty* Str = CastField<FStrProperty>(Property))
		{
			FString& Text = *static_cast<FString*>(Value);
			const FString Mapped = MappedName(Context, Text);
			Text = Mapped == Text ? Text : MatchCase(Text, Mapped);
			return;
		}
		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			FName& Name = *static_cast<FName*>(Value);
			const FString Old = Name.ToString();
			const FString Mapped = MappedName(Context, Old);
			if (Mapped != Old)
			{
				Name = FName(*MatchCase(Old, Mapped));
			}
			return;
		}
		if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			FText& Text = *static_cast<FText*>(Value);
			const FString Old = Text.ToString();
			if (const FText* Mapped = Context.TextMap.Find(Old))
			{
				Text = *Mapped;
				return;
			}
			if (Old.IsEmpty())
			{
				return;
			}
			const FString New = Substitute(Old, Plan.From, Plan.To);
			if (New != Old)
			{
				Text = FText::FromString(New);
			}
			else if (bTopLevel && !Context.bLinked && !Plan.To.IsEmpty())
			{
				Text = FText::FromString(Plan.To);
			}
			return;
		}
		FNumericProperty* Underlying = nullptr;
		if (UEnum* Enum = GetEnum(Property, Underlying))
		{
			const FString Old = ShortEnumName(Enum, Underlying->GetSignedIntPropertyValue(Value));
			const FString New = Substitute(Old, Plan.From, Plan.To);
			if (New != Old && !SetEnumByName(Property, Value, New))
			{
				Underlying->SetIntPropertyValue(Value, Enum->GetValueByIndex(0));
			}
			return;
		}
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			RewriteStruct(StructProperty->Struct, Value, Context, false);
			return;
		}
		if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(Array, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				RewriteValue(Owner, Array->Inner, Helper.GetRawPtr(Index), Context, false);
			}
		}
	}

	void RewriteStruct(const UStruct* Struct, void* Data, FRewrite& Context, bool bTopLevel)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			for (int32 Index = 0; Index < It->ArrayDim; ++Index)
			{
				RewriteValue(Struct, *It, It->ContainerPtrToValuePtr<void>(Data, Index), Context, bTopLevel);
			}
		}
	}

	struct FRowBuffer
	{
		const UScriptStruct* Struct;
		uint8* Memory;
		FRowBuffer(const UScriptStruct* InStruct, const void* Source) : Struct(InStruct)
		{
			Memory = static_cast<uint8*>(FMemory::Malloc(Struct->GetStructureSize(), Struct->GetMinAlignment()));
			Struct->InitializeStruct(Memory);
			Struct->CopyScriptStruct(Memory, Source);
		}
		~FRowBuffer()
		{
			Struct->DestroyStruct(Memory);
			FMemory::Free(Memory);
		}
	};

	void AddRow(UDataTable* Table, FName Name, const FRowBuffer& Row, TArray<UPackage*>& ToSave)
	{
		FDataTableEditorUtils::BroadcastPreChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
		Table->Modify();
		Table->AddRow(Name, *reinterpret_cast<const FTableRowBase*>(Row.Memory));
		Table->MarkPackageDirty();
		FDataTableEditorUtils::BroadcastPostChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
		ToSave.AddUnique(Table->GetOutermost());
	}

	bool IsWritable(UDataTable* Table, TArray<FString>& OutLog)
	{
		if (Table->GetOutermost()->HasAnyPackageFlags(PKG_Cooked))
		{
			OutLog.Add(FString::Printf(TEXT("warning: %s is cooked and can't be changed."), *Table->GetName()));
			return false;
		}
		return true;
	}

	void CollectTexts(const UStruct* Struct, const void* Data, TArray<FString>& Out)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (FTextProperty* Text = CastField<FTextProperty>(*It))
			{
				Out.Add(Text->GetPropertyValue_InContainer(Data).ToString());
			}
		}
	}
}

FString AcbTableRows::MatchCase(const FString& Original, const FString& Replacement)
{
	bool bUpper = false;
	bool bLower = false;
	for (TCHAR C : Original)
	{
		bUpper |= FChar::IsUpper(C);
		bLower |= FChar::IsLower(C);
	}
	if (bUpper && !bLower)
	{
		return Replacement.ToUpper();
	}
	if (bLower && !bUpper)
	{
		return Replacement.ToLower();
	}
	return Replacement;
}

FString AcbTableRows::Substitute(const FString& Text, const FString& From, const FString& To)
{
	if (From.IsEmpty() || From.Equals(To, ESearchCase::CaseSensitive))
	{
		return Text;
	}
	FString Out;
	int32 Pos = 0;
	while (Pos < Text.Len())
	{
		const int32 Hit = Text.Find(From, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos);
		if (Hit == INDEX_NONE)
		{
			break;
		}
		const int32 End = Hit + From.Len();
		const bool bStartOk = Hit == 0 || !AcbTableRowsLocal::IsAlnum(Text[Hit - 1]);
		const bool bEndOk = End >= Text.Len() || !AcbTableRowsLocal::IsAlnum(Text[End]);
		Out += Text.Mid(Pos, Hit - Pos);
		if (bStartOk && bEndOk)
		{
			Out += MatchCase(Text.Mid(Hit, From.Len()), To);
		}
		else
		{
			Out += Text.Mid(Hit, From.Len());
		}
		Pos = End;
	}
	Out += Text.Mid(Pos);
	return Out;
}

TArray<AcbTableRows::FPlannedRow> AcbTableRows::Find(const TArray<FString>& TemplateCues, TArray<FString>& OutLog)
{
	using namespace AcbTableRowsLocal;
	TArray<FPlannedRow> Result;
	if (TemplateCues.Num() == 0)
	{
		return Result;
	}
	const UAcbEditorSettings* Settings = GetDefault<UAcbEditorSettings>();
	UDataTable* UnlockTable = Load(Settings->BaseUnlockTable);
	FProperty* UniqueIdField = UnlockTable && UnlockTable->GetRowStruct() ? FindIntField(UnlockTable->GetRowStruct(), TEXT("UniqueID")) : nullptr;

	for (const FAcbRowTable& Entry : Settings->RowTables)
	{
		UDataTable* Table = Load(Entry.Table);
		if (!Table || !Table->GetRowStruct())
		{
			continue;
		}
		const UScriptStruct* Struct = Table->GetRowStruct();
		FName BestRow;
		int32 BestCovered = 0;
		int32 BestTotal = 0;
		TArray<FString> BestCues;
		for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
		{
			TArray<FString> Cues;
			CollectCues(Struct, Row.Value, Cues);
			int32 Covered = 0;
			for (const FString& Cue : Cues)
			{
				Covered += TemplateCues.ContainsByPredicate([&Cue](const FString& T) { return T.Equals(Cue, ESearchCase::IgnoreCase); }) ? 1 : 0;
			}
			if (Covered > BestCovered)
			{
				BestRow = Row.Key;
				BestCovered = Covered;
				BestTotal = Cues.Num();
				BestCues = Cues;
			}
		}
		if (BestCovered == 0)
		{
			continue;
		}

		FPlannedRow Plan;
		Plan.TableName = Entry.Name.IsEmpty() ? Table->GetName() : Entry.Name;
		Plan.Table = Entry.Table;
		Plan.NewTable = Entry.NewTable;
		Plan.FirstID = Entry.bNumberedRows ? Entry.FirstID : 0;
		Plan.LastID = Entry.bNumberedRows ? Entry.LastID : 0;
		Plan.UnlockNewTable = Entry.UnlockNewTable;
		Plan.FirstUnlockID = Entry.FirstUnlockID;
		Plan.LastUnlockID = Entry.LastUnlockID;
		Plan.TemplateRow = BestRow;
		Plan.CueFields = BestTotal;
		Plan.CoveredCueFields = BestCovered;
		Plan.TemplateCues = BestCues;
		Plan.bEnabled = Entry.NewTable.IsValid();

		const uint8* Template = Table->GetRowMap()[BestRow];
		FNumericProperty* Underlying = nullptr;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!IsEditableField(*It))
			{
				continue;
			}
			const FString Value = ExportField(*It, It->ContainerPtrToValuePtr<void>(Template));
			if (!CastField<FTextProperty>(*It) && !GetEnum(*It, Underlying) && Authored(Struct, *It).Contains(TEXT("Cue")))
			{
				continue;
			}
			const bool bCueName = BestCues.ContainsByPredicate([&Value](const FString& Cue) { return Cue.Equals(Value, ESearchCase::IgnoreCase); });
			if (bCueName)
			{
				continue;
			}
			FFieldInfo Info;
			Info.Field = It->GetFName();
			Info.Label = Authored(Struct, *It);
			Info.TemplateValue = Value;
			Plan.Fields.Add(Info);
		}

		if (FProperty* UnlockField = FindUnlockField(Struct))
		{
			const int32 UnlockId = CastField<FIntProperty>(UnlockField)->GetPropertyValue_InContainer(Template);
			Plan.UnlockField = UnlockField->GetFName();
			if (UnlockId != 0 && UniqueIdField)
			{
				Plan.UnlockTemplateRow = FindRowByInt(UnlockTable, UniqueIdField, UnlockId);
			}
			Plan.bAddUnlock = !Plan.UnlockTemplateRow.IsNone() && Plan.UnlockNewTable.IsValid();
		}
		Result.Add(MoveTemp(Plan));
	}
	return Result;
}

void AcbTableRows::SetCues(FPlannedRow& Row, const TMap<FString, FString>& CueMap)
{
	Row.CueMap.Reset();
	for (const TPair<FString, FString>& Pair : CueMap)
	{
		Row.CueMap.Add(Pair.Key, Pair.Value);
	}
	Row.From.Reset();
	Row.To.Reset();
	for (const FString& Template : Row.TemplateCues)
	{
		const FString* New = nullptr;
		for (const TPair<FString, FString>& Pair : CueMap)
		{
			if (Pair.Key.Equals(Template, ESearchCase::IgnoreCase))
			{
				New = &Pair.Value;
			}
		}
		if (!New)
		{
			continue;
		}
		const FString& T = Template;
		const FString& N = *New;
		int32 Prefix = 0;
		while (Prefix < T.Len() && Prefix < N.Len() && T[Prefix] == N[Prefix])
		{
			++Prefix;
		}
		int32 Underscore = INDEX_NONE;
		Prefix = T.Left(Prefix).FindLastChar(TEXT('_'), Underscore) ? Underscore + 1 : 0;
		int32 Suffix = 0;
		while (Suffix < T.Len() - Prefix && Suffix < N.Len() - Prefix && T[T.Len() - 1 - Suffix] == N[N.Len() - 1 - Suffix])
		{
			++Suffix;
		}
		const int32 SuffixStart = T.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, T.Len() - Suffix);
		Suffix = (Suffix > 0 && SuffixStart != INDEX_NONE) ? T.Len() - SuffixStart : 0;
		Row.From = T.Mid(Prefix, T.Len() - Prefix - Suffix);
		Row.To = N.Mid(Prefix, N.Len() - Prefix - Suffix);
		break;
	}
}

void AcbTableRows::Merge(TArray<FPlannedRow>& Pending, const FPlannedRow& Added)
{
	for (FPlannedRow& Existing : Pending)
	{
		if (Existing.NewTable == Added.NewTable && Existing.TemplateRow == Added.TemplateRow
			&& Existing.From.Equals(Added.From, ESearchCase::IgnoreCase) && Existing.To.Equals(Added.To, ESearchCase::IgnoreCase))
		{
			Existing.CueMap.Append(Added.CueMap);
			for (const TPair<FName, FString>& Value : Added.Values)
			{
				if (!Value.Value.IsEmpty())
				{
					Existing.Values.Add(Value.Key, Value.Value);
				}
			}
			if (!Added.RowName.IsEmpty())
			{
				Existing.RowName = Added.RowName;
			}
			return;
		}
	}
	Pending.Add(Added);
}

void AcbTableRows::RenameCue(TArray<FPlannedRow>& Pending, const FString& OldName, const FString& NewName)
{
	for (FPlannedRow& Row : Pending)
	{
		for (TPair<FString, FString>& Pair : Row.CueMap)
		{
			if (Pair.Value == OldName)
			{
				Pair.Value = NewName;
			}
		}
	}
	Pending.RemoveAll([](const FPlannedRow& Row)
	{
		for (const TPair<FString, FString>& Pair : Row.CueMap)
		{
			if (!Pair.Value.IsEmpty())
			{
				return false;
			}
		}
		return true;
	});
}

int32 AcbTableRows::Apply(const FString& CueFolder, const TArray<FPlannedRow>& Rows, TArray<UPackage*>& ToSave, TArray<FString>& OutLog)
{
	using namespace AcbTableRowsLocal;
	const UAcbEditorSettings* Settings = GetDefault<UAcbEditorSettings>();
	int32 Added = 0;
	for (const FPlannedRow& Plan : Rows)
	{
		if (!Plan.bEnabled)
		{
			continue;
		}
		UDataTable* Table = Load(Plan.Table);
		UDataTable* NewTable = Load(Plan.NewTable);
		if (!Table || !NewTable || !Table->GetRowStruct())
		{
			OutLog.Add(FString::Printf(TEXT("warning: %s: table not found (%s / %s); row not added."), *Plan.TableName, *Plan.Table.ToString(), *Plan.NewTable.ToString()));
			continue;
		}
		const UScriptStruct* Struct = Table->GetRowStruct();
		if (NewTable->GetRowStruct() != Struct)
		{
			OutLog.Add(FString::Printf(TEXT("warning: %s uses a different row struct from %s; row not added."), *NewTable->GetName(), *Table->GetName()));
			continue;
		}
		if (!IsWritable(NewTable, OutLog))
		{
			continue;
		}
		const uint8* Template = Table->FindRowUnchecked(Plan.TemplateRow);
		if (!Template)
		{
			OutLog.Add(FString::Printf(TEXT("warning: %s has no row %s any more; row not added."), *Table->GetName(), *Plan.TemplateRow.ToString()));
			continue;
		}

		TArray<FString> NewCues;
		for (const TPair<FString, FString>& Pair : Plan.CueMap)
		{
			if (!Pair.Value.IsEmpty())
			{
				NewCues.Add(Pair.Value);
			}
		}
		TArray<const UDataTable*> Holders;
		SourceTables(Table, Holders);
		SourceTables(NewTable, Holders);
		FString MatchedRow;
		for (const UDataTable* Check : Holders)
		{
			for (const TPair<FName, uint8*>& Row : Check->GetRowMap())
			{
				TArray<FString> Cues;
				CollectCues(Struct, Row.Value, Cues);
				if (MatchedRow.IsEmpty() && Cues.ContainsByPredicate([&NewCues](const FString& Cue) { return NewCues.Contains(Cue); }))
				{
					MatchedRow = FString::Printf(TEXT("%s row %s"), *Check->GetName(), *Row.Key.ToString());
				}
			}
		}
		if (!MatchedRow.IsEmpty())
		{
			OutLog.Add(FString::Printf(TEXT("%s already uses %s, so no new row was added to %s."), *MatchedRow, *FString::Join(NewCues, TEXT(", ")), *NewTable->GetName()));
			continue;
		}

		FRowBuffer Row(Struct, Template);
		FRewrite Context;
		Context.CueFolder = CueFolder;
		Context.Plan = &Plan;
		Context.Log = &OutLog;
		RewriteStruct(Struct, Row.Memory, Context, true);

		int64 TemplateNumber = 0;
		const bool bNumbered = ParseNumber(Plan.TemplateRow, TemplateNumber);
		FName RowName;
		int64 NewNumber = 0;
		TSet<int64> Used;
		for (const UDataTable* Check : { (const UDataTable*)Table, (const UDataTable*)NewTable })
		{
			for (const TPair<FName, uint8*>& Existing : Check->GetRowMap())
			{
				int64 Number = 0;
				if (ParseNumber(Existing.Key, Number))
				{
					Used.Add(Number);
				}
			}
		}
		int64 WrestlerNumber = 0;
		if (bNumbered)
		{
			if (FProperty* Wrestler = FindWrestlerIdField(Struct))
			{
				if (GetEnumValue(Wrestler, Template) == TemplateNumber)
				{
					WrestlerNumber = GetEnumValue(Wrestler, Row.Memory);
				}
			}
		}
		if (!Plan.RowName.IsEmpty())
		{
			RowName = UniqueName(Plan.RowName, { Table, NewTable });
			ParseNumber(RowName, NewNumber);
		}
		else if (WrestlerNumber > 0)
		{
			if (Used.Contains(WrestlerNumber))
			{
				OutLog.Add(FString::Printf(TEXT("%s already has row %lld for that WrestlerID; row not added (set Row name to add it anyway)."), *Plan.TableName, WrestlerNumber));
				continue;
			}
			NewNumber = WrestlerNumber;
			RowName = FName(*FString::Printf(TEXT("%lld"), NewNumber));
		}
		else if (bNumbered)
		{
			NewNumber = Allocate(Plan.FirstID, Plan.LastID, Used);
			if (NewNumber < 0)
			{
				OutLog.Add(FString::Printf(TEXT("warning: %s: no free row number between %d and %d; row not added."), *Plan.TableName, Plan.FirstID, Plan.LastID));
				continue;
			}
			RowName = FName(*FString::Printf(TEXT("%lld"), NewNumber));
		}
		else
		{
			const FString Old = Plan.TemplateRow.ToString();
			FString Wanted = Substitute(Old, Plan.From, Plan.To);
			if (Wanted == Old)
			{
				Wanted = Old + TEXT("_") + Plan.To;
			}
			RowName = UniqueName(Wanted, { Table, NewTable });
		}

		if (bNumbered && NewNumber > 0)
		{
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FIntProperty* Int = CastField<FIntProperty>(*It);
				if (Int && It->GetFName() != Plan.UnlockField && !Plan.Values.Contains(It->GetFName())
					&& Int->GetPropertyValue_InContainer(Template) == TemplateNumber)
				{
					Int->SetPropertyValue_InContainer(Row.Memory, int32(NewNumber));
				}
			}
		}

		FString UnlockNote;
		if (!Plan.UnlockField.IsNone() && Plan.bAddUnlock && !Plan.UnlockTemplateRow.IsNone() && !Plan.Values.Contains(Plan.UnlockField))
		{
			UDataTable* UnlockTable = Load(Settings->BaseUnlockTable);
			UDataTable* UnlockNew = Load(Plan.UnlockNewTable);
			FProperty* UniqueId = UnlockTable && UnlockTable->GetRowStruct() ? FindIntField(UnlockTable->GetRowStruct(), TEXT("UniqueID")) : nullptr;
			const uint8* UnlockTemplate = UnlockTable ? UnlockTable->FindRowUnchecked(Plan.UnlockTemplateRow) : nullptr;
			if (!UnlockTable || !UnlockNew || !UniqueId || !UnlockTemplate || UnlockNew->GetRowStruct() != UnlockTable->GetRowStruct() || !IsWritable(UnlockNew, OutLog))
			{
				OutLog.Add(TEXT("warning: unlock tables not found or don't match (Settings > Tables); the copied unlock ID is kept."));
			}
			else
			{
				const UScriptStruct* UnlockStruct = UnlockTable->GetRowStruct();
				FIntProperty* UniqueInt = CastField<FIntProperty>(UniqueId);
				TSet<int64> UsedUnlockIds;
				for (const UDataTable* Check : { (const UDataTable*)UnlockTable, (const UDataTable*)UnlockNew })
				{
					for (const TPair<FName, uint8*>& UnlockRow : Check->GetRowMap())
					{
						UsedUnlockIds.Add(UniqueInt->GetPropertyValue_InContainer(UnlockRow.Value));
					}
				}
				int64 NewId = -1;
				{
					const int64 TemplateUnlock = CastField<FIntProperty>(Struct->FindPropertyByName(Plan.UnlockField))->GetPropertyValue_InContainer(Template);
					const int64 Paired = (bNumbered && NewNumber > 0) ? NewNumber + (TemplateUnlock - TemplateNumber) : -1;
					const bool bInRange = Plan.FirstUnlockID <= 0 || (Paired >= Plan.FirstUnlockID && Paired <= FMath::Max(Plan.FirstUnlockID, Plan.LastUnlockID));
					NewId = (Paired > 0 && bInRange && !UsedUnlockIds.Contains(Paired)) ? Paired : Allocate(Plan.FirstUnlockID, Plan.LastUnlockID, UsedUnlockIds);
				}
				if (NewId < 0)
				{
					OutLog.Add(FString::Printf(TEXT("warning: no free unlock ID between %d and %d; the copied unlock ID is kept."), Plan.FirstUnlockID, Plan.LastUnlockID));
				}
				else
				{
					const int32 OldId = UniqueInt->GetPropertyValue_InContainer(UnlockTemplate);
					FRowBuffer Unlock(UnlockStruct, UnlockTemplate);
					UniqueInt->SetPropertyValue_InContainer(Unlock.Memory, int32(NewId));

					FRewrite UnlockContext;
					UnlockContext.CueFolder = CueFolder;
					UnlockContext.Plan = &Plan;
					UnlockContext.Log = &OutLog;
					TArray<FString> OldTexts;
					CollectTexts(Struct, Template, OldTexts);
					TArray<FText> NewTextValues;
					for (TFieldIterator<FProperty> It(Struct); It; ++It)
					{
						if (FTextProperty* Text = CastField<FTextProperty>(*It))
						{
							NewTextValues.Add(Text->GetPropertyValue_InContainer(Row.Memory));
						}
					}
					for (int32 Index = 0; Index < OldTexts.Num() && Index < NewTextValues.Num(); ++Index)
					{
						if (!OldTexts[Index].IsEmpty())
						{
							UnlockContext.TextMap.Add(OldTexts[Index], NewTextValues[Index]);
						}
					}
					FPlannedRow UnlockPlan = Plan;
					UnlockPlan.Values.Reset();
					UnlockContext.Plan = &UnlockPlan;
					UnlockContext.bLinked = true;
					RewriteStruct(UnlockStruct, Unlock.Memory, UnlockContext, true);
					if (Settings->bUnlockNewItems)
					{
						MakeUnlocked(UnlockStruct, Unlock.Memory);
					}

					const FString OldName = Plan.UnlockTemplateRow.ToString();
					const FString OldIdText = FString::Printf(TEXT("%d"), OldId);
					FString Wanted = OldName.EndsWith(OldIdText)
						? OldName.LeftChop(OldIdText.Len()) + FString::Printf(TEXT("%lld"), NewId)
						: FString::Printf(TEXT("Item%lld"), NewId);
					const FName UnlockName = UniqueName(Wanted, { UnlockTable, UnlockNew });
					AddRow(UnlockNew, UnlockName, Unlock, ToSave);
					CastField<FIntProperty>(Struct->FindPropertyByName(Plan.UnlockField))->SetPropertyValue_InContainer(Row.Memory, int32(NewId));
					UnlockNote = FString::Printf(TEXT(", unlock item %s in %s"), *UnlockName.ToString(), *UnlockNew->GetName());
				}
			}
		}

		AddRow(NewTable, RowName, Row, ToSave);
		++Added;
		OutLog.Add(FString::Printf(TEXT("%s: added row %s (copy of %s, %s -> %s)%s."), *NewTable->GetName(), *RowName.ToString(),
			*Plan.TemplateRow.ToString(), Plan.From.IsEmpty() ? TEXT("?") : *Plan.From, *Plan.To, *UnlockNote));
		if (Context.Missing.Num() > 0)
		{
			OutLog.Add(FString::Printf(TEXT("  no cue asset for %s, so %s left empty."), *FString::Join(Context.Missing, TEXT(", ")),
				Context.Missing.Num() == 1 ? TEXT("that field is") : TEXT("those fields are")));
		}
	}
	return Added;
}

void AcbTableRows::RefreshComposites(const TArray<UPackage*>& Changed)
{
	FArrayProperty* ParentsProperty = FindFProperty<FArrayProperty>(UCompositeDataTable::StaticClass(), TEXT("ParentTables"));
	if (!ParentsProperty || Changed.Num() == 0)
	{
		return;
	}
	for (TObjectIterator<UCompositeDataTable> It; It; ++It)
	{
		UCompositeDataTable* Composite = *It;
		if (Composite->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}
		TArray<const UDataTable*> Sources;
		AcbTableRowsLocal::SourceTables(Composite, Sources);
		const bool bAffected = Sources.ContainsByPredicate([&Changed](const UDataTable* Source) { return Changed.Contains(Source->GetOutermost()); });
		if (bAffected)
		{
			FPropertyChangedEvent Event(ParentsProperty);
			Composite->PostEditChangeProperty(Event);
		}
	}
}

void AcbTableRows::GetSourceTables(const UDataTable* Table, TArray<const UDataTable*>& OutTables)
{
	AcbTableRowsLocal::SourceTables(Table, OutTables);
}
