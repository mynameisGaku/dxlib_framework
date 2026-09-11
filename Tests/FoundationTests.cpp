#include "Support/Test.h"
#include "Dxf/Result.h"
#include "Dxf/Object.h"
#include "Dxf/SlotMap.h"
#include "Dxf/Clock.h"
#include "Dxf/Input.h"
#include <cmath>
#include <memory>
#include <limits>
using namespace Dxf;
namespace
{
class DTestObject : public DObject
{
};
class DOtherObject : public DObject
{
};
}
TEST("Result holds a move-only value")
{
	auto Result = TResult<std::unique_ptr<int>>::Success(std::make_unique<int>(42));
	REQUIRE(Result);
	REQUIRE(*Result.Value() == 42);
	auto Value = std::move(Result).Value();
	REQUIRE(*Value == 42);
}
TEST("Result preserves error and void success")
{
	auto Result = TResult<int>::Failure(EErrorCode::InvalidArgument, "bad input");
	REQUIRE(!Result);
	REQUIRE(Result.Error().Message == "bad input");
	REQUIRE(TResult<void>{});
}
TEST("RTTI supports safe derived checks")
{
	DTestObject Object;
	DObject& Base = Object;
	REQUIRE(Base.IsA<DTestObject>());
	REQUIRE(!Base.IsA<DOtherObject>());
	REQUIRE(Base.TryCast<DTestObject>() == &Object);
	REQUIRE(Base.TryCast<DOtherObject>() == nullptr);
}
TEST("SlotMap invalidates removed handles and increments generation")
{
	TSlotMap<DObject> Storage;
	auto Old = Storage.Insert(std::make_unique<DTestObject>());
	REQUIRE(Old.Get());
	REQUIRE(Storage.Remove(Old));
	auto New = Storage.Insert(std::make_unique<DTestObject>());
	REQUIRE(!Old.Get());
	REQUIRE(New.Get());
	REQUIRE(Old.GetId().Index == New.GetId().Index);
	REQUIRE(Old.GetId().Generation != New.GetId().Generation);
}
TEST("Handles do not resolve against a different owner")
{
	TSlotMap<DObject> A;
	TSlotMap<DObject> B;
	auto First = A.Insert(std::make_unique<DTestObject>());
	auto Second = B.Insert(std::make_unique<DTestObject>());
	REQUIRE(!B.Remove(First));
	REQUIRE(Second.Get());
	REQUIRE(First.GetId().Domain != Second.GetId().Domain);
}
TEST("Handle outlives storage without dangling pointer")
{
	TObjectHandle<DObject> Handle;
	{
		TSlotMap<DObject> Storage;
		Handle = Storage.Insert(std::make_unique<DTestObject>());
	}
	REQUIRE(!Handle.Get());
}
TEST("Typed handle rejects incorrect dynamic type")
{
	TSlotMap<DObject> Storage;
	auto Handle = Storage.Insert(std::make_unique<DTestObject>());
	REQUIRE(Handle.Cast<DTestObject>().Get());
	REQUIRE(!Handle.Cast<DOtherObject>().Get());
}
TEST("SlotMap snapshot permits removal without iterator invalidation")
{
	TSlotMap<DObject> Storage;
	for (int Index = 0; Index < 128; ++Index)
	{
		Storage.Insert(std::make_unique<DTestObject>());
	}
	for (auto Handle : Storage.Snapshot())
	{
		REQUIRE(Storage.Remove(Handle));
	}
	REQUIRE(Storage.Size() == 0);
}
TEST("FrameClock clamps simulation delta but retains real delta")
{
	FFrameClock Clock(0.25);
	REQUIRE(Clock.Sample(10.0).Value().DeltaSeconds == 0.0);
	const auto Time = Clock.Sample(11.0).Value();
	REQUIRE(Time.UnscaledDeltaSeconds == 1.0);
	REQUIRE(Time.DeltaSeconds == 0.25);
}
TEST("FrameClock rejects negative and nonfinite elapsed inputs")
{
	FFrameClock Clock;
	REQUIRE(Clock.Sample(3.0));
	REQUIRE(!Clock.Sample(2.0));
	REQUIRE(!Clock.Sample(std::numeric_limits<double>::quiet_NaN()));
	REQUIRE(Clock.Sample(4.0).Value().UnscaledDeltaSeconds == 1.0);
}
TEST("SceneClock pause and scale preserve unscaled time")
{
	FSceneClock Clock;
	REQUIRE(Clock.SetTimeScale(0.5));
	FFrameTime Time;
	Time.DeltaSeconds = 0.1;
	Time.UnscaledDeltaSeconds = 0.1;
	REQUIRE(std::abs(Clock.Advance(Time).DeltaSeconds - 0.05) < 1e-9);
	Clock.SetPaused(true);
	auto Paused = Clock.Advance(Time);
	REQUIRE(Paused.bPaused);
	REQUIRE(Paused.DeltaSeconds == 0.0);
	REQUIRE(Paused.UnscaledDeltaSeconds == 0.1);
	REQUIRE(!Clock.SetTimeScale(-1.0));
}
TEST("Input edges are stable during a frame")
{
	FInputStateTracker Tracker;
	FRawInput State;
	State.Keys[static_cast<std::size_t>(EKey::Space)] = true;
	Tracker.Advance(State);
	REQUIRE(Tracker.GetSnapshot().Pressed(EKey::Space));
	REQUIRE(Tracker.GetSnapshot().Pressed(EKey::Space));
	Tracker.Advance(State);
	REQUIRE(Tracker.GetSnapshot().Down(EKey::Space));
	REQUIRE(!Tracker.GetSnapshot().Pressed(EKey::Space));
	Tracker.Advance({});
	REQUIRE(Tracker.GetSnapshot().Released(EKey::Space));
}
TEST("Focus loss releases keys and mouse buttons")
{
	FInputStateTracker Tracker;
	FRawInput State;
	State.Keys[static_cast<std::size_t>(EKey::A)] = true;
	State.MouseButtons[0] = true;
	Tracker.Advance(State);
	State.bFocused = false;
	Tracker.Advance(State);
	REQUIRE(!Tracker.GetSnapshot().Down(EKey::A));
	REQUIRE(Tracker.GetSnapshot().Released(EKey::A));
	REQUIRE(Tracker.GetSnapshot().MouseReleased(EMouseButton::Left));
}
TEST("Gamepad disconnection emits release and clears axes")
{
	FInputStateTracker Tracker;
	FRawInput State;
	State.Pads[0].bConnected = true;
	State.Pads[0].Buttons[0] = true;
	State.Pads[0].LeftX = 0.8f;
	Tracker.Advance(State);
	REQUIRE(Tracker.GetSnapshot().PadPressed(0, 0));
	State.Pads[0].bConnected = false;
	Tracker.Advance(State);
	REQUIRE(Tracker.GetSnapshot().PadReleased(0, 0));
	REQUIRE(Tracker.GetSnapshot().GetRaw().Pads[0].LeftX == 0.0f);
}
TEST("InputMap action is held until all bound keys release")
{
	FInputMap Map;
	Map.Bind("Jump", EKey::Space);
	Map.Bind("Jump", EKey::W);
	FInputStateTracker Tracker;
	FRawInput Raw;
	Raw.Keys[static_cast<std::size_t>(EKey::Space)] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.Pressed("Jump"));
	Raw.Keys[static_cast<std::size_t>(EKey::W)] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(!Map.Pressed("Jump"));
	Raw.Keys[static_cast<std::size_t>(EKey::Space)] = false;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.Down("Jump"));
	REQUIRE(!Map.Released("Jump"));
	Tracker.Advance({});
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.Released("Jump"));
}
