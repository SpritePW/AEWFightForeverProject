#include "TableRowNameUtilityFunction.h"

UTableRowNameUtilityFunction::UTableRowNameUtilityFunction(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer) {
}

FTableRowName UTableRowNameUtilityFunction::ToTableRowName(const FName& RowName) {
    FTableRowName x;
    x.Name = RowName;
    return x;
}

FName UTableRowNameUtilityFunction::ToFName(const FTableRowName& TableRowName) {
    return TableRowName.Name;
}

FDataTableRowHandle UTableRowNameUtilityFunction::ToDataTableRowHandle(const FTableRowName& TableRowName) {
    FDataTableRowHandle x;
    x.RowName = TableRowName.Name;
    return x;
}

bool UTableRowNameUtilityFunction::IsEqual_FName(const FTableRowName& TableRowName, const FName& Name) {
    return TableRowName.Name == Name;
}

void UTableRowNameUtilityFunction::ConvertToNameSet(const TSet<FTableRowName>& TableRowNameSet, TSet<FName>& NameSet) {
    for (auto& Elem : TableRowNameSet)
    {
        NameSet.Emplace(Elem.Name);
    }
    return;
}


