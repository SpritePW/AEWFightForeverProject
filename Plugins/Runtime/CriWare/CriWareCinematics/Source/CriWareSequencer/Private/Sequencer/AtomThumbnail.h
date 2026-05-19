/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2018-2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : Atom Thumbnail
 * File     : AtomThumbnail.h
 *
 ****************************************************************************/

#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"
#include "MovieSceneSection.h"


class FAtomThumbnail
	: public ISlateViewport
	, public TSharedFromThis<FAtomThumbnail>
{
public:
	FAtomThumbnail(UMovieSceneSection& InSection, TRange<float> DrawRange, int InTextureSize, const FLinearColor& BaseColor, float DisplayScale);
	~FAtomThumbnail();

	/* ISlateViewport interface */
	virtual FIntPoint GetSize() const override;
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual bool RequiresVsync() const override;

	/** Returns whether this thumbnail has a texture to render */
	virtual bool ShouldRender() const { return TextureSize > 0; }

private:
	/** Generates the waveform preview and dumps it out to the OutBuffer */
	void GenerateWaveformPreview(TArray<uint8>& OutBuffer, TRange<float> DrawRange, float DisplayScale);

private:
	/** The section we are visualizing */
	UMovieSceneSection& Section;

	/** The Texture RHI that holds the thumbnail */
	class FSlateTexture2DRHIRef* Texture;

	/** Size of the texture */
	int32 TextureSize;

	/** Waveform colors */
	FLinearColor FillColor;
};
