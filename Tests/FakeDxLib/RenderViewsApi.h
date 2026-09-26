// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_FAKE_RENDER_VIEWS_API_H
#define DXF_FAKE_RENDER_VIEWS_API_H
// 手書きの追加テストダブル。実SDKのABI・実画面は検証しない。
#include "Toolbox/Utility.h"
#include "Toolbox/Vector.h"
#define DX_CMP_LESSEQUAL 4
#define DX_CMP_ALWAYS 8
#define DX_BLENDMODE_DESTCOLOR 8
#define DX_DIRECT3D_11 3
namespace DxLib
{
struct MATRIX
{
	Toolbox::f32 m[4][4];
};
inline Toolbox::int32 ViewCall_Internal();
inline Toolbox::f32 TestDrawZ = 0.2f;
inline Toolbox::int32 TestD3DVersion = DX_DIRECT3D_11;
inline Toolbox::int32 GetDrawScreen()
{
	return -1;
}
inline Toolbox::int32 GetUseDirect3DVersion()
{
	return TestD3DVersion;
}
inline Toolbox::int32 GetDrawScreenSize(Toolbox::int32* W, Toolbox::int32* H)
{
	*W = 640;
	*H = 480;
	return 0;
}
inline MATRIX GetCameraProjectionMatrix()
{
	return {};
}
inline Toolbox::int32 SetupCamera_ProjectionMatrix(MATRIX)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 SetCameraScreenCenter(Toolbox::f32, Toolbox::f32)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 SetDrawZ(Toolbox::f32 Z)
{
	TestDrawZ = Z;
	return ViewCall_Internal();
}
struct VECTOR
{
	Toolbox::f32 x;
	Toolbox::f32 y;
	Toolbox::f32 z;
};
struct FViewTrace
{
	Toolbox::int32 Calls = 0;
	Toolbox::int32 FailAt = -1;
	Toolbox::int32 Lines2D = 0;
	Toolbox::int32 Circles2D = 0;
	Toolbox::int32 Triangles2D = 0;
	Toolbox::int32 Lines3D = 0;
	Toolbox::int32 Triangles3D = 0;
	Toolbox::int32 DepthClears = 0;
	Toolbox::int32 Lighting = 1;
	Toolbox::int32 Z3D = 0;
	Toolbox::int32 WriteZ3D = 0;
	Toolbox::int32 LastFill = -1;
	Toolbox::TVector<Toolbox::int32> Order;
};
inline FViewTrace ViewTrace;
inline Toolbox::int32 ViewCall_Internal()
{
	return ++ViewTrace.Calls == ViewTrace.FailAt ? -1 : 0;
}
inline VECTOR VGet(Toolbox::f32 X, Toolbox::f32 Y, Toolbox::f32 Z)
{
	return {X, Y, Z};
}
inline Toolbox::int32 DrawLine(Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::uint32, Toolbox::int32 = 1)
{
	++ViewTrace.Lines2D;
	ViewTrace.Order.PushBack(2);
	return ViewCall_Internal();
}
inline Toolbox::int32 DrawCircle(Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::uint32, Toolbox::int32 Fill, Toolbox::int32 = 1)
{
	++ViewTrace.Circles2D;
	ViewTrace.LastFill = Fill;
	return ViewCall_Internal();
}
inline Toolbox::int32 DrawTriangle(Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::int32, Toolbox::uint32, Toolbox::int32 Fill)
{
	++ViewTrace.Triangles2D;
	ViewTrace.LastFill = Fill;
	return ViewCall_Internal();
}
inline Toolbox::int32 DrawLine3D(VECTOR, VECTOR, Toolbox::uint32)
{
	++ViewTrace.Lines3D;
	ViewTrace.Order.PushBack(3);
	return ViewCall_Internal();
}
/**
 * 8ビットの色（DxLibのCOLOR_U8と同じ並び）。
 */
struct COLOR_U8
{
	Toolbox::uint8 b;
	Toolbox::uint8 g;
	Toolbox::uint8 r;
	Toolbox::uint8 a;
};
/**
 * 3Dの頂点（DxLibのVERTEX3Dと同じ項目）。
 */
struct VERTEX3D
{
	VECTOR pos;
	VECTOR norm;
	COLOR_U8 dif;
	COLOR_U8 spc;
	Toolbox::f32 u;
	Toolbox::f32 v;
	Toolbox::f32 su;
	Toolbox::f32 sv;
};
inline COLOR_U8 GetColorU8(Toolbox::int32 R, Toolbox::int32 G, Toolbox::int32 B, Toolbox::int32 A)
{
	return {static_cast<Toolbox::uint8>(B), static_cast<Toolbox::uint8>(G), static_cast<Toolbox::uint8>(R), static_cast<Toolbox::uint8>(A)};
}
/**
 * テクスチャ付きのポリゴンの記録（四角形の確認用）。
 */
inline Toolbox::int32 TestPolygons3D = 0;
inline VERTEX3D TestQuadVertices[6]{};
inline Toolbox::int32 DrawPolygon3D(const VERTEX3D* Vertices, Toolbox::int32 Count, Toolbox::int32, Toolbox::int32)
{
	TestPolygons3D += Count;
	for (Toolbox::int32 I = 0; I < Count * 3 && I < 6; ++I)
	{
		TestQuadVertices[I] = Vertices[I];
	}
	ViewTrace.Order.PushBack(4);
	return ViewCall_Internal();
}
inline Toolbox::int32 DrawTriangle3D(VECTOR, VECTOR, VECTOR, Toolbox::uint32, Toolbox::int32 Fill)
{
	++ViewTrace.Triangles3D;
	ViewTrace.LastFill = Fill;
	ViewTrace.Order.PushBack(3);
	return ViewCall_Internal();
}
inline Toolbox::int32 SetUseZBuffer3D(Toolbox::int32 V)
{
	ViewTrace.Z3D = V;
	return ViewCall_Internal();
}
inline Toolbox::int32 SetWriteZBuffer3D(Toolbox::int32 V)
{
	ViewTrace.WriteZ3D = V;
	return ViewCall_Internal();
}
inline Toolbox::int32 SetZBufferCmpType(Toolbox::int32)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 SetZBufferCmpType3D(Toolbox::int32)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 SetUseLighting(Toolbox::int32 V)
{
	ViewTrace.Lighting = V;
	return ViewCall_Internal();
}
inline Toolbox::int32 SetUseBackCulling(Toolbox::int32)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 SetupCamera_Ortho(Toolbox::f32)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 SetupCamera_Perspective(Toolbox::f32)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 SetCameraNearFar(Toolbox::f32, Toolbox::f32)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 SetCameraPositionAndTargetAndUpVec(VECTOR, VECTOR, VECTOR)
{
	return ViewCall_Internal();
}
inline Toolbox::int32 ClearDrawScreenZBuffer()
{
	++ViewTrace.DepthClears;
	return ViewCall_Internal();
}
}
#endif
