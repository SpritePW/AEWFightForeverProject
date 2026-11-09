#include "AEWPackaging.h"
#include "AssetRegistryModule.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EditorStyleSet.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AEWCookSelection"

namespace
{
	struct FCookChoice
	{
		UPrimaryAssetLabel* Label = nullptr;
		FString Name;
		FString Folder;
		int32 Chunk = 0;
		bool bCook = true;
		bool bWas = true;
	};
}

namespace AEWPackaging
{
	int32 ChooseLabelsToCook()
	{
		const FText Title = LOCTEXT("Title", "Choose What To Cook");
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		AR.GetAssetsByClass(UPrimaryAssetLabel::StaticClass()->GetFName(), Assets, true);
		TArray<FCookChoice> Choices;
		for (const FAssetData& A : Assets)
		{
			const FString Package = A.PackageName.ToString();
			if (!Package.StartsWith(TEXT("/Game/")))
			{
				continue;
			}
			UPrimaryAssetLabel* Label = Cast<UPrimaryAssetLabel>(A.GetAsset());
			if (!Label)
			{
				continue;
			}
			FCookChoice C;
			C.Label = Label;
			C.Name = A.AssetName.ToString();
			C.Folder = FPackageName::GetLongPackagePath(Package);
			C.Chunk = Label->Rules.ChunkId;
			C.bCook = Label->Rules.CookRule != EPrimaryAssetCookRule::NeverCook && Label->Rules.CookRule != EPrimaryAssetCookRule::DevelopmentCook;
			C.bWas = C.bCook;
			Choices.Add(C);
		}
		if (Choices.Num() == 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("None", "There are no labels in the project yet."), &Title);
			return 0;
		}
		Choices.Sort([](const FCookChoice& A, const FCookChoice& B) { return A.Chunk != B.Chunk ? A.Chunk < B.Chunk : A.Name < B.Name; });

		bool bOk = false;
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(Title)
			.ClientSize(FVector2D(640.f, 520.f))
			.SupportsMinimize(false)
			.SupportsMaximize(false);
		TWeakPtr<SWindow> WeakWindow = Window;
		TSharedRef<SScrollBox> List = SNew(SScrollBox);
		for (FCookChoice& C : Choices)
		{
			FCookChoice* Ptr = &C;
			List->AddSlot().Padding(2.f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([Ptr]() { return Ptr->bCook ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Ptr](ECheckBoxState State) { Ptr->bCook = State == ECheckBoxState::Checked; })
				.ToolTipText(FText::FromString(Ptr->Folder))
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s   (pakchunk%d)   %s"), *Ptr->Name, Ptr->Chunk, *Ptr->Folder)))
				]
			];
		}
		auto SetAll = [&Choices](bool bValue)
		{
			for (FCookChoice& C : Choices)
			{
				C.bCook = bValue;
			}
			return FReply::Handled();
		};
		bool* OkPtr = &bOk;
		Window->SetContent(
			SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(12.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Help", "Ticked labels are cooked and packaged. Unticked ones are left out."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SButton).Text(LOCTEXT("All", "Tick all")).OnClicked_Lambda([SetAll]() { return SetAll(true); })
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).Text(LOCTEXT("NoneButton", "Untick all")).OnClicked_Lambda([SetAll]() { return SetAll(false); })
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SBorder).Padding(4.f)[ List ]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 10.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SButton).Text(LOCTEXT("Apply", "Apply")).OnClicked_Lambda([OkPtr, WeakWindow]()
						{
							*OkPtr = true;
							if (TSharedPtr<SWindow> W = WeakWindow.Pin())
							{
								W->RequestDestroyWindow();
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).Text(LOCTEXT("Cancel", "Cancel")).OnClicked_Lambda([WeakWindow]()
						{
							if (TSharedPtr<SWindow> W = WeakWindow.Pin())
							{
								W->RequestDestroyWindow();
							}
							return FReply::Handled();
						})
					]
				]
			]);
		GEditor->EditorAddModalWindow(Window);
		if (!bOk)
		{
			return 0;
		}
		TArray<UPackage*> ToSave;
		TArray<FString> On;
		TArray<FString> Off;
		for (FCookChoice& C : Choices)
		{
			if (C.bCook == C.bWas)
			{
				continue;
			}
			C.Label->Modify();
			C.Label->Rules.CookRule = C.bCook ? EPrimaryAssetCookRule::AlwaysCook : EPrimaryAssetCookRule::DevelopmentCook;
			C.Label->MarkPackageDirty();
			ToSave.Add(C.Label->GetOutermost());
			(C.bCook ? On : Off).Add(C.Name);
		}
		if (ToSave.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
		}
		TArray<FString> Lines;
		if (On.Num() > 0)
		{
			Lines.Add(TEXT("Now cooked: ") + FString::Join(On, TEXT(", ")));
		}
		if (Off.Num() > 0)
		{
			Lines.Add(TEXT("No longer cooked: ") + FString::Join(Off, TEXT(", ")));
		}
		if (Lines.Num() == 0)
		{
			Lines.Add(TEXT("Nothing changed."));
		}
		else
		{
			Lines.Add(TEXT("The cook settings update by themselves in a moment."));
		}
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Join(Lines, TEXT("\n"))), &Title);
		return ToSave.Num();
	}
}

#undef LOCTEXT_NAMESPACE
