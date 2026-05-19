/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Module Interface of CriWareSequencer
 * File     : ICriWareSequencer.h
 *
 ****************************************************************************/

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"		// For inline LoadModuleChecked()

/**
 * The public interface of the CriWareSequencer module
 */
class ICriWareSequencer : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to ICriWareSequencer
	 *
	 * @return Returns CriWareSequencer singleton instance, loading the module on demand if needed
	 */
	static inline ICriWareSequencer& Get()
	{
		return FModuleManager::LoadModuleChecked<ICriWareSequencer>("CriWareSequencer");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("CriWareSequencer");
	}
};
