// SPDX-License-Identifier: NOASSERTION
#include "Dxf/RenderGeometry3D.h"
namespace Dxf
{
namespace
{
// f32で差を取る前に倍精度へ移し、外積の中間オーバーフローを避ける。
struct FWideVector
{
	Toolbox::f64 X;
	Toolbox::f64 Y;
	Toolbox::f64 Z;
};
FWideVector Wide_Internal(Toolbox::FVector3 V)
{
	return {V.X, V.Y, V.Z};
}
FWideVector Difference_Internal(Toolbox::FVector3 A, Toolbox::FVector3 B)
{
	return {static_cast<Toolbox::f64>(A.X) - B.X, static_cast<Toolbox::f64>(A.Y) - B.Y, static_cast<Toolbox::f64>(A.Z) - B.Z};
}
FWideVector Cross_Internal(FWideVector A, FWideVector B)
{
	return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X};
}
Toolbox::f64 Dot_Internal(FWideVector A, FWideVector B)
{
	return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}
bool Valid_Internal(Toolbox::FVector3 V)
{
	return V.IsValid();
}
bool Narrow_Internal(FWideVector V, Toolbox::FVector3& Out)
{
	const Toolbox::f64 Limit = Toolbox::TNumericLimits<Toolbox::f32>::Max();
	if (!Toolbox::IsFinite(V.X) || !Toolbox::IsFinite(V.Y) || !Toolbox::IsFinite(V.Z) ||
	Toolbox::Abs(V.X) > Limit || Toolbox::Abs(V.Y) > Limit || Toolbox::Abs(V.Z) > Limit)
	{
		return false;
	}
	Out = {static_cast<Toolbox::f32>(V.X), static_cast<Toolbox::f32>(V.Y), static_cast<Toolbox::f32>(V.Z)};
	return true;
}
TResult<FGeometry3D> BadGeometry_Internal()
{
	return TResult<FGeometry3D>::Failure(EErrorCode::InvalidArgument, "Invalid or unrepresentable 3D shape");
}
FColor Shade_Internal(const FTriangle3D& Triangle, FColor Base, const FRenderView3D& View)
{
	if (View.Debug.Lighting == ELightingMode3D::Unlit)
	{
		return Base;
	}
	if (View.Debug.Lighting == ELightingMode3D::LightsOff)
	{
		return {0, 0, 0, Base.A};
	}
	const FWideVector Normal = Cross_Internal(Difference_Internal(Triangle.B, Triangle.A), Difference_Internal(Triangle.C, Triangle.A));
	const FWideVector Light = Wide_Internal(View.LightDirection);
	const Toolbox::f64 Denominator = Toolbox::Sqrt(Dot_Internal(Normal, Normal) * Dot_Internal(Light, Light));
	const Toolbox::f64 Cosine = View.bLightEnabled && Denominator > 0
	? Toolbox::Clamp(-Dot_Internal(Normal, Light) / Denominator, 0.0, 1.0) : 0.0;
	auto Channel = [&](Toolbox::uint8 B, Toolbox::uint8 Ambient, Toolbox::uint8 Diffuse)
	{
		const Toolbox::f64 Value = static_cast<Toolbox::f64>(B) * (Ambient + Diffuse * Cosine) / 255.0;
		return static_cast<Toolbox::uint8>(Toolbox::Clamp(Value + 0.5, 0.0, 255.0));
	};
	return {Channel(Base.R, View.AmbientColor.R, View.LightColor.R), Channel(Base.G, View.AmbientColor.G, View.LightColor.G),
		Channel(Base.B, View.AmbientColor.B, View.LightColor.B), Base.A};
}
}
bool IsValidRenderView3D(const FRenderView3D& View) noexcept
{
	if (!Valid_Internal(View.Eye) || !Valid_Internal(View.Target) || !Valid_Internal(View.Up) || !Valid_Internal(View.LightDirection) ||
	!Toolbox::IsFinite(View.VerticalFov) || View.VerticalFov <= 0 || View.VerticalFov >= 3.14159265f ||
	!Toolbox::IsFinite(View.NearPlane) || !Toolbox::IsFinite(View.FarPlane) || View.NearPlane <= 0 || View.FarPlane <= View.NearPlane)
	{
		return false;
	}
	if (View.Debug.Surface != ESurfaceMode3D::Solid && View.Debug.Surface != ESurfaceMode3D::Wireframe && View.Debug.Surface != ESurfaceMode3D::SolidWithEdges)
	{
		return false;
	}
	if (View.Debug.Lighting != ELightingMode3D::Normal && View.Debug.Lighting != ELightingMode3D::Unlit && View.Debug.Lighting != ELightingMode3D::LightsOff)
	{
		return false;
	}
	const FWideVector Direction = Difference_Internal(View.Target, View.Eye);
	const FWideVector Up = Wide_Internal(View.Up);
	const FWideVector Cross = Cross_Internal(Direction, Up);
	return Dot_Internal(Direction, Direction) > 0 && Dot_Internal(Up, Up) > 0 && Dot_Internal(Cross, Cross) > 0 &&
	Dot_Internal(Wide_Internal(View.LightDirection), Wide_Internal(View.LightDirection)) > 0;
}
bool IsValidGeometry3D(const FGeometryCommand3D& Value) noexcept
{
	if (Value.Options.Depth != EDepthMode3D::TestAndWrite && Value.Options.Depth != EDepthMode3D::TestOnly && Value.Options.Depth != EDepthMode3D::Always)
	{
		return false;
	}
	if (Value.Options.Layer != ERenderLayer3D::Scene && Value.Options.Layer != ERenderLayer3D::Overlay)
	{
		return false;
	}
	if (Value.Geometry.Triangles.Size() > 65536 || Value.Geometry.Lines.Size() > 65536)
	{
		return false;
	}
	for (const auto& Line : Value.Geometry.Lines)
	{
		if (!Valid_Internal(Line.Start) || !Valid_Internal(Line.End))
		{
			return false;
		}
	}
	for (const auto& Triangle : Value.Geometry.Triangles)
	{
		if (!Valid_Internal(Triangle.A) || !Valid_Internal(Triangle.B) || !Valid_Internal(Triangle.C))
		{
			return false;
		}
		const auto N = Cross_Internal(Difference_Internal(Triangle.B, Triangle.A), Difference_Internal(Triangle.C, Triangle.A));
		if (Dot_Internal(N, N) == 0)
		{
			return false;
		}
	}
	return true;
}
TResult<FGeometry3D> BuildBoxGeometry3D(const Toolbox::FOBB& Box)
{
	if (!Box.Center.IsValid() || !Box.HalfExtents.IsValid() || Box.HalfExtents.X <= 0 || Box.HalfExtents.Y <= 0 || Box.HalfExtents.Z <= 0)
	{
		return BadGeometry_Internal();
	}
	for (Toolbox::size_t I = 0; I < 3; ++I)
	{
		if (!Box.Axes[I].IsValid() || Toolbox::Abs(Dot_Internal(Wide_Internal(Box.Axes[I]), Wide_Internal(Box.Axes[I])) - 1.0) > 0.0001)
		{
			return BadGeometry_Internal();
		}
		for (Toolbox::size_t J = 0; J < I; ++J)
		{
			if (Toolbox::Abs(Dot_Internal(Wide_Internal(Box.Axes[I]), Wide_Internal(Box.Axes[J]))) > 0.0001)
			{
				return BadGeometry_Internal();
			}
		}
	}
	// 左手基底でも面の向きが反転しないよう後段で順序を調整する。
	const bool Mirrored = Dot_Internal(Cross_Internal(Wide_Internal(Box.Axes[0]), Wide_Internal(Box.Axes[1])), Wide_Internal(Box.Axes[2])) < 0;
	Toolbox::FVector3 Corners[8];
	for (Toolbox::uint32 I = 0; I < 8; ++I)
	{
		FWideVector V = Wide_Internal(Box.Center);
		for (Toolbox::uint32 Axis = 0; Axis < 3; ++Axis)
		{
			const Toolbox::f64 Scale = Box.HalfExtents.Component(static_cast<Toolbox::int32>(Axis)) * ((I & (1u << Axis)) != 0 ? 1.0 : -1.0);
			V.X += Box.Axes[Axis].X * Scale;
			V.Y += Box.Axes[Axis].Y * Scale;
			V.Z += Box.Axes[Axis].Z * Scale;
		}
		if (!Narrow_Internal(V, Corners[I]))
		{
			return BadGeometry_Internal();
		}
	}
	const Toolbox::uint32 Faces[12][3] = {{0,4,6},{0,6,2},{1,3,7},{1,7,5},{0,1,5},{0,5,4},{2,6,7},{2,7,3},{0,2,3},{0,3,1},{4,5,7},{4,7,6}};
	FGeometry3D Result;
	Result.Triangles.Reserve(12);
	Result.Lines.Reserve(12);
	for (const auto& Face : Faces)
	{
		Result.Triangles.PushBack({Corners[Face[0]], Corners[Face[Mirrored ? 2 : 1]], Corners[Face[Mirrored ? 1 : 2]]});
	}
	for (Toolbox::uint32 I = 0; I < 8; ++I)
	{
		for (Toolbox::uint32 Axis = 0; Axis < 3; ++Axis)
		{
			if ((I & (1u << Axis)) == 0)
			{
				Result.Lines.PushBack({Corners[I], Corners[I | (1u << Axis)]});
			}
		}
	}
	FGeometryCommand3D Validated{Toolbox::Move(Result), {}};
	if (!IsValidGeometry3D(Validated))
	{
		return BadGeometry_Internal();
	}
	return TResult<FGeometry3D>::Success(Toolbox::Move(Validated.Geometry));
}
TResult<FGeometry3D> BuildSphereGeometry3D(const Toolbox::FSphere& Sphere, Toolbox::uint32 Segments)
{
	if (!Sphere.Center.IsValid() || !Toolbox::IsFinite(Sphere.Radius) || Sphere.Radius < 0 || Segments < 4 || Segments > 64 || (Segments % 2) != 0)
	{
		return BadGeometry_Internal();
	}
	FGeometry3D Result;
	if (Sphere.Radius == 0)
	{
		return TResult<FGeometry3D>::Success(Toolbox::Move(Result));
	}
	const Toolbox::uint32 Stacks = Segments / 2;
	const Toolbox::f64 Pi = 3.14159265358979323846;
	Toolbox::TVector<Toolbox::FVector3> Grid((Stacks + 1) * Segments);
	for (Toolbox::uint32 Row = 0; Row <= Stacks; ++Row)
	{
		const Toolbox::f64 Phi = Pi * Row / Stacks;
		for (Toolbox::uint32 Col = 0; Col < Segments; ++Col)
		{
			const Toolbox::f64 Theta = 2.0 * Pi * Col / Segments;
			const Toolbox::f64 Radial = Row == 0 || Row == Stacks ? 0 : Toolbox::Sin(Phi) * Sphere.Radius;
			FWideVector V{Sphere.Center.X + Radial * Toolbox::Cos(Theta), Sphere.Center.Y + Toolbox::Cos(Phi) * Sphere.Radius, Sphere.Center.Z + Radial * Toolbox::Sin(Theta)};
			if (!Narrow_Internal(V, Grid[Row * Segments + Col]))
			{
				return BadGeometry_Internal();
			}
		}
	}
	Result.Triangles.Reserve(2 * Segments * (Stacks - 1));
	Result.Lines.Reserve(Segments * (2 * Stacks - 1));
	for (Toolbox::uint32 Row = 0; Row < Stacks; ++Row)
	{
		for (Toolbox::uint32 Col = 0; Col < Segments; ++Col)
		{
			const Toolbox::uint32 Next = (Col + 1) % Segments;
			const auto A = Grid[Row * Segments + Col];
			const auto B = Grid[Row * Segments + Next];
			const auto C = Grid[(Row + 1) * Segments + Col];
			const auto D = Grid[(Row + 1) * Segments + Next];
			if (Row > 0)
			{
				Result.Triangles.PushBack({A, B, C});
				Result.Lines.PushBack({A, B});
			}
			if (Row + 1 < Stacks)
			{
				Result.Triangles.PushBack({B, D, C});
			}
			Result.Lines.PushBack({A, C});
		}
	}
	FGeometryCommand3D Validated{Toolbox::Move(Result), {}};
	if (!IsValidGeometry3D(Validated))
	{
		return BadGeometry_Internal();
	}
	return TResult<FGeometry3D>::Success(Toolbox::Move(Validated.Geometry));
}
TResult<FPreparedGeometry3D> PrepareGeometry3D(const FGeometryCommand3D& Command, const FRenderView3D& View)
{
	if (!IsValidRenderView3D(View) || !IsValidGeometry3D(Command))
	{
		return TResult<FPreparedGeometry3D>::Failure(EErrorCode::InvalidArgument, "Invalid view or geometry");
	}
	FPreparedGeometry3D Result;
	// 不可視の面からだけ不透明なEdgeを生成しない。不正形状の検証は先に行う。
	if (Command.Options.Color.A == 0)
	{
		return TResult<FPreparedGeometry3D>::Success(Toolbox::Move(Result));
	}
	const bool Surface = View.Debug.Surface != ESurfaceMode3D::Wireframe;
	const bool Edges = View.Debug.Surface != ESurfaceMode3D::Solid;
	if (Surface)
	{
		Result.Triangles.Reserve(Command.Geometry.Triangles.Size());
		for (const auto& Triangle : Command.Geometry.Triangles)
		{
			Result.Triangles.PushBack({Triangle, Shade_Internal(Triangle, Command.Options.Color, View), Command.Options.Depth, Command.Options.Layer});
		}
	}
	const bool LineOnly = Command.Geometry.Triangles.IsEmpty();
	if (Edges || LineOnly)
	{
		const FColor Color = LineOnly || View.Debug.Surface == ESurfaceMode3D::Wireframe ? Command.Options.Color : View.Debug.EdgeColor;
		const EDepthMode3D Depth = LineOnly || View.Debug.Surface == ESurfaceMode3D::Wireframe ? Command.Options.Depth :
		(Command.Options.Depth == EDepthMode3D::Always ? EDepthMode3D::Always : EDepthMode3D::TestOnly);
		const ERenderLayer3D Layer = !LineOnly && View.Debug.Surface == ESurfaceMode3D::SolidWithEdges
		    ? ERenderLayer3D::Overlay : Command.Options.Layer;
		if (!Command.Geometry.Lines.IsEmpty())
		{
			Result.Lines.Reserve(Command.Geometry.Lines.Size());
			for (const auto& Line : Command.Geometry.Lines)
			{
				Result.Lines.PushBack({Line, Color, Depth, Layer});
			}
		}
		else
		{
			Result.Lines.Reserve(Command.Geometry.Triangles.Size() * 3);
			for (const auto& T : Command.Geometry.Triangles)
			{
				Result.Lines.PushBack({{T.A, T.B}, Color, Depth, Layer});
				Result.Lines.PushBack({{T.B, T.C}, Color, Depth, Layer});
				Result.Lines.PushBack({{T.C, T.A}, Color, Depth, Layer});
			}
		}
	}
	return TResult<FPreparedGeometry3D>::Success(Toolbox::Move(Result));
}
}
