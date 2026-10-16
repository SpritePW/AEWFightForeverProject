/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : CriWareSequencer Module Implementation
 * File     : CriWareSequencer.cpp
 *
 ****************************************************************************/

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ISequencer.h"
#include "ISequencerModule.h"

#include "CriWareSequencerPrivate.h"
#include "ICriWareSequencer.h"
#include "CriWareSequencerEditorStyle.h"
#include "AtomTrackEditor.h"
#if !defined(CRIWARE_UE4_LE)	/* <cri_delete_if_LE> */
#include "ManaTrackEditor.h"
#endif /* </cri_delete_if_LE> */



/**
 * CriWareSequencer module implementation (private)
 */
class FCriWareSequencerModule : public ICriWareSequencer
{
public:
	/** IModuleInterface */
	virtual void StartupModule() override
	{
		FCriWareSequencerEditorStyle::Get();

		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
		AtomTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FAtomTrackEditor::CreateTrackEditor));
#if !defined(CRIWARE_UE4_LE)	/* <cri_delete_if_LE> */
		ManaTrackCreateEditorHandle = SequencerModule.RegisterPropertyTrackEditor<FManaTrackEditor>();
#endif /* </cri_delete_if_LE> */
	}

	virtual void ShutdownModule() override
	{
		FCriWareSequencerEditorStyle::Destroy();

		ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer");
		if (SequencerModulePtr)
		{
			SequencerModulePtr->UnRegisterTrackEditor(AtomTrackCreateEditorHandle);
#if !defined(CRIWARE_UE4_LE)	/* <cri_delete_if_LE> */
			SequencerModulePtr->UnRegisterTrackEditor(ManaTrackCreateEditorHandle);
#endif /* </cri_delete_if_LE> */
		}
	}

private:
	/** Registered delegate handles */
	FDelegateHandle AtomTrackCreateEditorHandle;
#if !defined(CRIWARE_UE4_LE)	/* <cri_delete_if_LE> */
	FDelegateHandle ManaTrackCreateEditorHandle;
#endif /* </cri_delete_if_LE> */
};

IMPLEMENT_MODULE(FCriWareSequencerModule, CriWareSequencer);
