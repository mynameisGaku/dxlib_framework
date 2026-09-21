// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/DebugDrawStore.h"
namespace
{
Dxf::FGeometryCommand3D Marker_Internal()
{
	Dxf::FGeometryCommand3D Value;
	Value.Geometry.Lines.PushBack({{0,0,0},{1,0,0}});
	return Value;
}
}
TEST("debug categories owner generations and scopes are distinct")
{
	Dxf::FDebugDrawStore3D Store(4);
	Dxf::FDebugDrawTag Tag;
	Tag.Owner = {1, 2, 3};
	Tag.Scope = {1, 9, 1};
	Tag.Category = 4;
	Tag.Lifetime = Dxf::EDebugLifetime::Persistent;
	REQUIRE(Store.Add(Marker_Internal(), Tag).Value());
	Dxf::FDebugDrawFilter Filter;
	Filter.Categories = 4;
	Filter.bSelectedOnly = true;
	Filter.Selected = Tag.Owner;
	REQUIRE(Store.Snapshot(Filter).Value().Size() == 1);
	Filter.Selected.Generation = 4;
	REQUIRE(Store.Snapshot(Filter).Value().IsEmpty());
	Filter.bSelectedOnly = false;
	Filter.Categories = 2;
	REQUIRE(Store.Snapshot(Filter).Value().IsEmpty());
	REQUIRE(Store.ClearScope({1,9,2}) == 0);
	REQUIRE(Store.ClearScope(Tag.Scope) == 1);
}
TEST("debug lifespan distinguishes frame real time and paused game time")
{
	Dxf::FDebugDrawStore3D Store;
	Dxf::FDebugDrawTag Tag;
	REQUIRE(Store.Add(Marker_Internal(), Tag).Value());
	Tag.Lifetime = Dxf::EDebugLifetime::Seconds;
	Tag.Seconds = 1;
	Tag.Clock = Dxf::EDebugClock::Game;
	REQUIRE(Store.Add(Marker_Internal(), Tag).Value());
	Tag.Clock = Dxf::EDebugClock::Real;
	REQUIRE(Store.Add(Marker_Internal(), Tag).Value());
	REQUIRE(Store.Advance(1, 0, 0.5));
	REQUIRE(Store.Snapshot({}).Value().Size() == 2);
	REQUIRE(Store.Advance(2, 0, 0.5));
	REQUIRE(Store.Snapshot({}).Value().Size() == 1);
	REQUIRE(Store.Advance(3, 1, 0.1));
	REQUIRE(Store.Snapshot({}).Value().IsEmpty());
}
TEST("debug budget and disabled collection are explicit and bounded")
{
	Dxf::FDebugDrawStore3D Store(1);
	Dxf::FDebugDrawTag Tag;
	REQUIRE(Store.SetEnabledCategories(2));
	REQUIRE(!Store.Add(Marker_Internal(), Tag).Value());
	REQUIRE(Store.Snapshot({}).Value().IsEmpty());
	Tag.Category = 2;
	REQUIRE(Store.Add(Marker_Internal(), Tag).Value());
	REQUIRE(!Store.Add(Marker_Internal(), Tag).Value());
	REQUIRE(Store.GetDroppedCount() == 1);
	REQUIRE(Store.Snapshot({}).Value().Size() == 1);
}
TEST("invalid debug clock update leaves previously recorded values alive")
{
	Dxf::FDebugDrawStore3D Store;
	REQUIRE(Store.Add(Marker_Internal(), {}).Value());
	REQUIRE(!Store.Advance(1, -1, 0));
	REQUIRE(Store.Snapshot({}).Value().Size() == 1);
	REQUIRE(Store.Advance(1, 0, 0));
	REQUIRE(!Store.Advance(1, 1, 1));
}
TEST("debug 2D store contains data only and rejects nonfinite positions")
{
	Dxf::FDebugDrawStore2D Store;
	Dxf::FLineCommand2D Line;
	Line.End = {20, 30};
	REQUIRE(Store.Add(Line, {}).Value());
	REQUIRE(Store.Snapshot({}).Value()[0].End.X == 20);
	Line.End.X = Toolbox::TNumericLimits<Toolbox::f32>::Max();
	REQUIRE(!Store.Add(Line, {}));
}
#include "Dxf/DebugDrawAdapters.h"
TEST("debug adapter uses the dimension context without native or physics ownership")
{
	Dxf::FDebugDrawStore3D Store;
	REQUIRE(Store.Add(Marker_Internal(), {}).Value());
	Dxf::FRenderQueue2D Queue;
	Toolbox::FJobSystem Jobs(2);
	Dxf::FRenderContext Render(Queue, nullptr, &Jobs);
	Queue.SetAccepting_Internal(true);
	REQUIRE(Dxf::SubmitDebugDraw(Store, {}, Render.Get3D()));
	REQUIRE(Store.Snapshot({}).Value().Size() == 1);
}
TEST("debug history accepts the first frame zero but rejects its duplicate")
{
	Dxf::FDebugDrawStore3D Store;
	REQUIRE(Store.Advance(0, 0, 0));
	REQUIRE(!Store.Advance(0, 0, 0));
	REQUIRE(Store.Advance(1, 0, 0));
}
TEST("debug persistent records without scopes can be cleared and budget reused")
{
	Dxf::FDebugDrawStore3D Store(1);
	Dxf::FDebugDrawTag Tag;
	Tag.Lifetime = Dxf::EDebugLifetime::Persistent;
	REQUIRE(Store.Add(Marker_Internal(), Tag).Value());
	REQUIRE(Store.Clear());
	REQUIRE(Store.Snapshot({}).Value().IsEmpty());
	REQUIRE(Store.Add(Marker_Internal(), Tag).Value());
}
