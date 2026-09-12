#include "Toolbox/UniquePtr.h"
#include "Support/Test.h"
#include "Dxf/Result.h"
#include "Dxf/Object.h"
#include "Dxf/SlotMap.h"
#include "Dxf/Clock.h"
#include "Dxf/Input.h"
#include "Toolbox/Utility.h"
#include "Toolbox/SharedPtr.h"
using namespace Dxf;
namespace
{
// オブジェクト登録と型判定を検証する最小実装。
class DTestObject : public DObject
{
};
// 型変換を拒否すべき別種のオブジェクト。
class DOtherObject : public DObject
{
};
} // namespace
TEST("Result holds a move-only value")
{
	// 検証対象の操作が返した成否と値。
	auto Result = TResult<Toolbox::TUniquePtr<Toolbox::int32>>::Success(Toolbox::MakeUnique<Toolbox::int32>(42));
	REQUIRE(Result);
	REQUIRE(*Result.Value() == 42);
	// 結果から取り出した値。
	auto Value = Toolbox::Move(Result).Value();
	REQUIRE(*Value == 42);
}
TEST("Result preserves error and void success")
{
	// 検証対象の操作が返した成否と値。
	auto Result = TResult<Toolbox::int32>::Failure(EErrorCode::InvalidArgument, "bad input");
	REQUIRE(!Result);
	REQUIRE(Result.Error().Message == "bad input");
	REQUIRE(TResult<void>{});
}
TEST("RTTI supports safe derived checks")
{
	// 検証対象のオブジェクト。
	DTestObject Object;
	// 基底型を通じた型判定の参照。
	DObject& Base = Object;
	REQUIRE(Base.IsA<DTestObject>());
	REQUIRE(!Base.IsA<DOtherObject>());
	REQUIRE(Base.TryCast<DTestObject>() == &Object);
	REQUIRE(Base.TryCast<DOtherObject>() == nullptr);
}
TEST("SlotMap invalidates removed handles and increments generation")
{
	// 検証対象を所有する世代付き格納先。
	TSlotMap<DObject> Storage;
	// 削除または置換する前の登録。
	auto Old = Storage.Insert(Toolbox::MakeUnique<DTestObject>());
	REQUIRE(Old.Get());
	REQUIRE(Storage.Remove(Old));
	// 同じ格納先へ再登録した対象。
	auto New = Storage.Insert(Toolbox::MakeUnique<DTestObject>());
	REQUIRE(!Old.Get());
	REQUIRE(New.Get());
	REQUIRE(Old.GetId().Index == New.GetId().Index);
	REQUIRE(Old.GetId().Generation != New.GetId().Generation);
}
TEST("Handles do not resolve against a different owner")
{
	TSlotMap<DObject> A;
	TSlotMap<DObject> B;
	// 最初に生成または登録した対象。
	auto First = A.Insert(Toolbox::MakeUnique<DTestObject>());
	// 二番目に生成または登録した対象。
	auto Second = B.Insert(Toolbox::MakeUnique<DTestObject>());
	REQUIRE(!B.Remove(First));
	REQUIRE(Second.Get());
	REQUIRE(First.GetId().Domain != Second.GetId().Domain);
}
TEST("Handle outlives storage without dangling pointer")
{
	// 生存期間や世代を検証する登録ハンドル。
	TObjectHandle<DObject> Handle;
	{
		// 検証対象を所有する世代付き格納先。
		TSlotMap<DObject> Storage;
		Handle = Storage.Insert(Toolbox::MakeUnique<DTestObject>());
	}
	REQUIRE(!Handle.Get());
}
TEST("Typed handle rejects incorrect dynamic type")
{
	// 検証対象を所有する世代付き格納先。
	TSlotMap<DObject> Storage;
	// 生存期間や世代を検証する登録ハンドル。
	auto Handle = Storage.Insert(Toolbox::MakeUnique<DTestObject>());
	REQUIRE(Handle.Cast<DTestObject>().Get());
	REQUIRE(!Handle.Cast<DOtherObject>().Get());
}
TEST("SlotMap snapshot permits removal without iterator invalidation")
{
	// 検証対象を所有する世代付き格納先。
	TSlotMap<DObject> Storage;
	for (Toolbox::int32 Index = 0; Index < 128; ++Index)
	{
		Storage.Insert(Toolbox::MakeUnique<DTestObject>());
	}
	for (auto Handle : Storage.Snapshot())
	{
		REQUIRE(Storage.Remove(Handle));
	}
	REQUIRE(Storage.Size() == 0);
}
TEST("FrameClock clamps simulation delta but retains real delta")
{
	// 実時間とシミュレーション時間を分ける時計。
	FFrameClock Clock(0.25);
	REQUIRE(Clock.Sample(10.0).Value().DeltaSeconds == 0.0);
	// サンプリングしたフレーム時刻。
	const auto Time = Clock.Sample(11.0).Value();
	REQUIRE(Time.UnscaledDeltaSeconds == 1.0);
	REQUIRE(Time.DeltaSeconds == 0.25);
}
TEST("FrameClock rejects negative and nonfinite elapsed inputs")
{
	// 実時間とシミュレーション時間を分ける時計。
	FFrameClock Clock;
	REQUIRE(Clock.Sample(3.0));
	REQUIRE(!Clock.Sample(2.0));
	REQUIRE(!Clock.Sample(Toolbox::TNumericLimits<Toolbox::f64>::QuietNaN()));
	REQUIRE(Clock.Sample(4.0).Value().UnscaledDeltaSeconds == 1.0);
}
TEST("SceneClock pause and scale preserve unscaled time")
{
	// 実時間とシミュレーション時間を分ける時計。
	FSceneClock Clock;
	REQUIRE(Clock.SetTimeScale(0.5));
	// サンプリングしたフレーム時刻。
	FFrameTime Time;
	Time.DeltaSeconds = 0.1;
	Time.UnscaledDeltaSeconds = 0.1;
	REQUIRE(Toolbox::Abs(Clock.Advance(Time).DeltaSeconds - 0.05) < 1e-9);
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
	// 検証する処理の状態。
	FRawInput State;
	State.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = true;
	Tracker.Advance(State);
	REQUIRE(Tracker.GetSnapshot().WasPressed(EKey::Space));
	REQUIRE(Tracker.GetSnapshot().WasPressed(EKey::Space));
	Tracker.Advance(State);
	REQUIRE(Tracker.GetSnapshot().IsDown(EKey::Space));
	REQUIRE(!Tracker.GetSnapshot().WasPressed(EKey::Space));
	Tracker.Advance({});
	REQUIRE(Tracker.GetSnapshot().WasReleased(EKey::Space));
}
TEST("Focus loss releases keys and mouse buttons")
{
	FInputStateTracker Tracker;
	// 検証する処理の状態。
	FRawInput State;
	State.Keys[static_cast<Toolbox::size_t>(EKey::A)] = true;
	State.MouseButtons[0] = true;
	Tracker.Advance(State);
	State.bFocused = false;
	Tracker.Advance(State);
	REQUIRE(!Tracker.GetSnapshot().IsDown(EKey::A));
	REQUIRE(Tracker.GetSnapshot().WasReleased(EKey::A));
	REQUIRE(Tracker.GetSnapshot().WasMouseReleased(EMouseButton::Left));
}
TEST("Gamepad disconnection emits release and clears axes")
{
	FInputStateTracker Tracker;
	// 検証する処理の状態。
	FRawInput State;
	State.Pads[0].bConnected = true;
	State.Pads[0].Buttons[0] = true;
	State.Pads[0].LeftX = 0.8f;
	Tracker.Advance(State);
	REQUIRE(Tracker.GetSnapshot().WasPadPressed(0, 0));
	State.Pads[0].bConnected = false;
	Tracker.Advance(State);
	REQUIRE(Tracker.GetSnapshot().WasPadReleased(0, 0));
	REQUIRE(Tracker.GetSnapshot().GetRaw().Pads[0].LeftX == 0.0f);
}
TEST("InputMap action is held until all bound keys release")
{
	FInputMap Map;
	Map.Bind("Jump", EKey::Space);
	Map.Bind("Jump", EKey::W);
	FInputStateTracker Tracker;
	// 正規化前の入力データ。
	FRawInput Raw;
	Raw.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.WasPressed("Jump"));
	Raw.Keys[static_cast<Toolbox::size_t>(EKey::W)] = true;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(!Map.WasPressed("Jump"));
	Raw.Keys[static_cast<Toolbox::size_t>(EKey::Space)] = false;
	Tracker.Advance(Raw);
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.IsDown("Jump"));
	REQUIRE(!Map.WasReleased("Jump"));
	Tracker.Advance({});
	Map.Update(Tracker.GetSnapshot());
	REQUIRE(Map.WasReleased("Jump"));
}
TEST("Result can carry an error object as successful data without confusing the failure branch")
{
	auto Success = TResult<FError>::Success({EErrorCode::NotFound, "successful data"});
	REQUIRE(Success && Success.Value().Message == "successful data");
	// 失敗が記録された状態。
	auto Failure = TResult<FError>::Failure(EErrorCode::BackendFailure, "actual failure");
	REQUIRE(!Failure && Failure.Error().Message == "actual failure");
}
