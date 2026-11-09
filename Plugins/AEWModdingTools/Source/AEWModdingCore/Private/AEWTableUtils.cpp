#include "AEWTableUtils.h"

#include "DataTableEditorUtils.h"
#include "Engine/CompositeDataTable.h"
#include "Engine/DataTable.h"
#include "Misc/OutputDeviceNull.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AEWModdingCore"

namespace AEWTables
{
	UDataTable* Load(const FSoftObjectPath& Path)
	{
		return Path.IsValid() ? Cast<UDataTable>(Path.TryLoad()) : nullptr;
	}

	FProperty* FindField(const UStruct* Struct, const FString& Name)
	{
		if (!Struct || Name.IsEmpty())
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (Struct->GetAuthoredNameForField(*It).Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		const FString Prefix = Name + TEXT("_");
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool SetByText(const UStruct* Struct, void* Container, const FString& Name, const FString& Text, FString& OutError)
	{
		FProperty* Prop = FindField(Struct, Name);
		if (!Prop)
		{
			OutError = FString::Printf(TEXT("%s has no field '%s'"), Struct ? *Struct->GetName() : TEXT("(null)"), *Name);
			return false;
		}
		FOutputDeviceNull Null;
		if (!Prop->ImportText(*Text, Prop->ContainerPtrToValuePtr<void>(Container), PPF_None, nullptr, &Null))
		{
			OutError = FString::Printf(TEXT("%s.%s: could not set '%s'"), *Struct->GetName(), *Name, *Text);
			return false;
		}
		return true;
	}

	bool SetByText(const UStruct* Struct, void* Container, const FString& Name, const FString& Text)
	{
		FString Error;
		return SetByText(Struct, Container, Name, Text, Error);
	}

	FString GetAsText(const UStruct* Struct, const void* Container, const FString& Name)
	{
		FString Out;
		if (FProperty* Prop = FindField(Struct, Name))
		{
			Prop->ExportTextItem(Out, Prop->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
		}
		return Out;
	}

	UEnum* GetEnum(FProperty* Property, FNumericProperty*& OutUnderlying)
	{
		OutUnderlying = nullptr;
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

	static FNumericProperty* IntegerOf(FProperty* Prop)
	{
		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Prop))
		{
			return EnumProperty->GetUnderlyingProperty();
		}
		FNumericProperty* Num = CastField<FNumericProperty>(Prop);
		return (Num && Num->IsInteger()) ? Num : nullptr;
	}

	bool GetInt(const UStruct* Struct, const void* Container, const FString& Name, int64& Out)
	{
		FProperty* Prop = FindField(Struct, Name);
		if (FNumericProperty* Num = IntegerOf(Prop))
		{
			Out = Num->GetSignedIntPropertyValue(Prop->ContainerPtrToValuePtr<void>(Container));
			return true;
		}
		return false;
	}

	bool SetInt(const UStruct* Struct, void* Container, const FString& Name, int64 Value)
	{
		FProperty* Prop = FindField(Struct, Name);
		if (FNumericProperty* Num = IntegerOf(Prop))
		{
			Num->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<void>(Container), Value);
			return true;
		}
		return false;
	}

	TArray<UDataTable*> GetParents(const UDataTable* Table)
	{
		TArray<UDataTable*> Out;
		const UCompositeDataTable* Composite = Cast<UCompositeDataTable>(Table);
		FArrayProperty* ArrayProp = Composite ? FindFProperty<FArrayProperty>(UCompositeDataTable::StaticClass(), TEXT("ParentTables")) : nullptr;
		FObjectPropertyBase* Inner = ArrayProp ? CastField<FObjectPropertyBase>(ArrayProp->Inner) : nullptr;
		if (!Inner)
		{
			return Out;
		}
		FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Composite));
		for (int32 i = 0; i < Helper.Num(); ++i)
		{
			if (UDataTable* Parent = Cast<UDataTable>(Inner->GetObjectPropertyValue(Helper.GetRawPtr(i))))
			{
				Out.Add(Parent);
			}
		}
		return Out;
	}

	void GetSourceTables(const UDataTable* Table, TArray<const UDataTable*>& Out)
	{
		if (!Table || Out.Contains(Table))
		{
			return;
		}
		if (Cast<UCompositeDataTable>(Table))
		{
			for (UDataTable* Parent : GetParents(Table))
			{
				GetSourceTables(Parent, Out);
			}
			return;
		}
		Out.Add(Table);
	}

	void RefreshComposite(UDataTable* Table)
	{
		if (UCompositeDataTable* Composite = Cast<UCompositeDataTable>(Table))
		{
			if (FProperty* ParentsProp = FindFProperty<FProperty>(UCompositeDataTable::StaticClass(), TEXT("ParentTables")))
			{
				FPropertyChangedEvent Event(ParentsProp);
				static_cast<UObject*>(Composite)->PostEditChangeProperty(Event);
			}
			Composite->MarkPackageDirty();
		}
	}

	void MakeUnlocked(const UScriptStruct* Struct, void* Row)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FString Name = Struct->GetAuthoredNameForField(*It);
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

	FName UnlockRowName(int64 UnlockId)
	{
		return FName(*FString::Printf(TEXT("Item%lld"), UnlockId));
	}

	TSet<int64> CollectInts(const TArray<const UDataTable*>& Tables, const FString& Field)
	{
		TSet<int64> Out;
		for (const UDataTable* Table : Tables)
		{
			if (!Table || !Table->GetRowStruct())
			{
				continue;
			}
			for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
			{
				int64 Value = 0;
				if (GetInt(Table->GetRowStruct(), Row.Value, Field, Value))
				{
					Out.Add(Value);
				}
			}
		}
		return Out;
	}

	int64 FirstFree(const TSet<int64>& Used, int64 First, int64 Last)
	{
		for (int64 Value = First; Value <= Last; ++Value)
		{
			if (!Used.Contains(Value))
			{
				return Value;
			}
		}
		return -1;
	}

	FString Escape(const FString& In)
	{
		return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
	}

	FString InvText(const FString& In)
	{
		return FString::Printf(TEXT("INVTEXT(\"%s\")"), *Escape(In));
	}

	FRowBuffer::FRowBuffer(const UScriptStruct* InStruct, const void* Source)
		: Struct(InStruct)
	{
		Memory = static_cast<uint8*>(FMemory::Malloc(FMath::Max(1, Struct->GetStructureSize()), Struct->GetMinAlignment()));
		Struct->InitializeStruct(Memory);
		if (Source)
		{
			Struct->CopyScriptStruct(Memory, Source);
		}
	}

	FRowBuffer::~FRowBuffer()
	{
		Struct->DestroyStruct(Memory);
		FMemory::Free(Memory);
	}

	const FTableRowBase& FRowBuffer::AsRow() const
	{
		return *reinterpret_cast<const FTableRowBase*>(Memory);
	}

	void ReplaceRow(UDataTable* Table, FName RowName, const FRowBuffer& Row)
	{
		FDataTableEditorUtils::BroadcastPreChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
		Table->Modify();
		if (Table->GetRowMap().Contains(RowName))
		{
			Table->RemoveRow(RowName);
		}
		Table->AddRow(RowName, Row.AsRow());
		Table->MarkPackageDirty();
		FDataTableEditorUtils::BroadcastPostChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	}
}

namespace AEWMenus
{
	static const TCHAR* RootMenuName = TEXT("LevelEditor.MainMenu.Window.AEWModdingTools");

	static UToolMenu* RootMenu()
	{
		UToolMenus* Menus = UToolMenus::Get();
		if (UToolMenu* Window = Menus->ExtendMenu("LevelEditor.MainMenu.Window"))
		{
			FToolMenuSection& Section = Window->FindOrAddSection("AEWFightForever");
			Section.Label = LOCTEXT("SectionLabel", "AEW Fight Forever");
			if (!Section.FindEntry("AEWModdingTools"))
			{
				Section.AddSubMenu("AEWModdingTools", LOCTEXT("SubMenu", "AEW Modding Tools"),
					LOCTEXT("SubMenuTip", "ACB Editor, Edit Part Maker, Arena Maker"), FNewToolMenuChoice());
			}
		}
		const FName Root(RootMenuName);
		if (!Menus->IsMenuRegistered(Root))
		{
			Menus->RegisterMenu(Root);
		}
		return Menus->ExtendMenu(Root);
	}

	const FSection& ToolsSection()
	{
		static const FSection Section{ TEXT("Tools"), LOCTEXT("ToolsSection", "Tools"), 0 };
		return Section;
	}

	const FSection& ArenaSection()
	{
		static const FSection Section{ TEXT("Arena"), LOCTEXT("ArenaSection", "Arena"), 10 };
		return Section;
	}

	const FSection& PackagingSection()
	{
		static const FSection Section{ TEXT("Packaging"), LOCTEXT("PackagingSection", "Packaging"), 20 };
		return Section;
	}

	FName MenuName(const FString& Menu)
	{
		return Menu.IsEmpty() ? FName(RootMenuName) : FName(*FString::Printf(TEXT("%s.%s"), RootMenuName, *Menu));
	}

	struct FItem
	{
		FName Name;
		FText Label;
		FText Tip;
		FUIAction Action;
		FNewToolMenuDelegate Fill;
		int32 Kind = 0;
		int32 Order = 0;
	};

	struct FRegisteredSection
	{
		FSection Info;
		TArray<FItem> Items;
	};

	static TMap<FString, TArray<FRegisteredSection>>& Registry()
	{
		static TMap<FString, TArray<FRegisteredSection>> Menus;
		return Menus;
	}

	static UToolMenu* MenuFor(const FString& Menu)
	{
		if (!RootMenu())
		{
			return nullptr;
		}
		UToolMenus* Menus = UToolMenus::Get();
		const FName Name = MenuName(Menu);
		if (!Menus->IsMenuRegistered(Name))
		{
			Menus->RegisterMenu(Name);
		}
		return Menus->ExtendMenu(Name);
	}

	static void Rebuild(const FString& Menu)
	{
		UToolMenu* Target = MenuFor(Menu);
		if (!Target)
		{
			return;
		}
		TArray<FRegisteredSection>& Sections = Registry().FindOrAdd(Menu);
		Sections.StableSort([](const FRegisteredSection& A, const FRegisteredSection& B) { return A.Info.Order < B.Info.Order; });
		for (const FRegisteredSection& S : Sections)
		{
			Target->RemoveSection(S.Info.Name);
		}
		for (FRegisteredSection& S : Sections)
		{
			FToolMenuSection& Section = Target->AddSection(S.Info.Name, S.Info.Label);
			S.Items.StableSort([](const FItem& A, const FItem& B) { return A.Order < B.Order; });
			for (const FItem& I : S.Items)
			{
				if (I.Kind == 0)
				{
					Section.AddMenuEntry(I.Name, I.Label, I.Tip, FSlateIcon(), I.Action);
				}
				else if (I.Kind == 1)
				{
					Section.AddSubMenu(I.Name, I.Label, I.Tip, FNewToolMenuChoice());
				}
				else
				{
					Section.AddSubMenu(I.Name, I.Label, I.Tip, I.Fill);
				}
			}
		}
	}

	static void Add(const FString& Menu, const FSection& Section, FItem&& Item)
	{
		TArray<FRegisteredSection>& Sections = Registry().FindOrAdd(Menu);
		FRegisteredSection* Found = Sections.FindByPredicate([&Section](const FRegisteredSection& S) { return S.Info.Name == Section.Name; });
		if (!Found)
		{
			Found = &Sections.AddDefaulted_GetRef();
			Found->Info = Section;
		}
		const FName Name = Item.Name;
		Found->Items.RemoveAll([Name](const FItem& I) { return I.Name == Name; });
		Found->Items.Add(MoveTemp(Item));
		Rebuild(Menu);
	}

	void AddEntry(const FString& Menu, const FSection& Section, int32 Order, FName EntryName, const FText& Label, const FText& ToolTip, const FUIAction& Action)
	{
		FItem Item;
		Item.Name = EntryName;
		Item.Label = Label;
		Item.Tip = ToolTip;
		Item.Action = Action;
		Item.Order = Order;
		Add(Menu, Section, MoveTemp(Item));
	}

	void AddSubMenu(const FString& Menu, const FSection& Section, int32 Order, FName EntryName, const FText& Label, const FText& ToolTip)
	{
		MenuFor(Menu.IsEmpty() ? EntryName.ToString() : Menu + TEXT(".") + EntryName.ToString());
		FItem Item;
		Item.Name = EntryName;
		Item.Label = Label;
		Item.Tip = ToolTip;
		Item.Kind = 1;
		Item.Order = Order;
		Add(Menu, Section, MoveTemp(Item));
	}

	void AddDynamicSubMenu(const FString& Menu, const FSection& Section, int32 Order, FName EntryName, const FText& Label, const FText& ToolTip, const FNewToolMenuDelegate& Fill)
	{
		FItem Item;
		Item.Name = EntryName;
		Item.Label = Label;
		Item.Tip = ToolTip;
		Item.Fill = Fill;
		Item.Kind = 2;
		Item.Order = Order;
		Add(Menu, Section, MoveTemp(Item));
	}
}

#undef LOCTEXT_NAMESPACE
