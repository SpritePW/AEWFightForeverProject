#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FChunkNameItem
{
	int32 Chunk = 0;
	FString Name;
	FString Zip;
	TArray<FString> Labels;
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
	void ProcessNow();
	void AddLog(const FString& Line);
	FText GetStatusText() const;
	FText GetPreviewText() const;
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FChunkNameItem> Item, const TSharedRef<STableViewBase>& Owner);
	TSharedRef<ITableRow> OnGenerateAssetRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner);

	TArray<TSharedPtr<FChunkNameItem>> AllItems;
	TArray<TSharedPtr<FChunkNameItem>> VisibleItems;
	TSharedPtr<SListView<TSharedPtr<FChunkNameItem>>> ListView;
	TSharedPtr<FChunkNameItem> Selected;
	TArray<TSharedPtr<FString>> SelectedAssets;
	TSharedPtr<SListView<TSharedPtr<FString>>> AssetView;
	FString FilterText;
	FString OutputFolder;

	bool bRefreshPending = false;
	double RefreshAt = 0.0;
	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;
	FDelegateHandle RenamedHandle;

	FString LogText;
};
