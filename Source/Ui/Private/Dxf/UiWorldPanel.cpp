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
// 有限線分とパネルの交点。
Toolbox::TOptional<FVector2> IntersectUiWorldPanel3D(const FUiWorldPanel3D& Panel, Toolbox::FVector3 Start, Toolbox::FVector3 End,
                                                     Toolbox::FVector3* OutWorld) noexcept
{
	const Toolbox::FVector3 Right = Panel.TopRight - Panel.TopLeft;
	const Toolbox::FVector3 Down = Panel.BottomLeft - Panel.TopLeft;
	// 表面の向き（右×下）。
	const Toolbox::FVector3 Normal = Toolbox::Cross(Right, Down);
	const Toolbox::FVector3 Direction = End - Start;
	const Toolbox::f64 Denominator = Toolbox::Dot(Normal, Direction);
	const Toolbox::f64 RightLength = Toolbox::LengthSquared(Right);
	const Toolbox::f64 DownLength = Toolbox::LengthSquared(Down);
	if (!(RightLength > 0) || !(DownLength > 0) || !Toolbox::IsFinite(Denominator) || Toolbox::Abs(Denominator) < 1e-12)
	{
		return {};
	}
	// 表面は、線分が法線と逆向きに進む（視点が表側にある）ときだけ受ける。
	if (!Panel.bDoubleSided && Denominator > 0)
	{
		return {};
	}
	const Toolbox::f64 T = Toolbox::Dot(Normal, Panel.TopLeft - Start) / Denominator;
	if (!(T >= 0) || !(T <= 1))
	{
		return {};
	}
	const Toolbox::FVector3 Hit = Start + Direction * static_cast<Toolbox::f32>(T);
	const Toolbox::f64 U = Toolbox::Dot(Hit - Panel.TopLeft, Right) / RightLength;
	const Toolbox::f64 V = Toolbox::Dot(Hit - Panel.TopLeft, Down) / DownLength;
	// 半開区間（右端・下端は含まない）。
	if (!(U >= 0) || !(U < 1) || !(V >= 0) || !(V < 1))
	{
		return {};
	}
	if (OutWorld != nullptr)
	{
		*OutWorld = Hit;
	}
	return FVector2{static_cast<Toolbox::f32>(U), static_cast<Toolbox::f32>(V)};
}
} // namespace Dxf
