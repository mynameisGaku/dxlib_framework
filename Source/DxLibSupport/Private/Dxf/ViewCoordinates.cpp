// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ViewCoordinates.h"
namespace Dxf
{
namespace
{
// カメラ計算の差・積を倍精度で保持する。公開行列の作用方向は変更しない。
using FWide = Toolbox::TArray<Toolbox::f64, 3>;
// 解決した矩形とカメラ基底。呼出しの間だけ存在する値。
struct FCamera
{
	// 描画先全体での有効範囲。
	FIntRect Rect;
	// 画面右・上・カメラ前方の単位基底。
	FWide Right;
	FWide Up;
	FWide Forward;
	// 正射影の半高さ、または透視の単位距離での半高さ。
	Toolbox::f64 HalfHeight = 0;
};
// 倍精度の基底への射影長。
Toolbox::f64 Dot_Internal(const FWide& A, const FWide& B)
{
	return A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
}
// カメラの右・上方向を構成する外積。
FWide Cross_Internal(const FWide& A, const FWide& B)
{
	return {A[1] * B[2] - A[2] * B[1], A[2] * B[0] - A[0] * B[2], A[0] * B[1] - A[1] * B[0]};
}
// ゼロ方向を拒否して単位方向へ変換する。
bool Normalize_Internal(FWide& Value)
{
	const Toolbox::f64 Length = Toolbox::Sqrt(Dot_Internal(Value, Value));
	if (!Toolbox::IsFinite(Length) || Length == 0)
	{
		return false;
	}
	for (auto& Part : Value)
	{
		Part /= Length;
	}
	return true;
}
// 入力を検査し、呼出し専用の矩形と基底を解決する。
bool Resolve_Internal(const FRenderView3D& View, Toolbox::int32 Width, Toolbox::int32 Height, FCamera& Camera)
{
	// 照明は計算に使用しない。カメラと描画領域だけを検査する。
	if (Width <= 0 || Height <= 0 || !View.Eye.IsValid() || !View.Target.IsValid() || !View.Up.IsValid() || !Toolbox::IsFinite(View.NearPlane) || !Toolbox::IsFinite(View.FarPlane) || View.NearPlane <= 0 || View.FarPlane <= View.NearPlane || !Toolbox::IsFinite(View.VerticalFov) || View.VerticalFov <= 0 || View.VerticalFov >= 3.14159265f || !Toolbox::IsFinite(View.OrthographicHeight) || View.OrthographicHeight <= 0)
	{
		return false;
	}
	Camera.Rect = View.bViewport ? View.Viewport : FIntRect{0, 0, Width, Height};
	const auto& R = Camera.Rect;
	if (R.Left < 0 || R.Top < 0 || R.Right <= R.Left || R.Bottom <= R.Top || R.Right > Width || R.Bottom > Height)
	{
		return false;
	}
	Camera.Forward = {static_cast<Toolbox::f64>(View.Target.X) - View.Eye.X, static_cast<Toolbox::f64>(View.Target.Y) - View.Eye.Y, static_cast<Toolbox::f64>(View.Target.Z) - View.Eye.Z};
	if (!Normalize_Internal(Camera.Forward))
	{
		return false;
	}
	Camera.Right = Cross_Internal({View.Up.X, View.Up.Y, View.Up.Z}, Camera.Forward);
	if (!Normalize_Internal(Camera.Right))
	{
		return false;
	}
	Camera.Up = Cross_Internal(Camera.Forward, Camera.Right);
	Camera.HalfHeight = View.bOrthographic ? View.OrthographicHeight * 0.5 : (Toolbox::Sin(View.VerticalFov * 0.5) / Toolbox::Cos(View.VerticalFov * 0.5));
	return Toolbox::IsFinite(Camera.HalfHeight) && Camera.HalfHeight > 0;
}
// 公開座標のf32に収まる値だけを格納する。
bool Narrow_Internal(Toolbox::f64 Value, Toolbox::f32& Out)
{
	if (!Toolbox::IsFinite(Value) || Toolbox::Abs(Value) > Toolbox::TNumericLimits<Toolbox::f32>::Max())
	{
		return false;
	}
	Out = static_cast<Toolbox::f32>(Value);
	return true;
}
// 画面境界を丸めず半開区間で検査する。
bool Contains_Internal(const FIntRect& R, FVector2 Point)
{
	return static_cast<Toolbox::f64>(Point.X) >= R.Left && static_cast<Toolbox::f64>(Point.X) < R.Right && static_cast<Toolbox::f64>(Point.Y) >= R.Top && static_cast<Toolbox::f64>(Point.Y) < R.Bottom;
}
} // namespace
TResult<FProjectedPoint3D> ProjectWorldToScreen(const FRenderView3D& View, Toolbox::int32 Width, Toolbox::int32 Height, Toolbox::FVector3 World)
{
	// Nativeとは共有しない計算用ビュー。
	FCamera Camera;
	if (!World.IsValid() || !Resolve_Internal(View, Width, Height, Camera))
	{
		return TResult<FProjectedPoint3D>::Failure(EErrorCode::InvalidArgument, "Invalid view projection input");
	}
	// 眼からの相対位置を、右・上・前の直交基底へ写す。
	const FWide Delta{static_cast<Toolbox::f64>(World.X) - View.Eye.X, static_cast<Toolbox::f64>(World.Y) - View.Eye.Y, static_cast<Toolbox::f64>(World.Z) - View.Eye.Z};
	// カメラ前方向の距離。ユークリッド距離とは区別する。
	const Toolbox::f64 Z = Dot_Internal(Delta, Camera.Forward);
	if (!View.bOrthographic && Z <= 0)
	{
		return TResult<FProjectedPoint3D>::Failure(EErrorCode::InvalidArgument, "Perspective point is on or behind eye plane");
	}
	const auto& R = Camera.Rect;
	// 矩形の縦寸法に合わせた画素倍率。
	const Toolbox::f64 Scale = (R.Bottom - R.Top) * 0.5 / Camera.HalfHeight / (View.bOrthographic ? 1 : Z);
	// 成功時だけ返す投影結果。
	FProjectedPoint3D Result;
	if (!Narrow_Internal(R.Left + (R.Right - R.Left) * 0.5 + Dot_Internal(Delta, Camera.Right) * Scale, Result.Screen.X) || !Narrow_Internal(R.Top + (R.Bottom - R.Top) * 0.5 - Dot_Internal(Delta, Camera.Up) * Scale, Result.Screen.Y))
	{
		return TResult<FProjectedPoint3D>::Failure(EErrorCode::InvalidArgument, "Projected coordinates exceed finite range");
	}
	const Toolbox::f64 Near = View.NearPlane;
	const Toolbox::f64 Far = View.FarPlane;
	Result.Depth = View.bOrthographic ? (Z - Near) / (Far - Near) : (Far / (Far - Near)) * ((Z - Near) / Z);
	if (!Toolbox::IsFinite(Result.Depth))
	{
		return TResult<FProjectedPoint3D>::Failure(EErrorCode::InvalidArgument, "Projected depth is not finite");
	}
	Result.bInsideView = Contains_Internal(R, Result.Screen) && Z >= Near && Z <= Far;
	return TResult<FProjectedPoint3D>::Success(Result);
}
TResult<Toolbox::TOptional<FLine3D>> MakeViewPickSegment(const FRenderView3D& View, Toolbox::int32 Width, Toolbox::int32 Height, FVector2 Screen)
{
	using FResult = TResult<Toolbox::TOptional<FLine3D>>;
	// Nativeとは共有しない計算用ビュー。
	FCamera Camera;
	if (!Toolbox::IsFinite(Screen.X) || !Toolbox::IsFinite(Screen.Y) || !Resolve_Internal(View, Width, Height, Camera))
	{
		return FResult::Failure(EErrorCode::InvalidArgument, "Invalid pick segment input");
	}
	const auto& R = Camera.Rect;
	if (!Contains_Internal(R, Screen))
	{
		return FResult::Success({});
	}
	// 連続ピクセル位置を中心基準へ変換する。半ピクセルを加算しない。
	const Toolbox::f64 X = (Screen.X - (R.Left + (R.Right - R.Left) * 0.5)) * (2 * Camera.HalfHeight / (R.Bottom - R.Top));
	const Toolbox::f64 Y = ((R.Top + (R.Bottom - R.Top) * 0.5) - Screen.Y) * (2 * Camera.HalfHeight / (R.Bottom - R.Top));
	// 近接面と遠方面の端点。
	FLine3D Segment;
	for (Toolbox::int32 End = 0; End < 2; ++End)
	{
		const Toolbox::f64 Z = End == 0 ? View.NearPlane : View.FarPlane;
		Toolbox::f32 Values[3]{};
		for (Toolbox::int32 Axis = 0; Axis < 3; ++Axis)
		{
			const Toolbox::f64 Lateral = (Camera.Right[Axis] * X + Camera.Up[Axis] * Y) * (View.bOrthographic ? 1 : Z);
			if (!Narrow_Internal(View.Eye.Component(Axis) + Camera.Forward[Axis] * Z + Lateral, Values[Axis]))
			{
				return FResult::Failure(EErrorCode::InvalidArgument, "Pick segment exceeds finite world coordinates");
			}
		}
		(End == 0 ? Segment.Start : Segment.End) = {Values[0], Values[1], Values[2]};
	}
	if (Segment.Start == Segment.End)
	{
		return FResult::Failure(EErrorCode::InvalidArgument, "Pick segment loses depth precision");
	}
	return FResult::Success(Segment);
}
} // namespace Dxf
