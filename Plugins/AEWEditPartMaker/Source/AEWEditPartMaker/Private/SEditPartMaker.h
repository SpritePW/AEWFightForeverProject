#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "EditPartMakerCore.h"

class STextComboBox;
class FAssetThumbnailPool;
struct FAssetData;

class SEditPartMaker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditPartMaker) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetMesh(USkeletalMesh* Mesh);

private:
	void ReloadTables();
	void SuggestIds(bool bPartsID = true, bool bUnlockID = true);
	void CheckOnly();
	void DoMake();
	void UseContentBrowserSelection();
	void AddLog(const FString& Line);
	void OpenSettings();
	void OnSettingsChanged(UObject* Settings, FPropertyChangedEvent& Event);

	TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Content, const FText& Tooltip = FText::GetEmpty());
	TSharedRef<SWidget> MakeTextField(FString* Target, const FText& Hint);
	TSharedRef<STextComboBox> MakeCombo(TArray<TSharedPtr<FString>>* Options, FString* Target);
	static void FillOptions(TArray<TSharedPtr<FString>>& Out, const TArray<FString>& Values, const TArray<FString>& Leading = TArray<FString>());
	static void Select(const TSharedPtr<STextComboBox>& Combo, const TArray<TSharedPtr<FString>>& Options, const FString& Value, FString* Target);

	FEditPartMakerTables Tables;
	FPartRequest Request;

	TArray<TSharedPtr<FString>> SlotOptions;
	TArray<TSharedPtr<FString>> GenderOptions;
	TArray<TSharedPtr<FString>> FaceOptions;
	TArray<TSharedPtr<FString>> AttireOptions;
	TArray<TSharedPtr<FString>> WACategoryOptions;
	TArray<TSharedPtr<FString>> CompatibleOptions;

	TSharedPtr<STextComboBox> SlotCombo;
	TSharedPtr<STextComboBox> GenderCombo;
	TSharedPtr<STextComboBox> FaceCombo;
	TSharedPtr<STextComboBox> AttireCombo;
	TSharedPtr<STextComboBox> WACategoryCombo;
	TSharedPtr<STextComboBox> CompatibleCombo;

	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	FString LogText;
};
