// SPDX-License-Identifier: NOASSERTION
#include "Dxf/PhysicsDebugDisplay2D.h"
namespace Dxf
{
namespace
{
// 表示変換と速度線の倍率が使用可能か調べる。
bool ValidDisplay_Internal(const FPhysicsDebugView2D& View, const FPhysicsDebugDisplaySettings2D& Settings) noexcept
{
	return Toolbox::IsFinite(View.ScreenOrigin.X) && Toolbox::IsFinite(View.ScreenOrigin.Y) &&
	       Toolbox::IsFinite(View.PixelsPerMeter) && View.PixelsPerMeter > 0 &&
	       Toolbox::IsFinite(Settings.VelocitySeconds) && Settings.VelocitySeconds >= 0 &&
	       Settings.VelocitySeconds <= 10;
}
// メートル座標の点を画面ピクセルへ移す。Y軸を反転する。
FVector2 ToScreen_Internal(const FPhysicsDebugView2D& View, Toolbox::FVector2 Point) noexcept
{
	return {View.ScreenOrigin.X + Point.X * View.PixelsPerMeter, View.ScreenOrigin.Y - Point.Y * View.PixelsPerMeter};
}
// 角度方向へ長さだけ進めた、メートル座標のずれ。
Toolbox::FVector2 Direction_Internal(Toolbox::f32 Angle, Toolbox::f32 Length) noexcept
{
	return {static_cast<Toolbox::f32>(Toolbox::Cos(Toolbox::f64(Angle)) * Length),
	        static_cast<Toolbox::f32>(Toolbox::Sin(Toolbox::f64(Angle)) * Length)};
}
// 画面座標の線命令が有限値だけで構成されるか調べる。
bool ValidScreen_Internal(FVector2 Point) noexcept
{
	return Toolbox::IsFinite(Point.X) && Toolbox::IsFinite(Point.Y);
}
} // namespace
TResult<void> BuildPhysicsDebugCommands2D(const FPhysicsDebugItem2D& Item, const FPhysicsDebugView2D& View,
                                          const FPhysicsDebugDisplaySettings2D& Settings,
                                          Toolbox::TVector<FRenderCommand>& Output)
{
	if (!ValidDisplay_Internal(View, Settings))
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid 2D physics debug display");
	}
	FDrawStyle Style;
	Style.Layer = Settings.Layer;
	Style.Color = Item.Type == EBodyType::Static
	                  ? FColor{140, 140, 140, 255}
	                  : (Item.bSleeping ? FColor{70, 140, 255, 255} : FColor{80, 255, 130, 255});
	// 失敗時に呼出し側の配列を変更しないための一時領域。
	Toolbox::TVector<FRenderCommand> Commands;
	auto AddLine = [&](Toolbox::FVector2 Start, Toolbox::FVector2 End)
	{
		Commands.PushBack(FLineCommand2D{ToScreen_Internal(View, Start), ToScreen_Internal(View, End), Style});
	};
	if (Settings.bColliders)
	{
		Item.Shape.Visit(
		    [&](const auto& Shape)
		    {
			    if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Shape)>, Toolbox::FCircle2D>)
			    {
				    Commands.PushBack(FCircleCommand2D{ToScreen_Internal(View, Shape.Center),
				                                       Shape.Radius * View.PixelsPerMeter, false, Style});
				    // 円は回転で輪郭が変わらないため、Body角の方向へ半径線を描く。
				    AddLine(Shape.Center, Shape.Center + Direction_Internal(Item.BodyAngle, Shape.Radius));
			    }
			    else
			    {
				    // 変換済みの角度から四隅を作る。Body角は変換時に一度だけ加算済み。
				    const Toolbox::FVector2 AxisX = Direction_Internal(Shape.Angle, Shape.HalfExtents.X);
				    const Toolbox::FVector2 AxisY =
				        Direction_Internal(Shape.Angle + 1.57079632679f, Shape.HalfExtents.Y);
				    const Toolbox::FVector2 Corners[4] = {Shape.Center + AxisX + AxisY, Shape.Center - AxisX + AxisY,
				                                          Shape.Center - AxisX - AxisY, Shape.Center + AxisX - AxisY};
				    for (Toolbox::size_t Index = 0; Index < 4; ++Index)
				    {
					    AddLine(Corners[Index], Corners[(Index + 1) % 4]);
				    }
			    }
		    });
	}
	if (Settings.bVelocities && Settings.VelocitySeconds != 0 &&
	    (Item.Velocity.X * Item.Velocity.X + Item.Velocity.Y * Item.Velocity.Y) > 1e-12f)
	{
		AddLine(Item.CenterOfMass, Item.CenterOfMass + Item.Velocity * Settings.VelocitySeconds);
	}
	if (Settings.bCenters)
	{
		// 画面上で一定の大きさになるよう、4ピクセルをメートルへ換算する。
		const Toolbox::f32 Half = 4.0f / View.PixelsPerMeter;
		AddLine(Item.CenterOfMass - Toolbox::FVector2{Half, 0}, Item.CenterOfMass + Toolbox::FVector2{Half, 0});
		AddLine(Item.CenterOfMass - Toolbox::FVector2{0, Half}, Item.CenterOfMass + Toolbox::FVector2{0, Half});
	}
	for (const auto& Command : Commands)
	{
		const bool bValid = Command.Visit(
		    [](const auto& Value)
		    {
			    if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Value)>, FLineCommand2D>)
			    {
				    return ValidScreen_Internal(Value.Start) && ValidScreen_Internal(Value.End);
			    }
			    else if constexpr (Toolbox::IsSame<Toolbox::TDecay<decltype(Value)>, FCircleCommand2D>)
			    {
				    return ValidScreen_Internal(Value.Center) && Toolbox::IsFinite(Value.Radius) && Value.Radius >= 0;
			    }
			    else
			    {
				    return false;
			    }
		    });
		if (!bValid)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "2D debug shape is not representable");
		}
	}
	Output.Reserve(Output.Size() + Commands.Size());
	for (auto& Command : Commands)
	{
		Output.PushBack(Toolbox::Move(Command));
	}
	return {};
}
TResult<void> SubmitPhysicsDebugSnapshot2D(const FPhysicsDebugSnapshot2D& Snapshot, const FPhysicsDebugView2D& View,
                                           const FPhysicsDebugDisplaySettings2D& Settings, FRender2DContext& Render)
{
	if (!IsValidPhysicsDebugSnapshot2D(Snapshot) || !ValidDisplay_Internal(View, Settings))
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid 2D physics debug snapshot");
	}
	// 全命令を先に作り、一部だけが描画要求へ入る状態を避ける。
	Toolbox::TVector<FRenderCommand> Commands;
	for (const auto& Item : Snapshot.Items)
	{
		auto Built = BuildPhysicsDebugCommands2D(Item, View, Settings, Commands);
		if (!Built)
		{
			return Built;
		}
	}
	return Render.SubmitGenerated(Commands.Size(),
	                              [&](Toolbox::size_t Index, FRenderCommand& Output)
	                              {
		                              Output = Commands[Index];
		                              return TResult<void>{};
	                              });
}
} // namespace Dxf
