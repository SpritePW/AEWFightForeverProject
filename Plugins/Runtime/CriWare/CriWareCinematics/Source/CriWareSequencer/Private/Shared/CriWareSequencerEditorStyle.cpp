/****************************************************************************
 *
 * CRI Middleware SDK
 *
 * Copyright (c) 2021 CRI Middleware Co., Ltd.
 *
 * Library  : CRIWARE plugin for Unreal Engine 4
 * Module   : CriWareSequencer Module Implementation
 * File     : CriWareSequencerEditorStyle.cpp
 *
 ****************************************************************************/

#include "CriWareSequencerEditorStyle.h"

#include "Math/Vector2D.h"
#include "Styling/SlateStyleRegistry.h"


/* Static initialization
 *****************************************************************************/

TOptional<FCriWareSequencerEditorStyle> FCriWareSequencerEditorStyle::Singleton;


/* FCriWareSequencerEditorStyle structors
 *****************************************************************************/

FCriWareSequencerEditorStyle::FCriWareSequencerEditorStyle()
	: FSlateStyleSet("CriWareSequencerEditorStyle")
{
	const FVector2D Icon16x16(16.f, 16.f);
	const FVector2D Icon64x64(64.f, 64.f);


	FString PluginPath = TEXT("Runtime/CriWare/CriWareCinematics");
	FString CorePath = FPaths::Combine(*(FPaths::ProjectPluginsDir()), PluginPath);
	if (!FPaths::DirectoryExists(CorePath)) {
		CorePath = FPaths::Combine(*(FPaths::EnginePluginsDir()), PluginPath);
	}
	SetContentRoot(CorePath / TEXT("Content/Icons"));
	SetCoreContentRoot(CorePath);


	Set("ClassIcon.ManaPlane", new FSlateImageBrush(RootToContentDir(TEXT("ManaTexture_16x.png")), Icon16x16));
	Set("ClassThumbnail.ManaPlane", new FSlateImageBrush(RootToContentDir(TEXT("ManaTexture_64x.png")), Icon64x64));

	Set("ClassIcon.ManaPlaneComponent", new FSlateImageBrush(RootToContentDir(TEXT("ManaTexture_16x.png")), Icon16x16));
	Set("ClassThumbnail.ManaPlaneComponent", new FSlateImageBrush(RootToContentDir(TEXT("ManaTexture_64x.png")), Icon64x64));

	Set("ClassIcon.AtomPlane", new FSlateImageBrush(RootToContentDir(TEXT("AtomVoiceOn_16x.png")), Icon16x16));
	Set("ClassThumbnail.AtomPlane", new FSlateImageBrush(RootToContentDir(TEXT("AtomVoiceOn_64x.png")), Icon64x64));

	Set("ClassIcon.AtomPlaneComponent", new FSlateImageBrush(RootToContentDir(TEXT("AtomVoiceOn_16x.png")), Icon16x16));
	Set("ClassThumbnail.AtomPlaneComponent", new FSlateImageBrush(RootToContentDir(TEXT("AtomVoiceOn_64x.png")), Icon64x64));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}


FCriWareSequencerEditorStyle::~FCriWareSequencerEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
