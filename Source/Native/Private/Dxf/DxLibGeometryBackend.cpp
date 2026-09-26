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
	if (View.bViewport)
	{
		if (DxLib::GetUseDirect3DVersion() != DX_DIRECT3D_11)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "Viewport requires Direct3D11");
		}
		// GetDrawScreenSizeはウィンドウ寸法を返すため、テクスチャ描画先は別に問い合わせる。
		const Toolbox::int32 Target = DxLib::GetDrawScreen();
		const Toolbox::int32 SizeResult = Target < 0 ? DxLib::GetDrawScreenSize(&m_TargetWidth, &m_TargetHeight)
		                                             : DxLib::GetGraphSize(Target, &m_TargetWidth, &m_TargetHeight);
		if (SizeResult < 0 || View.Viewport.Right > m_TargetWidth || View.Viewport.Bottom > m_TargetHeight)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Viewport exceeds current target");
		}
	}
	m_bView3D = true;
	m_ModelView = View;
	if (View.bViewport)
	{
		const auto& Rect = View.Viewport;
		// D3D11のClearDepthStencilViewは矩形を無視する。色・アルファを保持して深度1だけを書く。
		if (DxLib::SetDrawArea(Rect.Left, Rect.Top, Rect.Right, Rect.Bottom) < 0 ||
		    DxLib::SetUseZBufferFlag(TRUE) < 0 || DxLib::SetWriteZBufferFlag(TRUE) < 0 ||
		    DxLib::SetZBufferCmpType(DX_CMP_ALWAYS) < 0 || DxLib::SetDrawZ(1.0f) < 0 ||
		    DxLib::SetDrawBlendMode(DX_BLENDMODE_DESTCOLOR, 255) < 0 ||
		    DxLib::DrawBox(Rect.Left, Rect.Top, Rect.Right, Rect.Bottom, DxLib::GetColor(255, 255, 255), TRUE) < 0)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "Viewport depth initialization failed");
		}
	}
	// 照明は受け取った頂点色へ評価済み。DxLibのライトで二重評価しない。
	if (DxLib::SetUseLighting(FALSE) < 0 || DxLib::SetUseBackCulling(FALSE) < 0 ||
	    DxLib::SetDrawBright(255, 255, 255) < 0 || DxLib::SetUseVertexShader(-1) < 0 ||
	    DxLib::SetUsePixelShader(-1) < 0 ||
	    (View.bOrthographic ? DxLib::SetupCamera_Ortho(View.OrthographicHeight)
	                        : DxLib::SetupCamera_Perspective(View.VerticalFov)) < 0 ||
	    DxLib::SetCameraNearFar(View.NearPlane, View.FarPlane) < 0 ||
	    DxLib::SetCameraPositionAndTargetAndUpVec(NativeVector_Internal(View.Eye), NativeVector_Internal(View.Target),
	                                              NativeVector_Internal(View.Up)) < 0 ||
	    (!View.bViewport && DxLib::ClearDrawScreenZBuffer() < 0))
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "3D view setup failed");
	}
	if (View.bViewport)
	{
		const auto& Rect = View.Viewport;
		// 全描画先基準の投影を矩形の縦サイズへ縮小する。Xも同率なので画素の縦横比は変えない。
		auto Projection = DxLib::GetCameraProjectionMatrix();
		const Toolbox::f32 Scale = static_cast<Toolbox::f32>(Rect.Bottom - Rect.Top) / m_TargetHeight;
		Projection.m[0][0] *= Scale;
		Projection.m[1][1] *= Scale;
		if (DxLib::SetupCamera_ProjectionMatrix(Projection) < 0 ||
		    DxLib::SetCameraScreenCenter(Rect.Left + (Rect.Right - Rect.Left) * 0.5f,
		                                 Rect.Top + (Rect.Bottom - Rect.Top) * 0.5f) < 0)
		{
			return TResult<void>::Failure(EErrorCode::BackendFailure, "Viewport projection failed");
		}
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
// テクスチャを貼った四角形を描く。
// @param Quad 四角形。
TResult<void> FDxLibRenderBackend::DrawTexturedQuad3D(const FTexturedQuad3D& Quad)
{
	if (!m_bView3D)
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "No active 3D view");
	}
	if (!Quad.Texture.IsValid())
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "Invalid quad texture");
	}
	const auto& C = Quad.Corners;
	// 表面（右×下）の向き。表面だけの指定で視点が裏側なら描かない。
	const Toolbox::FVector3 Normal = Toolbox::Cross(C[1] - C[0], C[3] - C[0]);
	if (!Quad.bDoubleSided && Toolbox::Dot(Normal, m_ModelView.Eye - C[0]) <= 0)
	{
		return {};
	}
	auto State = ShapeState_Internal(Quad.Tint, Quad.Depth);
	if (!State)
	{
		return State;
	}
	if (DxLib::SetUseLighting(FALSE) < 0 || DxLib::SetUseBackCulling(FALSE) < 0)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Quad state failed");
	}
	// 頂点（左上・右上・右下・左下）とテクスチャ座標。
	const Toolbox::f32 U[4] = {0, 1, 1, 0};
	const Toolbox::f32 V[4] = {0, 0, 1, 1};
	const Toolbox::size_t Order[6] = {0, 1, 2, 0, 2, 3};
	DxLib::VERTEX3D Vertices[6];
	const Toolbox::f32 Length = Toolbox::Length(Normal);
	const Toolbox::FVector3 Unit = Length > 0 ? Normal * (1.0f / Length) : Toolbox::FVector3{0, 0, -1};
	for (Toolbox::size_t Index = 0; Index < 6; ++Index)
	{
		const Toolbox::size_t Corner = Order[Index];
		auto& Vertex = Vertices[Index];
		Vertex.pos = NativeVector_Internal(C[Corner]);
		Vertex.norm = NativeVector_Internal(Unit);
		// 不透明度はShapeStateの定数へ一度だけ渡す。頂点側で二重に掛けない。
		Vertex.dif = DxLib::GetColorU8(Quad.Tint.R, Quad.Tint.G, Quad.Tint.B, 255);
		Vertex.spc = DxLib::GetColorU8(0, 0, 0, 0);
		Vertex.u = U[Corner];
		Vertex.v = V[Corner];
		Vertex.su = U[Corner];
		Vertex.sv = V[Corner];
	}
	auto Drawn = Detail::CheckNative_Internal(DxLib::DrawPolygon3D(Vertices, 2, Quad.Texture.GetNativeHandle_Internal(), TRUE),
	                                          "DrawPolygon3D failed");
	// 同じビューの基本形状はCPU側で照明済みなので、照明無効を維持する。
	if (DxLib::SetUseLighting(FALSE) < 0 && Drawn)
	{
		return TResult<void>::Failure(EErrorCode::BackendFailure, "Lighting restore failed");
	}
	return Drawn;
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
	if (m_ModelView.bViewport)
	{
		// 失敗後も各項目を独立して復帰する。2Dだけのフレームには追加処理しない。
		if (DxLib::SetDrawArea(0, 0, m_TargetWidth, m_TargetHeight) < 0)
		{
			Success = false;
		}
		if (DxLib::SetDrawZ(0.2f) < 0)
		{
			Success = false;
		}
		if (DxLib::SetupCamera_Perspective(1.0471975512f) < 0)
		{
			Success = false;
		}
		if (DxLib::SetCameraScreenCenter(m_TargetWidth * 0.5f, m_TargetHeight * 0.5f) < 0)
		{
			Success = false;
		}
	}
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
