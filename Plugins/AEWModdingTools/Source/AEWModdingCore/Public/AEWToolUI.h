#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Input/Reply.h"
#include "Misc/Attribute.h"

class UObject;

namespace AEWToolUI
{
	AEWMODDINGCORE_API TSharedRef<SWidget> MakeToolbar(TSharedRef<SWidget> Buttons, const TAttribute<FText>& Status = TAttribute<FText>());

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeBody(TSharedPtr<SWidget> Left, TSharedRef<SWidget> Right, const FString* LogText, float LeftFraction = 0.58f);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeSidePanel(TSharedRef<SWidget> Content);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeSection(const FText& Title, TSharedRef<SWidget> Content, bool bCollapsed = false);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Content, const FText& Tooltip = FText::GetEmpty(), float LabelWidth = 100.f);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeTextField(FString* Target, const FText& Hint);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeSettingsView(UObject* Settings);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeLog(const FString* LogText);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeButton(const FText& Text, const FText& Tooltip, TFunction<void()> OnClicked, const TAttribute<bool>& IsEnabled = true);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakePrimaryButton(const FText& Text, const FText& Tooltip, TFunction<void()> OnClicked, const TAttribute<bool>& IsEnabled = true);

	AEWMODDINGCORE_API TSharedRef<SWidget> MakeDangerButton(const FText& Text, const FText& Tooltip, TFunction<void()> OnClicked, const TAttribute<bool>& IsEnabled = true);

	AEWMODDINGCORE_API void OpenSettings(FName SectionName);
}
