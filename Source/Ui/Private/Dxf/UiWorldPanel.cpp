// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiWorldPanel.h"
namespace Dxf
{
// パネルが映る画面の画素の矩形。
FUiPixelRect FUiWorldPanel2D::GetPixelRect() const noexcept
{
	// 左上と右下の画面の位置（Y上向きの世界では、上端のYが画面の上になる）。
	const FVector2 TopLeft = Transform.ToScreen(WorldTopLeft);
	const FVector2 BottomRight =
	    Transform.ToScreen({WorldTopLeft.X + WorldSize.X, Transform.bYUp ? WorldTopLeft.Y - WorldSize.Y : WorldTopLeft.Y + WorldSize.Y});
	auto Pixel = [](Toolbox::f32 Value) -> Toolbox::int32
	{
		const Toolbox::f64 Floored = Toolbox::Floor(static_cast<Toolbox::f64>(Value));
		return Toolbox::IsFinite(Floored) ? static_cast<Toolbox::int32>(Toolbox::Clamp<Toolbox::f64>(Floored, -1.0e8, 1.0e8)) : 0;
	};
	FUiPixelRect Rect{Pixel(TopLeft.X), Pixel(TopLeft.Y), Pixel(BottomRight.X), Pixel(BottomRight.Y)};
	if (Rect.Right < Rect.Left || Rect.Bottom < Rect.Top)
	{
		return {Rect.Left, Rect.Top, Rect.Left, Rect.Top};
	}
	return Rect;
}
namespace
{
// 差と内積を倍精度で計算し、f32の世界座標を先に差し引かない。
struct FPanelVector
{
	Toolbox::f64 X;
	Toolbox::f64 Y;
	Toolbox::f64 Z;
};
FPanelVector Difference_Internal(Toolbox::FVector3 A, Toolbox::FVector3 B) noexcept
{
	return {static_cast<Toolbox::f64>(A.X) - B.X, static_cast<Toolbox::f64>(A.Y) - B.Y,
	        static_cast<Toolbox::f64>(A.Z) - B.Z};
}
Toolbox::f64 Dot_Internal(FPanelVector A, FPanelVector B) noexcept
{
	return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
}
FPanelVector Cross_Internal(FPanelVector A, FPanelVector B) noexcept
{
	return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X};
}
} // namespace
// 直交していない平行四辺形も、描画時と同じ二つの辺を基底として解く。
Toolbox::TOptional<FVector2> IntersectUiWorldPanel3D(const FUiWorldPanel3D& Panel, Toolbox::FVector3 Start,
                                                     Toolbox::FVector3 End, Toolbox::FVector3* OutWorld,
                                                     bool bAllowOutside) noexcept
{
	const auto Right = Difference_Internal(Panel.TopRight, Panel.TopLeft);
	const auto Down = Difference_Internal(Panel.BottomLeft, Panel.TopLeft);
	const auto Direction = Difference_Internal(End, Start);
	const auto Offset = Difference_Internal(Start, Panel.TopLeft);
	const auto Normal = Cross_Internal(Right, Down);
	const Toolbox::f64 RR = Dot_Internal(Right, Right);
	const Toolbox::f64 DD = Dot_Internal(Down, Down);
	const Toolbox::f64 RD = Dot_Internal(Right, Down);
	const Toolbox::f64 Determinant = Dot_Internal(Normal, Normal);
	const Toolbox::f64 Denominator = Dot_Internal(Normal, Direction);
	if (!Toolbox::IsFinite(Determinant) || !Toolbox::IsFinite(Denominator) || !(Determinant > RR * DD * 1e-24) ||
	    Denominator == 0 || (!Panel.bDoubleSided && Denominator > 0))
	{
		return {};
	}
	const Toolbox::f64 Time = -Dot_Internal(Normal, Offset) / Denominator;
	if (!Toolbox::IsFinite(Time) || Time < 0 || Time > 1)
	{
		return {};
	}
	const FPanelVector Relative{Offset.X + Direction.X * Time, Offset.Y + Direction.Y * Time,
	                            Offset.Z + Direction.Z * Time};
	const Toolbox::f64 R = Dot_Internal(Relative, Right);
	const Toolbox::f64 D = Dot_Internal(Relative, Down);
	const Toolbox::f64 U = (R * DD - D * RD) / Determinant;
	const Toolbox::f64 V = (D * RR - R * RD) / Determinant;
	const Toolbox::f64 Maximum = Toolbox::TNumericLimits<Toolbox::f32>::Max();
	if (!Toolbox::IsFinite(U) || !Toolbox::IsFinite(V) || Toolbox::Abs(U) > Maximum || Toolbox::Abs(V) > Maximum ||
	    (!bAllowOutside && (U < 0 || U >= 1 || V < 0 || V >= 1)))
	{
		return {};
	}
	if (OutWorld != nullptr)
	{
		*OutWorld = {static_cast<Toolbox::f32>((1 - Time) * Start.X + Time * End.X),
		             static_cast<Toolbox::f32>((1 - Time) * Start.Y + Time * End.Y),
		             static_cast<Toolbox::f32>((1 - Time) * Start.Z + Time * End.Z)};
	}
	return FVector2{static_cast<Toolbox::f32>(U), static_cast<Toolbox::f32>(V)};
}
} // namespace Dxf
