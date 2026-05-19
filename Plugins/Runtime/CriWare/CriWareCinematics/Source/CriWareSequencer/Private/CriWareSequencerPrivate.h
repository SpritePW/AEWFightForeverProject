/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2017-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : CriWareSequencer
 * File     : CriWareSequencerPrivate.h
 *
 ****************************************************************************/

#pragma once

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
