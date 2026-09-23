// SPDX-License-Identifier: NOASSERTION
#include "Dxf/DxLibRenderBackend.h"
#include "NativeApi.h"
namespace Dxf
{
namespace
{
// 入力は2Dキューで整数の安全な表現範囲を検証済み。
Toolbox::int32 Pixel_Internal(Toolbox::f32 Value)
{
	return static_cast<Toolbox::int32>(Toolbox::RoundToLong(Value));
}
// DxLib境界のVECTORにのみ変換する。
DxLib::VECTOR NativeVector_Internal(Toolbox::FVector3 Value)
{
	return DxLib::VGet(Value.X, Value.Y, Value.Z);
}
// 色と深度を各命令に明示し、以前の命令の状態に依存しない。
TResult<void> ShapeState_Internal(FColor Color, EDepthMode3D Depth)
{
	const bool Test = Depth != EDepthMode3D::Always;
	const bool Write = Depth == EDepthMode3D::TestAndWrite;
	if (DxLib::SetUseZBufferFlag(Test ? TRUE : FALSE) < 0 || DxLib::SetWriteZBufferFlag(Write ? TRUE : FALSE) < 0 ||
	DxLib::SetUseZBuffer3D(Test ? TRUE : FALSE) < 0 || DxLib::SetWriteZBuffer3D(Write ? TRUE : FALSE) < 0 ||
	DxLib::SetZBufferCmpType(DX_CMP_LESSEQUAL) < 0 || DxLib::SetZBufferCmpType3D(DX_CMP_LESSEQUAL) < 0 ||
	DxLib::SetDrawBlendMode(DX_BLENDMODE_ALPHA, Color.A) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "3D style failed");
	}
	return {};
}
}
TResult<void> FDxLibRenderBackend::DrawLine2D(const FLineCommand2D& Command)
{
	auto Style = ApplyStyle_Internal(Command.Options, false);
	if (!Style)
	{
		return Style;
	}
	const FColor Color = Command.Options.Color;
	return Detail::CheckNative_Internal(DxLib::DrawLine(Pixel_Internal(Command.Start.X), Pixel_Internal(Command.Start.Y),
	Pixel_Internal(Command.End.X), Pixel_Internal(Command.End.Y), DxLib::GetColor(Color.R, Color.G, Color.B)), "DrawLine failed");
}
TResult<void> FDxLibRenderBackend::DrawCircle2D(const FCircleCommand2D& Command)
{
	auto Style = ApplyStyle_Internal(Command.Options, false);
	if (!Style)
	{
		return Style;
	}
	const FColor Color = Command.Options.Color;
	return Detail::CheckNative_Internal(DxLib::DrawCircle(Pixel_Internal(Command.Center.X), Pixel_Internal(Command.Center.Y),
	Pixel_Internal(Command.Radius), DxLib::GetColor(Color.R, Color.G, Color.B), Command.bFilled ? TRUE : FALSE), "DrawCircle failed");
}
TResult<void> FDxLibRenderBackend::DrawTriangle2D(const FTriangleCommand2D& Command)
{
	auto Style = ApplyStyle_Internal(Command.Options, false);
	if (!Style)
	{
		return Style;
	}
	const FColor Color = Command.Options.Color;
	return Detail::CheckNative_Internal(DxLib::DrawTriangle(Pixel_Internal(Command.A.X), Pixel_Internal(Command.A.Y),
	Pixel_Internal(Command.B.X), Pixel_Internal(Command.B.Y), Pixel_Internal(Command.C.X), Pixel_Internal(Command.C.Y),
	DxLib::GetColor(Color.R, Color.G, Color.B), Command.bFilled ? TRUE : FALSE), "DrawTriangle failed");
}
TResult<void> FDxLibRenderBackend::BeginView3D(const FRenderView3D& View)
{
	if (m_bView3D || !IsValidRenderView3D(View))
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid or nested 3D view");
	}
	m_bView3D = true;
	m_ModelView = View;
	// 照明は受け取った頂点色へ評価済み。DxLibのライトで二重評価しない。
	if (DxLib::SetUseLighting(FALSE) < 0 || DxLib::SetUseBackCulling(FALSE) < 0 ||
	    DxLib::SetDrawBright(255, 255, 255) < 0 || DxLib::SetUseVertexShader(-1) < 0 ||
	    DxLib::SetUsePixelShader(-1) < 0 ||
	    (View.bOrthographic ? DxLib::SetupCamera_Ortho(View.OrthographicHeight)
	                        : DxLib::SetupCamera_Perspective(View.VerticalFov)) < 0 ||
	    DxLib::SetCameraNearFar(View.NearPlane, View.FarPlane) < 0 ||
	    DxLib::SetCameraPositionAndTargetAndUpVec(NativeVector_Internal(View.Eye), NativeVector_Internal(View.Target),
	                                              NativeVector_Internal(View.Up)) < 0 ||
	    DxLib::ClearDrawScreenZBuffer() < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "3D view setup failed");
	}
	return {};
}
TResult<void> FDxLibRenderBackend::DrawGeometry3D(const FPreparedGeometry3D& Geometry)
{
	if (!m_bView3D)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "No active 3D view");
	}
	for (const auto& Entry : Geometry.Triangles)
	{
		auto State = ShapeState_Internal(Entry.Color, Entry.Depth);
		if (!State)
		{
			return State;
		}
		const auto& T = Entry.Triangle;
		auto Drawn = Detail::CheckNative_Internal(DxLib::DrawTriangle3D(NativeVector_Internal(T.A), NativeVector_Internal(T.B), NativeVector_Internal(T.C),
		DxLib::GetColor(Entry.Color.R, Entry.Color.G, Entry.Color.B), TRUE), "DrawTriangle3D failed");
		if (!Drawn)
		{
			return Drawn;
		}
	}
	for (const auto& Entry : Geometry.Lines)
	{
		auto State = ShapeState_Internal(Entry.Color, Entry.Depth);
		if (!State)
		{
			return State;
		}
		auto Drawn = Detail::CheckNative_Internal(DxLib::DrawLine3D(NativeVector_Internal(Entry.Line.Start), NativeVector_Internal(Entry.Line.End),
		DxLib::GetColor(Entry.Color.R, Entry.Color.G, Entry.Color.B)), "DrawLine3D failed");
		if (!Drawn)
		{
			return Drawn;
		}
	}
	return {};
}
TResult<void> FDxLibRenderBackend::EndView3D()
{
	if (!m_bView3D)
	{
		return {};
	}
	m_bView3D = false;
	// 一項目の復帰に失敗しても、残りの復帰処理を試す。
	bool Success = RestoreModelLights_Internal();
	if (DxLib::SetUseZBufferFlag(FALSE) < 0)
	{
		Success = false;
	}
	if (DxLib::SetWriteZBufferFlag(FALSE) < 0)
	{
		Success = false;
	}
	if (DxLib::SetUseZBuffer3D(FALSE) < 0)
	{
		Success = false;
	}
	if (DxLib::SetWriteZBuffer3D(FALSE) < 0)
	{
		Success = false;
	}
	if (DxLib::SetUseLighting(TRUE) < 0)
	{
		Success = false;
	}
	if (DxLib::SetUseBackCulling(FALSE) < 0)
	{
		Success = false;
	}
	if (DxLib::SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255) < 0)
	{
		Success = false;
	}
	return Success ? TResult<void>{} : TResult<void>::Failure(EErrorCode::BackendFailure, "3D view cleanup failed");
}
}
