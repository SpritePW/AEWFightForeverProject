#include "AEWToolUI.h"

#include "EditorStyleSet.h"
#include "IDetailsView.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace AEWToolUI
{
	TSharedRef<SWidget> MakeToolbar(TSharedRef<SWidget> Buttons, const TAttribute<FText>& Status)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				Buttons
			];
		if (Status.IsBound() || !Status.Get().IsEmpty())
		{
			Box->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(Status)
			];
		}
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(6.f, 4.f))
			[
				Box
			];
	}

	TSharedRef<SWidget> MakeSidePanel(TSharedRef<SWidget> Content)
	{
		return SNew(SScrollBox)
			+ SScrollBox::Slot().Padding(6.f, 0.f, 2.f, 0.f)
			[
				Content
			];
	}

	TSharedRef<SWidget> MakeBody(TSharedPtr<SWidget> Left, TSharedRef<SWidget> Right, const FString* LogText, float LeftFraction)
	{
		TSharedRef<SWidget> Top = Right;
		if (Left.IsValid())
		{
			Top = SNew(SSplitter)
				+ SSplitter::Slot().Value(LeftFraction)
				[
					Left.ToSharedRef()
				]
				+ SSplitter::Slot().Value(1.f - LeftFraction)
				[
					Right
				];
		}
		return SNew(SBox).Padding(6.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot().Value(0.82f)
				[
					Top
				]
				+ SSplitter::Slot().Value(0.18f)
				[
					MakeLog(LogText)
				]
			];
	}

	TSharedRef<SWidget> MakeSection(const FText& Title, TSharedRef<SWidget> Content, bool bCollapsed)
	{
		return SNew(SExpandableArea)
			.InitiallyCollapsed(bCollapsed)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.AreaTitle(Title)
			.AreaTitleFont(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
			.Padding(FMargin(8.f, 4.f, 8.f, 8.f))
			.BodyContent()
			[
				Content
			];
	}

	TSharedRef<SWidget> MakeRow(const FText& Label, TSharedRef<SWidget> Content, const FText& Tooltip, float LabelWidth)
	{
		return SNew(SHorizontalBox)
			.ToolTipText(Tooltip)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 2, 8, 2)
			[
				SNew(SBox).WidthOverride(LabelWidth)
				[
					SNew(STextBlock).Text(Label)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0, 2)
			[
				Content
			];
	}

	TSharedRef<SWidget> MakeTextField(FString* Target, const FText& Hint)
	{
		return SNew(SEditableTextBox)
			.HintText(Hint)
			.Text_Lambda([Target]() { return FText::FromString(*Target); })
			.OnTextChanged_Lambda([Target](const FText& T) { *Target = T.ToString(); });
	}

	TSharedRef<SWidget> MakeSettingsView(UObject* Settings)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs Args;
		Args.bAllowSearch = true;
		Args.bHideSelectionTip = true;
		Args.bShowOptions = false;
		Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		TSharedRef<IDetailsView> View = PropertyEditor.CreateDetailView(Args);
		View->SetObject(Settings);
		return SNew(SBox).MaxDesiredHeight(500.f)
			[
				View
			];
	}

	TSharedRef<SWidget> MakeLog(const FString* LogText)
	{
		return SNew(SMultiLineEditableTextBox)
			.IsReadOnly(true)
			.AutoWrapText(true)
			.Text_Lambda([LogText]() { return FText::FromString(LogText ? *LogText : FString()); });
	}

	static TSharedRef<SWidget> Button(const FText& Text, const FText& Tooltip, TFunction<void()> OnClicked, const TAttribute<bool>& IsEnabled, const TCHAR* Style)
	{
		TSharedRef<SButton> B = SNew(SButton)
			.Text(Text)
			.ToolTipText(Tooltip)
			.IsEnabled(IsEnabled)
			.OnClicked_Lambda([OnClicked]() { if (OnClicked) { OnClicked(); } return FReply::Handled(); });
		if (Style)
		{
			B->SetButtonStyle(&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>(Style));
			B->SetForegroundColor(FLinearColor::White);
		}
		return B;
	}

	TSharedRef<SWidget> MakeButton(const FText& Text, const FText& Tooltip, TFunction<void()> OnClicked, const TAttribute<bool>& IsEnabled)
	{
		return Button(Text, Tooltip, MoveTemp(OnClicked), IsEnabled, nullptr);
	}

	TSharedRef<SWidget> MakePrimaryButton(const FText& Text, const FText& Tooltip, TFunction<void()> OnClicked, const TAttribute<bool>& IsEnabled)
	{
		return Button(Text, Tooltip, MoveTemp(OnClicked), IsEnabled, TEXT("FlatButton.Success"));
	}

	TSharedRef<SWidget> MakeDangerButton(const FText& Text, const FText& Tooltip, TFunction<void()> OnClicked, const TAttribute<bool>& IsEnabled)
	{
		return Button(Text, Tooltip, MoveTemp(OnClicked), IsEnabled, TEXT("FlatButton.Danger"));
	}

	void OpenSettings(FName SectionName)
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->ShowViewer("Project", "Plugins", SectionName);
		}
	}
}
