// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_FAKE_RENDER_VIEWS_API_H
#define DXF_FAKE_RENDER_VIEWS_API_H
// 手書きの追加テストダブル。実SDKのABI・実画面は検証しない。
#include "Toolbox/Utility.h"
#include "Toolbox/Vector.h"
#define DX_CMP_LESSEQUAL 4
namespace DxLib
{
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
