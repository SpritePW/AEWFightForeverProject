/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2013-2018 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Precompiled header for CriWareEditor
 * File     : CriWareEditorPrivatePCH.h
 *
 ****************************************************************************/

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 0
#include "AssetRegistry/AssetData.h"
#else
#include "AssetData.h"
#endif
#include "AssetToolsModule.h"
#include "Misc/TextFilter.h"
#include "Internationalization/Internationalization.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCriWareEditor, Log, All);

#include "CriWareApi.h"
#include "CriWareLocalization.h"
