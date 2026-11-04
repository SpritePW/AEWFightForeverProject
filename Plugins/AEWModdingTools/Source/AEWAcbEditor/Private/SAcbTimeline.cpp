#include "SAcbTimeline.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace AcbTimelineLocal
{
	constexpr float HandleHeight = 14.f;
	constexpr float TimeHeight = 14.f;
	constexpr float BarHeight = 56.f;
	constexpr float HitSlop = 6.f;

	const FLinearColor Background(0.015f, 0.015f, 0.015f, 1.f);
	const FLinearColor Wave(0.32f, 0.42f, 0.55f, 1.f);
	const FLinearColor WaveInLoop(0.45f, 0.62f, 0.85f, 1.f);
	const FLinearColor LoopFill(0.95f, 0.6f, 0.15f, 0.12f);
	const FLinearColor LoopColor(0.95f, 0.6f, 0.15f, 1.f);
	const FLinearColor StartColor(0.3f, 0.85f, 0.4f, 1.f);
	const FLinearColor PlayedFill(1.f, 1.f, 1.f, 0.05f);
	const FLinearColor PlayheadColor(1.f, 1.f, 1.f, 1.f);
	const FLinearColor ScrubColor(1.f, 1.f, 1.f, 0.5f);
	const FLinearColor Subdued(0.6f, 0.6f, 0.6f, 1.f);

	const FSlateBrush* White()
	{
		return FCoreStyle::Get().GetBrush("GenericWhiteBox");
	}

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, float X, float Y, float W, float H, const FLinearColor& Color)
	{
		if (W <= 0.f || H <= 0.f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2D(X, Y), FVector2D(W, H)), White(), ESlateDrawEffect::None, Color);
	}

	void Label(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FString& Text, float X, float Y, float MaxX, bool bCenter, const FLinearColor& Color)
	{
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
		const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
		float Left = bCenter ? X - Size.X * 0.5f : X;
		Left = FMath::Clamp(Left, 0.f, FMath::Max(MaxX - Size.X, 0.f));
		FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(FVector2D(Left, Y), Size), Text, Font, ESlateDrawEffect::None, Color);
	}
}

void SAcbTimeline::Construct(const FArguments& InArgs)
{
	NumFrames = InArgs._NumFrames;
	SampleRate = InArgs._SampleRate;
	Position = InArgs._Position;
	Start = InArgs._Start;
	LoopStart = InArgs._LoopStart;
	LoopEnd = InArgs._LoopEnd;
	Loop = InArgs._Loop;
	CanSetStart = InArgs._CanSetStart;
	Peaks = InArgs._Peaks;
	OnSeek = InArgs._OnSeek;
	OnMarkerMoved = InArgs._OnMarkerMoved;
}

FString SAcbTimeline::FormatTime(int64 Frames, int32 Rate)
{
	if (Rate <= 0 || Frames < 0)
	{
		return TEXT("-");
	}
	const double Seconds = double(Frames) / double(Rate);
	const int32 Minutes = int32(Seconds / 60.0);
	return FString::Printf(TEXT("%d:%06.3f"), Minutes, Seconds - Minutes * 60.0);
}

FVector2D SAcbTimeline::ComputeDesiredSize(float) const
{
	return FVector2D(240.f, AcbTimelineLocal::HandleHeight + AcbTimelineLocal::BarHeight + AcbTimelineLocal::TimeHeight);
}

float SAcbTimeline::FrameToX(int64 Frame, float Width) const
{
	const int64 Total = NumFrames.Get();
	return Total > 0 ? float(double(FMath::Clamp<int64>(Frame, 0, Total)) / double(Total) * Width) : 0.f;
}

int64 SAcbTimeline::XToFrame(float X, float Width) const
{
	const int64 Total = NumFrames.Get();
	if (Total <= 0 || Width <= 0.f)
	{
		return 0;
	}
	return FMath::Clamp<int64>(int64(double(X) / double(Width) * double(Total)), 0, Total);
}

EAcbTimelineMarker SAcbTimeline::HitMarker(float X, float Width) const
{
	EAcbTimelineMarker Best = EAcbTimelineMarker::None;
	float BestDistance = AcbTimelineLocal::HitSlop;
	auto Try = [&](EAcbTimelineMarker Marker, int64 Frame)
	{
		if (Frame < 0)
		{
			return;
		}
		const float Distance = FMath::Abs(FrameToX(Frame, Width) - X);
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = Marker;
		}
	};
	if (Loop.Get())
	{
		Try(EAcbTimelineMarker::LoopStart, LoopStart.Get());
		Try(EAcbTimelineMarker::LoopEnd, LoopEnd.Get());
	}
	if (CanSetStart.Get())
	{
		Try(EAcbTimelineMarker::Start, FMath::Max<int64>(Start.Get(), 0));
	}
	return Best;
}

int64 SAcbTimeline::ClampMarker(EAcbTimelineMarker Marker, int64 Frame) const
{
	const int64 Total = NumFrames.Get();
	Frame = FMath::Clamp<int64>(Frame, 0, Total);
	if (Marker == EAcbTimelineMarker::LoopStart && LoopEnd.Get() > 0)
	{
		Frame = FMath::Min<int64>(Frame, LoopEnd.Get() - 1);
	}
	else if (Marker == EAcbTimelineMarker::LoopEnd)
	{
		Frame = FMath::Max<int64>(Frame, FMath::Max<int64>(LoopStart.Get(), 0) + 1);
	}
	return FMath::Clamp<int64>(Frame, 0, Total);
}

int32 SAcbTimeline::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& Out,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace AcbTimelineLocal;
	const float Width = Geometry.GetLocalSize().X;
	const float Top = HandleHeight;
	const float Height = BarHeight;
	const float Mid = Top + Height * 0.5f;
	const int64 Total = NumFrames.Get();
	const int32 Rate = SampleRate.Get();
	const bool bLoop = Loop.Get();
	const int64 LS = LoopStart.Get();
	const int64 LE = LoopEnd.Get();
	const int64 S = Start.Get();
	const int64 Pos = Position.Get();
	const float Dim = IsEnabled() ? 1.f : 0.4f;

	Box(Out, LayerId, Geometry, 0.f, Top, Width, Height, Background);
	if (Total <= 0)
	{
		Label(Out, LayerId + 1, Geometry, TEXT("Select one cue"), Width * 0.5f, Mid - 6.f, Width, true, Subdued);
		return LayerId + 1;
	}

	const bool bLoopValid = bLoop && LS >= 0 && LE > LS;
	if (bLoopValid)
	{
		const float X0 = FrameToX(LS, Width);
		const float X1 = FrameToX(LE, Width);
		Box(Out, LayerId + 1, Geometry, X0, Top, X1 - X0, Height, LoopFill);
	}
	if (Pos >= 0)
	{
		Box(Out, LayerId + 1, Geometry, 0.f, Top, FrameToX(Pos, Width), Height, PlayedFill);
	}

	if (Peaks && Peaks->Num() > 0)
	{
		const int32 Buckets = Peaks->Num();
		const float Step = 2.f;
		for (float X = 0.f; X < Width; X += Step)
		{
			const int32 B0 = FMath::Clamp(int32(X / Width * Buckets), 0, Buckets - 1);
			const int32 B1 = FMath::Clamp(int32((X + Step) / Width * Buckets), B0, Buckets - 1);
			float Peak = 0.f;
			for (int32 B = B0; B <= B1; ++B)
			{
				Peak = FMath::Max(Peak, (*Peaks)[B]);
			}
			const float H = FMath::Max(1.f, Peak * (Height - 4.f));
			const int64 Frame = XToFrame(X, Width);
			const bool bInLoop = bLoopValid && Frame >= LS && Frame < LE;
			Box(Out, LayerId + 2, Geometry, X, Mid - H * 0.5f, Step - 0.5f, H, (bInLoop ? WaveInLoop : Wave) * FLinearColor(Dim, Dim, Dim, 1.f));
		}
	}
	else
	{
		Box(Out, LayerId + 2, Geometry, 0.f, Mid, Width, 1.f, Wave);
		Label(Out, LayerId + 3, Geometry, TEXT("Play to load the waveform"), Width * 0.5f, Mid + 4.f, Width, true, Subdued);
	}

	if (bLoop && LS >= 0)
	{
		const float X = FrameToX(LS, Width);
		Box(Out, LayerId + 4, Geometry, X - 1.f, Top, 2.f, Height, LoopColor);
		Box(Out, LayerId + 4, Geometry, X - 1.f, 2.f, 8.f, HandleHeight - 2.f, LoopColor);
		Label(Out, LayerId + 5, Geometry, TEXT("["), X + 1.f, 0.f, Width, false, FLinearColor::Black);
	}
	if (bLoop && LE >= 0)
	{
		const float X = FrameToX(LE, Width);
		Box(Out, LayerId + 4, Geometry, X - 1.f, Top, 2.f, Height, LoopColor);
		Box(Out, LayerId + 4, Geometry, X - 7.f, 2.f, 8.f, HandleHeight - 2.f, LoopColor);
		Label(Out, LayerId + 5, Geometry, TEXT("]"), X - 6.f, 0.f, Width, false, FLinearColor::Black);
	}
	{
		const bool bHasStart = S >= 0;
		const float X = FrameToX(FMath::Max<int64>(S, 0), Width);
		const FLinearColor Color = bHasStart ? StartColor : StartColor * FLinearColor(1.f, 1.f, 1.f, 0.4f);
		if (bHasStart)
		{
			Box(Out, LayerId + 4, Geometry, X - 1.f, Top, 2.f, Height, Color);
		}
		Box(Out, LayerId + 4, Geometry, X - 4.f, 2.f, 8.f, HandleHeight - 2.f, CanSetStart.Get() ? Color : Color * FLinearColor(1.f, 1.f, 1.f, 0.3f));
		Label(Out, LayerId + 5, Geometry, TEXT("S"), X, 0.f, Width, true, FLinearColor::Black);
	}

	if (Pos >= 0)
	{
		Box(Out, LayerId + 6, Geometry, FrameToX(Pos, Width) - 1.f, Top - 2.f, 2.f, Height + 4.f, PlayheadColor);
	}
	if (bScrubbing && ScrubFrame >= 0)
	{
		Box(Out, LayerId + 6, Geometry, FrameToX(ScrubFrame, Width) - 1.f, Top, 2.f, Height, ScrubColor);
	}

	const float TimeY = Top + Height + 1.f;
	Label(Out, LayerId + 7, Geometry, FormatTime(Pos >= 0 ? Pos : 0, Rate), 0.f, TimeY, Width, false, Pos >= 0 ? FLinearColor::White : Subdued);
	Label(Out, LayerId + 7, Geometry, FormatTime(Total, Rate), Width, TimeY, Width, false, Subdued);
	if (HoverX >= 0.f)
	{
		const int64 HoverFrame = Dragging != EAcbTimelineMarker::None ? -1 : XToFrame(HoverX, Width);
		int64 Shown = HoverFrame;
		if (Dragging == EAcbTimelineMarker::Start) Shown = S;
		else if (Dragging == EAcbTimelineMarker::LoopStart) Shown = LS;
		else if (Dragging == EAcbTimelineMarker::LoopEnd) Shown = LE;
		if (bScrubbing) Shown = ScrubFrame;
		if (Shown >= 0)
		{
			Box(Out, LayerId + 5, Geometry, HoverX - 0.5f, Top, 1.f, Height, FLinearColor(1.f, 1.f, 1.f, 0.25f));
			Label(Out, LayerId + 7, Geometry, FormatTime(Shown, Rate), FrameToX(Shown, Width), TimeY, Width, true, FLinearColor::White);
		}
	}
	return LayerId + 7;
}

FReply SAcbTimeline::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || NumFrames.Get() <= 0)
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float Width = MyGeometry.GetLocalSize().X;
	Dragging = HitMarker(Local.X, Width);
	if (Dragging == EAcbTimelineMarker::None)
	{
		bScrubbing = true;
		ScrubFrame = XToFrame(Local.X, Width);
	}
	HoverX = Local.X;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SAcbTimeline::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const float Width = MyGeometry.GetLocalSize().X;
	HoverX = FMath::Clamp(Local.X, 0.f, Width);
	if (Dragging != EAcbTimelineMarker::None && HasMouseCapture())
	{
		OnMarkerMoved.ExecuteIfBound(Dragging, ClampMarker(Dragging, XToFrame(HoverX, Width)));
		return FReply::Handled();
	}
	if (bScrubbing && HasMouseCapture())
	{
		ScrubFrame = XToFrame(HoverX, Width);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SAcbTimeline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	if (bScrubbing && ScrubFrame >= 0)
	{
		OnSeek.ExecuteIfBound(ScrubFrame);
	}
	Dragging = EAcbTimelineMarker::None;
	bScrubbing = false;
	ScrubFrame = -1;
	return FReply::Handled().ReleaseMouseCapture();
}

void SAcbTimeline::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SLeafWidget::OnMouseLeave(MouseEvent);
	if (!HasMouseCapture())
	{
		HoverX = -1.f;
	}
}

FCursorReply SAcbTimeline::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (Dragging != EAcbTimelineMarker::None)
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
	return HitMarker(Local.X, MyGeometry.GetLocalSize().X) != EAcbTimelineMarker::None
		? FCursorReply::Cursor(EMouseCursor::ResizeLeftRight)
		: FCursorReply::Cursor(EMouseCursor::Hand);
}
