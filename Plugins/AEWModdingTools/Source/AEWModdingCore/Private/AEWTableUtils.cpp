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

	void AddToolEntry(FName EntryName, const FText& Label, const FText& ToolTip, const FUIAction& Action)
	{
		if (UToolMenu* Root = RootMenu())
		{
			Root->FindOrAddSection("Tools").AddMenuEntry(EntryName, Label, ToolTip, FSlateIcon(), Action);
		}
	}

	static UToolMenu* SubMenuOf(UToolMenu* Parent, const FString& ParentName, FName Group, const FText& GroupLabel)
	{
		UToolMenus* Menus = UToolMenus::Get();
		if (!Parent)
		{
			return nullptr;
		}
		FToolMenuSection& Groups = Parent->FindOrAddSection("Groups");
		if (!Groups.FindEntry(Group))
		{
			Groups.AddSubMenu(Group, GroupLabel, FText::GetEmpty(), FNewToolMenuChoice());
		}
		const FName GroupMenu(*FString::Printf(TEXT("%s.%s"), *ParentName, *Group.ToString()));
		if (!Menus->IsMenuRegistered(GroupMenu))
		{
			Menus->RegisterMenu(GroupMenu);
		}
		return Menus->ExtendMenu(GroupMenu);
	}

	void AddGroupedToolEntry(FName Group, const FText& GroupLabel, FName EntryName, const FText& Label, const FText& ToolTip, const FUIAction& Action)
	{
		if (UToolMenu* Sub = SubMenuOf(RootMenu(), RootMenuName, Group, GroupLabel))
		{
			Sub->FindOrAddSection("Tools").AddMenuEntry(EntryName, Label, ToolTip, FSlateIcon(), Action);
		}
	}

	void AddSubGroupedToolEntry(FName Group, const FText& GroupLabel, FName SubGroup, const FText& SubGroupLabel, FName EntryName, const FText& Label, const FText& ToolTip, const FUIAction& Action)
	{
		const FString GroupName = FString::Printf(TEXT("%s.%s"), RootMenuName, *Group.ToString());
		if (UToolMenu* Sub = SubMenuOf(SubMenuOf(RootMenu(), RootMenuName, Group, GroupLabel), GroupName, SubGroup, SubGroupLabel))
		{
			Sub->FindOrAddSection("Tools").AddMenuEntry(EntryName, Label, ToolTip, FSlateIcon(), Action);
		}
	}
}

#undef LOCTEXT_NAMESPACE
