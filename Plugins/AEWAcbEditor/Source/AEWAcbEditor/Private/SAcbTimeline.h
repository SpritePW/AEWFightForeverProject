#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

enum class EAcbTimelineMarker : uint8
{
	None,
	Start,
	LoopStart,
	LoopEnd
};

class SAcbTimeline : public SLeafWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSeek, int64);
	DECLARE_DELEGATE_TwoParams(FOnMarkerMoved, EAcbTimelineMarker, int64);

	SLATE_BEGIN_ARGS(SAcbTimeline)
		: _NumFrames(0)
		, _SampleRate(0)
		, _Position(-1)
		, _Start(-1)
		, _LoopStart(-1)
		, _LoopEnd(-1)
		, _Loop(false)
		, _CanSetStart(true)
		, _Peaks(nullptr)
	{}
		SLATE_ATTRIBUTE(int64, NumFrames)
		SLATE_ATTRIBUTE(int32, SampleRate)
		SLATE_ATTRIBUTE(int64, Position)
		SLATE_ATTRIBUTE(int64, Start)
		SLATE_ATTRIBUTE(int64, LoopStart)
		SLATE_ATTRIBUTE(int64, LoopEnd)
		SLATE_ATTRIBUTE(bool, Loop)
		SLATE_ATTRIBUTE(bool, CanSetStart)
		SLATE_ARGUMENT(const TArray<float>*, Peaks)
		SLATE_EVENT(FOnSeek, OnSeek)
		SLATE_EVENT(FOnMarkerMoved, OnMarkerMoved)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static FString FormatTime(int64 Frames, int32 SampleRate);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	float FrameToX(int64 Frame, float Width) const;
	int64 XToFrame(float X, float Width) const;
	EAcbTimelineMarker HitMarker(float X, float Width) const;
	int64 ClampMarker(EAcbTimelineMarker Marker, int64 Frame) const;

	TAttribute<int64> NumFrames;
	TAttribute<int32> SampleRate;
	TAttribute<int64> Position;
	TAttribute<int64> Start;
	TAttribute<int64> LoopStart;
	TAttribute<int64> LoopEnd;
	TAttribute<bool> Loop;
	TAttribute<bool> CanSetStart;
	const TArray<float>* Peaks = nullptr;
	FOnSeek OnSeek;
	FOnMarkerMoved OnMarkerMoved;

	EAcbTimelineMarker Dragging = EAcbTimelineMarker::None;
	bool bScrubbing = false;
	int64 ScrubFrame = -1;
	float HoverX = -1.f;
};
