// SPDX-License-Identifier: NOASSERTION
#include "RenderPass3D.h"
#include "Toolbox/Algorithm.h"
namespace Dxf::Detail
{
namespace
{
// 元パケットを参照する索引。計画中だけ存在し、外部資源を保持しない。
struct FTransparentPrimitive
{
	Toolbox::size_t Packet = 0;
	Toolbox::size_t Primitive = 0;
	Toolbox::f64 Depth = 0;
	bool bLine = false;
};
// f32入力の差・重心・射影を丸めず計算するための方向。
struct FSortDirection
{
	Toolbox::f64 X = 0;
	Toolbox::f64 Y = 0;
	Toolbox::f64 Z = 0;
};
// 同値ではfalseを返し、安定ソートが受付順を維持する。
bool FartherFirst_Internal(const FTransparentPrimitive& A, const FTransparentPrimitive& B)
{
	return A.Depth > B.Depth;
}
// 拡大縮小に依存しないカメラの前方向。ビューは呼出し前に検証する。
FSortDirection Forward_Internal(const FRenderView3D& View)
{
	const Toolbox::f64 X = static_cast<Toolbox::f64>(View.Target.X) - View.Eye.X;
	const Toolbox::f64 Y = static_cast<Toolbox::f64>(View.Target.Y) - View.Eye.Y;
	const Toolbox::f64 Z = static_cast<Toolbox::f64>(View.Target.Z) - View.Eye.Z;
	const Toolbox::f64 Length = Toolbox::Sqrt(X * X + Y * Y + Z * Z);
	return {X / Length, Y / Length, Z / Length};
}
// 距離二乗ではなく、符号付きのビュー奥行きを返す。
Toolbox::f64 Project_Internal(Toolbox::f64 X, Toolbox::f64 Y, Toolbox::f64 Z,
	const FRenderView3D& View, FSortDirection Forward)
{
	return (X - View.Eye.X) * Forward.X + (Y - View.Eye.Y) * Forward.Y + (Z - View.Eye.Z) * Forward.Z;
}
// 三頂点の重心。f32の加算で巨大座標をあふれさせない。
Toolbox::f64 Depth_Internal(const FPreparedTriangle3D& Entry, const FRenderView3D& View, FSortDirection Forward)
{
	const auto& T = Entry.Triangle;
	return Project_Internal((static_cast<Toolbox::f64>(T.A.X) + T.B.X + T.C.X) / 3.0,
		(static_cast<Toolbox::f64>(T.A.Y) + T.B.Y + T.C.Y) / 3.0,
		(static_cast<Toolbox::f64>(T.A.Z) + T.B.Z + T.C.Z) / 3.0, View, Forward);
}
// 線分の中点。線と面を同じキー空間で並べる。
Toolbox::f64 Depth_Internal(const FPreparedLine3D& Entry, const FRenderView3D& View, FSortDirection Forward)
{
	return Project_Internal((static_cast<Toolbox::f64>(Entry.Line.Start.X) + Entry.Line.End.X) / 2.0,
		(static_cast<Toolbox::f64>(Entry.Line.Start.Y) + Entry.Line.End.Y) / 2.0,
		(static_cast<Toolbox::f64>(Entry.Line.Start.Z) + Entry.Line.End.Z) / 2.0, View, Forward);
}
// 完全透明は計画へ入れない。Alwaysや明示OverlayはSceneから分離する。
template <typename T>
void Classify_Internal(const T& Entry, Toolbox::size_t Packet, Toolbox::size_t Primitive, bool bLine,
	const FRenderView3D& View, FSortDirection Forward, Toolbox::TVector<T>& Opaque,
	Toolbox::TVector<T>& Overlay, Toolbox::TVector<FTransparentPrimitive>& Transparent)
{
	if (Entry.Color.A == 0)
	{
		return;
	}
	if (Entry.Layer == ERenderLayer3D::Overlay || Entry.Depth == EDepthMode3D::Always)
	{
		T Copy = Entry;
		if (Copy.Depth != EDepthMode3D::Always)
		{
			Copy.Depth = EDepthMode3D::TestOnly;
		}
		Overlay.PushBack(Copy);
	}
	else if (Entry.Color.A == 255)
	{
		Opaque.PushBack(Entry);
	}
	else
	{
		const Toolbox::f64 Depth = Depth_Internal(Entry, View, Forward);
		if (!Toolbox::IsFinite(Depth))
		{
			throw Toolbox::FException("Nonfinite transparency sort key");
		}
		Transparent.PushBack({Packet, Primitive, Depth, bLine});
	}
}
// 空のパケットによるBackend呼出しを作らない。
void AppendNonempty_Internal(Toolbox::TVector<FPreparedGeometry3D>& Output, FPreparedGeometry3D Packet)
{
	if (!Packet.Triangles.IsEmpty() || !Packet.Lines.IsEmpty())
	{
		Output.PushBack(Toolbox::Move(Packet));
	}
}
}
TResult<Toolbox::TVector<FPreparedGeometry3D>> BuildRenderPasses3D_Internal(
	const Toolbox::TVector<FPreparedGeometry3D>& Prepared,
	Toolbox::size_t Begin, Toolbox::size_t End, const FRenderView3D& View)
{
	using TResultType = TResult<Toolbox::TVector<FPreparedGeometry3D>>;
	if (Begin > End || End > Prepared.Size() || !IsValidRenderView3D(View))
	{
		return TResultType::Failure(EErrorCode::InvalidArgument, "Invalid render pass range or view");
	}
	Toolbox::TVector<FPreparedGeometry3D> Output;
	Toolbox::TVector<FPreparedGeometry3D> Overlays;
	Toolbox::TVector<FTransparentPrimitive> Transparent;
	const FSortDirection Forward = Forward_Internal(View);
	for (Toolbox::size_t Packet = Begin; Packet < End; ++Packet)
	{
		FPreparedGeometry3D Opaque;
		FPreparedGeometry3D Overlay;
		const auto& Source = Prepared[Packet];
		for (Toolbox::size_t Index = 0; Index < Source.Triangles.Size(); ++Index)
		{
			Classify_Internal(Source.Triangles[Index], Packet, Index, false, View, Forward,
				Opaque.Triangles, Overlay.Triangles, Transparent);
		}
		for (Toolbox::size_t Index = 0; Index < Source.Lines.Size(); ++Index)
		{
			Classify_Internal(Source.Lines[Index], Packet, Index, true, View, Forward,
				Opaque.Lines, Overlay.Lines, Transparent);
		}
		AppendNonempty_Internal(Output, Toolbox::Move(Opaque));
		AppendNonempty_Internal(Overlays, Toolbox::Move(Overlay));
	}
	if (!Transparent.IsEmpty())
	{
		Toolbox::StableSort(Transparent.Data(), Transparent.Data() + Transparent.Size(), FartherFirst_Internal);
	}
	// 同種が連続する範囲だけまとめる。面と線を別配列へ一括分離すると奥行き順が壊れる。
	FPreparedGeometry3D Run;
	bool bPreviousLine = false;
	for (Toolbox::size_t Index = 0; Index < Transparent.Size(); ++Index)
	{
		const auto& Reference = Transparent[Index];
		if (Index != 0 && Reference.bLine != bPreviousLine)
		{
			AppendNonempty_Internal(Output, Toolbox::Move(Run));
			Run = {};
		}
		bPreviousLine = Reference.bLine;
		if (Reference.bLine)
		{
			auto Entry = Prepared[Reference.Packet].Lines[Reference.Primitive];
			Entry.Depth = EDepthMode3D::TestOnly;
			Run.Lines.PushBack(Entry);
		}
		else
		{
			auto Entry = Prepared[Reference.Packet].Triangles[Reference.Primitive];
			Entry.Depth = EDepthMode3D::TestOnly;
			Run.Triangles.PushBack(Entry);
		}
	}
	AppendNonempty_Internal(Output, Toolbox::Move(Run));
	for (auto& Overlay : Overlays)
	{
		AppendNonempty_Internal(Output, Toolbox::Move(Overlay));
	}
	return TResultType::Success(Toolbox::Move(Output));
}
}
