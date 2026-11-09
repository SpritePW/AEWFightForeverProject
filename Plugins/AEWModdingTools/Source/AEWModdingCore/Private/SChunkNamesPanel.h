#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UPrimaryAssetLabel;

struct FChunkNameItem
{
	int32 Chunk = 0;
	FString Name;
	FString Zip;
	TArray<FString> Labels;
	TArray<TWeakObjectPtr<UPrimaryAssetLabel>> LabelObjects;
	TArray<FString> Assets;
};

class SChunkNamesPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChunkNamesPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SChunkNamesPanel();
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	static const FName TabName;

private:
	void Refresh();
	void QueueRefresh();
	void OnAssetChanged(const struct FAssetData& Asset);
	void OnAssetRenamed(const struct FAssetData& Asset, const FString& OldPath);
	void ApplyFilter();
	void Select(TSharedPtr<FChunkNameItem> Item);
	void Save();
	void BrowseFolder();
	void AddLog(const FString& Line);
	FText GetStatusText() const;
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FChunkNameItem> Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> OnGenerateAssetRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> OnGenerateLabelRow(TSharedPtr<TWeakObjectPtr<UPrimaryAssetLabel>> Item, const TSharedRef<STableViewBase>& Owner);

	TArray<TSharedPtr<FChunkNameItem>> AllItems;
	TArray<TSharedPtr<FChunkNameItem>> VisibleItems;
	TSharedPtr<SListView<TSharedPtr<FChunkNameItem>>> ListView;
	TSharedPtr<FChunkNameItem> Selected;
	TArray<TSharedPtr<FString>> SelectedAssets;
	TSharedPtr<SListView<TSharedPtr<FString>>> AssetView;
	TArray<TSharedPtr<TWeakObjectPtr<UPrimaryAssetLabel>>> SelectedLabels;
	TSharedPtr<SListView<TSharedPtr<TWeakObjectPtr<UPrimaryAssetLabel>>>> LabelView;
	FString FilterText;
	FString OutputFolder;

	bool bRefreshPending = false;
	double RefreshAt = 0.0;
	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;
	FDelegateHandle RenamedHandle;

	FString LogText;
};
