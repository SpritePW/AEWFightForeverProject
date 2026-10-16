/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : CriWareSequencer Module Implementation
 * File     : CriWareSequencerEditorStyle.h
 *
 ****************************************************************************/

#pragma once

#include "Misc/Optional.h"
#include "Styling/SlateStyle.h"


class FCriWareSequencerEditorStyle
	: public FSlateStyleSet
{
public:

	/** Default constructor. */
	FCriWareSequencerEditorStyle();

	/** Destructor. */
	~FCriWareSequencerEditorStyle();

public:

	static FCriWareSequencerEditorStyle& Get()
	{
		if (!Singleton.IsSet())
		{
			Singleton.Emplace();
		}

		return Singleton.GetValue();
	}

	static void Destroy()
	{
		Singleton.Reset();
	}

private:

	static TOptional<FCriWareSequencerEditorStyle> Singleton;
};
