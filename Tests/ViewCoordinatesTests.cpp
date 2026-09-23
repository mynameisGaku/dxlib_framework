// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/ViewCoordinates.h"
#include "Toolbox/SegmentIntersection.h"
#include "../Examples/ModelViewer/PickingExample.h"
namespace
{
using namespace Dxf;
using namespace Toolbox;
FRenderView3D Camera_Internal()
{
	FRenderView3D View;
	View.Eye = {0, 0, 0};
	View.Target = {0, 0, 1};
	View.Up = {0, 1, 0};
	View.NearPlane = 1;
	View.FarPlane = 11;
	View.VerticalFov = 1.57079632679f;
	View.OrthographicHeight = 8;
	return View;
}
void Near_Internal(f64 A, f64 B, f64 Tolerance = 0.0001)
{
	REQUIRE(Abs(A - B) <= Tolerance);
}
} // namespace
TEST("view coordinates have analytic perspective and orthographic results")
{
	auto View = Camera_Internal();
	auto Point = ProjectWorldToScreen(View, 800, 600, {1, 1, 2});
	REQUIRE(Point);
	Near_Internal(Point.Value().Screen.X, 550);
	Near_Internal(Point.Value().Screen.Y, 150);
	Near_Internal(Point.Value().Depth, 0.55);
	REQUIRE(Point.Value().bInsideView);
	auto Segment = MakeViewPickSegment(View, 800, 600, {550, 150});
	REQUIRE(Segment && Segment.Value());
	Near_Internal(Segment.Value()->Start.X, 0.5);
	Near_Internal(Segment.Value()->Start.Y, 0.5);
	Near_Internal(Segment.Value()->Start.Z, 1);
	Near_Internal(Segment.Value()->End.X, 5.5);
	Near_Internal(Segment.Value()->End.Z, 11);
	View.bOrthographic = true;
	Point = ProjectWorldToScreen(View, 800, 600, {1, 1, 2});
	REQUIRE(Point);
	Near_Internal(Point.Value().Screen.X, 475);
	Near_Internal(Point.Value().Screen.Y, 225);
	Near_Internal(Point.Value().Depth, 0.1);
	Segment = MakeViewPickSegment(View, 800, 600, {475, 225});
	REQUIRE(Segment && Segment.Value());
	Near_Internal(Segment.Value()->Start.X, 1);
	Near_Internal(Segment.Value()->End.X, 1);
	Near_Internal(Segment.Value()->Start.Z, 1);
	Near_Internal(Segment.Value()->End.Z, 11);
}
TEST("split odd targets use half open bounds without a half pixel adjustment")
{
	auto View = Camera_Internal();
	View.bViewport = true;
	View.Viewport = {320, 7, 641, 481};
	auto Center = ProjectWorldToScreen(View, 641, 481, {0, 0, 2});
	REQUIRE(Center);
	Near_Internal(Center.Value().Screen.X, 480.5);
	Near_Internal(Center.Value().Screen.Y, 244);
	for (const FVector2 Screen : TArray<FVector2, 4>{FVector2{320, 7}, FVector2{640.5f, 480.5f}, FVector2{321, 100}, FVector2{480.5f, 244}})
	{
		const auto Ray = MakeViewPickSegment(View, 641, 481, Screen);
		REQUIRE(Ray && Ray.Value());
		const auto Point = ProjectWorldToScreen(View, 641, 481, Ray.Value()->Start + (Ray.Value()->End - Ray.Value()->Start) * 0.5f);
		REQUIRE(Point);
		Near_Internal(Point.Value().Screen.X, Screen.X);
		Near_Internal(Point.Value().Screen.Y, Screen.Y);
	}
	for (const FVector2 Screen : TArray<FVector2, 4>{FVector2{319.999f, 100}, FVector2{641, 100}, FVector2{400, 6.99f}, FVector2{400, 481}})
	{
		const auto Ray = MakeViewPickSegment(View, 641, 481, Screen);
		REQUIRE(Ray && !Ray.Value());
	}
	View.Viewport = {0, 0, 320, 481};
	REQUIRE(!MakeViewPickSegment(View, 641, 481, {320, 240}).Value());
	REQUIRE(MakeViewPickSegment(View, 641, 481, {319.5f, 240}).Value());
}
TEST("moved rotated camera has analytic world endpoints and independent lights")
{
	auto View = Camera_Internal();
	View.Eye = {10, 20, 30};
	View.Target = {11, 20, 30};
	View.Up = {0, 2, 0};
	auto Ray = MakeViewPickSegment(View, 200, 100, {150, 25});
	REQUIRE(Ray && Ray.Value());
	Near_Internal(Ray.Value()->Start.X, 11);
	Near_Internal(Ray.Value()->Start.Y, 20.5);
	Near_Internal(Ray.Value()->Start.Z, 29);
	View.LightDirection.X = TNumericLimits<f32>::QuietNaN();
	auto Point = ProjectWorldToScreen(View, 200, 100, {12, 21, 28});
	REQUIRE(Point);
	Near_Internal(Point.Value().Screen.X, 150);
	Near_Internal(Point.Value().Screen.Y, 25);
	View.bOrthographic = true;
	Ray = MakeViewPickSegment(View, 200, 100, {150, 25});
	REQUIRE(Ray && Ray.Value());
	Near_Internal(Ray.Value()->Start.X, 11);
	Near_Internal(Ray.Value()->Start.Y, 22);
	Near_Internal(Ray.Value()->Start.Z, 26);
}
TEST("clip depth and perspective behind eye are explicit")
{
	auto View = Camera_Internal();
	for (int32 Ortho = 0; Ortho < 2; ++Ortho)
	{
		View.bOrthographic = Ortho != 0;
		auto Near = ProjectWorldToScreen(View, 640, 480, {0, 0, 1});
		auto Far = ProjectWorldToScreen(View, 640, 480, {0, 0, 11});
		REQUIRE(Near && Far && Near.Value().bInsideView && Far.Value().bInsideView);
		Near_Internal(Near.Value().Depth, 0);
		Near_Internal(Far.Value().Depth, 1);
		REQUIRE(!ProjectWorldToScreen(View, 640, 480, {0, 0, 0.5f}).Value().bInsideView);
		REQUIRE(!ProjectWorldToScreen(View, 640, 480, {0, 0, 12}).Value().bInsideView);
		REQUIRE(!ProjectWorldToScreen(View, 640, 480, {100, 0, 2}).Value().bInsideView);
	}
	REQUIRE(!ProjectWorldToScreen(View, 640, 480, {0, 0, -2}).Value().bInsideView);
	View.bOrthographic = false;
	REQUIRE(!ProjectWorldToScreen(View, 640, 480, {0, 0, 0}));
	REQUIRE(!ProjectWorldToScreen(View, 640, 480, {0, 0, -2}));
}
TEST("invalid and unrepresentable coordinate inputs fail without a native session")
{
	const auto Good = Camera_Internal();
	auto View = Good;
	const f32 Nan = TNumericLimits<f32>::QuietNaN();
	REQUIRE(!ProjectWorldToScreen(View, 0, 480, {0, 0, 2}));
	REQUIRE(!MakeViewPickSegment(View, 640, -1, {1, 1}));
	REQUIRE(!MakeViewPickSegment(View, 640, 480, {Nan, 1}));
	REQUIRE(!ProjectWorldToScreen(View, 640, 480, {Nan, 0, 2}));
	for (int32 Case = 0; Case < 9; ++Case)
	{
		View = Good;
		switch (Case)
		{
		case 0:
			View.Target = View.Eye;
			break;
		case 1:
			View.Up = {0, 0, 1};
			break;
		case 2:
			View.NearPlane = 0;
			break;
		case 3:
			View.FarPlane = View.NearPlane;
			break;
		case 4:
			View.VerticalFov = 0;
			break;
		case 5:
			View.OrthographicHeight = Nan;
			break;
		case 6:
			View.Eye.X = Nan;
			break;
		case 7:
			View.bViewport = true;
			View.Viewport = {0, 0, 641, 480};
			break;
		case 8:
			View.bViewport = true;
			View.Viewport = {10, 0, 10, 480};
			break;
		}
		REQUIRE(!MakeViewPickSegment(View, 640, 480, {320, 240}));
		REQUIRE(!ProjectWorldToScreen(View, 640, 480, {0, 0, 2}));
	}
	View = Good;
	View.Eye = {0, 0, TNumericLimits<f32>::Max()};
	View.Target = {0, 0, 0};
	REQUIRE(!MakeViewPickSegment(View, 640, 480, {320, 240}));
}
TEST("finite segment intersections include endpoints tangencies and inside starts")
{
	const FSphere Sphere{{0, 0, 5}, 1};
	const FOBB Box{{0, 0, 5}, {1, 1, 1}};
	for (int32 Shape = 0; Shape < 2; ++Shape)
	{
		const auto Hit = Shape == 0 ? IntersectSegment({0, 0, 0}, {0, 0, 10}, Sphere) : IntersectSegment({0, 0, 0}, {0, 0, 10}, Box);
		REQUIRE(Hit);
		Near_Internal(*Hit, 0.4);
	}
	Near_Internal(*IntersectSegment({1, 0, 0}, {1, 0, 10}, Sphere), 0.5);
	Near_Internal(*IntersectSegment({0, 0, 5}, {0, 0, 5}, Sphere), 0);
	Near_Internal(*IntersectSegment({0, 0, 5}, {0, 0, 10}, Box), 0);
	Near_Internal(*IntersectSegment({0, 0, 0}, {0, 0, 4}, Sphere), 1);
	Near_Internal(*IntersectSegment({0, 0, 0}, {0, 0, 4}, Box), 1);
	REQUIRE(!IntersectSegment({0, 0, 0}, {0, 0, 3.99f}, Sphere));
	REQUIRE(!IntersectSegment({0, 0, 0}, {0, 0, 3.99f}, Box));
	REQUIRE(!IntersectSegment({2, 0, 0}, {2, 0, 10}, Box));
	REQUIRE(!IntersectSegment({0, 0, 6.01f}, {0, 0, 10}, Sphere));
	REQUIRE(!IntersectSegment({0, 0, 6.01f}, {0, 0, 10}, Box));
	FOBB Rotated = Box;
	const f32 C = static_cast<f32>(Sqrt(0.5));
	Rotated.Axes = {FVector3{C, 0, C}, FVector3{0, 1, 0}, FVector3{-C, 0, C}};
	Near_Internal(*IntersectSegment({0, 0, 0}, {0, 0, 10}, Rotated), (5 - Sqrt(2.0)) / 10);
}
TEST("sample picking chooses nearest stable ties and clips to the requested view")
{
	const auto View = Camera_Internal();
	const FSphere Sphere{{0, 0, 5}, 1};
	FOBB Box{{0, 0, 8}, {1, 1, 1}};
	REQUIRE(Dxf::ModelViewer::PickExampleShapes(View, 640, 480, {320, 240}, Sphere, Box).Value() == 0);
	Box.Center.Z = 3;
	REQUIRE(Dxf::ModelViewer::PickExampleShapes(View, 640, 480, {320, 240}, Sphere, Box).Value() == 1);
	Box.Center.Z = 5;
	REQUIRE(Dxf::ModelViewer::PickExampleShapes(View, 640, 480, {320, 240}, Sphere, Box).Value() == 0);
	REQUIRE(Dxf::ModelViewer::PickExampleShapes(View, 640, 480, {640, 240}, Sphere, Box).Value() == -1);
	REQUIRE(Dxf::ModelViewer::PickExampleShapes(View, 640, 480, {0, 0}, Sphere, Box).Value() == -1);
	REQUIRE(Dxf::ModelViewer::PickExampleShapes(View, 640, 480, {320, 240}, FSphere{{0, 0, 0}, 0.1f}, FOBB{{0, 0, 20}, {1, 1, 1}}).Value() == -1);
}

TEST("segment rejects invalid shapes and finite limits are not silently clamped")
{
	const f32 Nan = TNumericLimits<f32>::QuietNaN();
	for (int32 Case = 0; Case < 3; ++Case)
	{
		bool Failed = false;
		try
		{
			if (Case == 0)
			{
				(void)IntersectSegment({0, 0, 0}, {0, 0, 10}, FSphere{{0, 0, 5}, -1});
			}
			if (Case == 1)
			{
				(void)IntersectSegment({Nan, 0, 0}, {0, 0, 10}, FSphere{{0, 0, 5}, 1});
			}
			if (Case == 2)
			{
				(void)IntersectSegment({0, 0, 0}, {0, 0, 10}, FOBB{{0, 0, 5}, {-1, 1, 1}});
			}
		}
		catch (const FException&)
		{
			Failed = true;
		}
		REQUIRE(Failed);
	}
	const FSphere Sphere{{0, 0, 5}, 1};
	REQUIRE(!IntersectSegment({1.001f, 0, 0}, {1.001f, 0, 10}, Sphere));
	REQUIRE(IntersectSegment({0.999f, 0, 0}, {0.999f, 0, 10}, Sphere));
	auto View = Camera_Internal();
	View.FarPlane = TNumericLimits<f32>::Max();
	REQUIRE(!MakeViewPickSegment(View, 800, 100, {799, 50}));
	const f32 Inf = View.FarPlane * 2.0f;
	REQUIRE(!ProjectWorldToScreen(View, 800, 100, {Inf, 0, 5}));
}
