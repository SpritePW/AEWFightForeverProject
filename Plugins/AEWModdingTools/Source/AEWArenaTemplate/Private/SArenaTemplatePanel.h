#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "AEWArenaTools.h"

class SCheckBox;
class FArenaLogCapture;

struct FArenaTemplateItem
{
	FString Package;
	FString Name;
	FString Folder;
};

class SArenaTemplatePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArenaTemplatePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArenaTemplatePanel();

private:
	void Refresh();
	void ApplyFilter();
	void Create();
	void AddLog(const FString& Line);
	bool IsRegistering() const;
	bool IdProblem() const;
	FString MapName() const;
	FText GetStatusText() const;
	FText GetPreviewText() const;
	FText GetArenaListText() const;
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArenaTemplateItem> Item, const TSharedRef<STableViewBase>& Owner);

	TArray<TSharedPtr<FArenaTemplateItem>> AllTemplates;
	TArray<TSharedPtr<FArenaTemplateItem>> VisibleTemplates;
	TSharedPtr<SListView<TSharedPtr<FArenaTemplateItem>>> ListView;
	TSharedPtr<FArenaTemplateItem> Selected;
	FString FilterText;

	TArray<FAEWArenaListEntry> Arenas;
	int32 NextId = -1;

	FString Name = TEXT("MyArena");
	FString DisplayName;
	FString ParentFolder;
	int32 ArenaId = 0;
	int32 ChunkId = 100;
	bool bCreateLabel = true;
	bool bAddMaps = true;
	bool bRegister = true;

	mutable int32 CheckedId = -1;
	mutable bool bCheckedFree = true;

	FString LogText;
	TSharedPtr<FArenaLogCapture> LogCapture;
};
