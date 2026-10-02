// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/InputProfileCodec.h"
#include "Dxf/InputStateTracker.h"
#include "Dxf/InputRebindCapture.h"
#include "Toolbox/Platform.h"
#include "../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 失敗を含む全分岐で後続の検査を通常確保へ戻す。
struct FInjection
{
	// 実際の確保故障注入が到達したか。
	bool& Actual;
	~FInjection()
	{
		Actual = Testing::WasAllocationFailureInjected();
		Testing::SetAllocationFailureCountdown(-1);
	}
};
FInputProfilePolicy Policy()
{
	// 検証後に返す独立した候補値。
	FInputProfilePolicy Result;
	Result.AllowedActions = {"Jump", "Interact"};
	return Result;
}
FInputProfile Profile()
{
	// 検証後に返す独立した候補値。
	FInputProfile Result;
	// 型付きのキーボード入力。
	FInputBindingDescription Key;
	Key.Key = EKey::K;
	// 対象のマウスボタン割当。
	FInputBindingDescription Mouse;
	Mouse.Device = EInputBindingDevice::Mouse;
	Mouse.MouseButton = EMouseButton::Right;
	Result.Actions = {{"Jump", {Key}}, {"Interact", {Mouse}}};
	return Result;
}
template <typename Function>
void Sweep(const char* Name, Function Run)
{
	// 最初の非注入正常試行へ到達したか。
	bool Completed = false;
	for (int64 Countdown = 0; Countdown < 4096; ++Countdown)
	{
		// 区間開始時の確保件数。
		const uint64 Before = Testing::GetOutstandingTestAllocations();
		// 実際の確保故障注入が到達したか。
		bool Actual = false;
		// 注入した処理が例外で失敗したか。
		bool Failed = false;
		Run(Countdown, Actual, Failed);
		REQUIRE(Actual ? Failed : !Failed);
		REQUIRE(Testing::GetOutstandingTestAllocations() == Before);
		Out << FString("PROFILE_FAULT ") + Name + " countdown=" + ToString(Countdown) + " injected=" + ToString(Actual ? int32(1) : int32(0)) + " failed=" + ToString(Failed ? int32(1) : int32(0)) + " outstanding_delta=0" << '\n';
		if (!Actual)
		{
			Completed = true;
			break;
		}
	}
	REQUIRE(Completed);
}
} // namespace
TEST("Profile allocation failure cannot publish a partial input map and retry succeeds")
{
	// このゲームが許可するActionと予約入力。
	const auto Rules = Policy();
	// この境界で検査する入力値。
	const auto Value = Profile();
	// 繰り返し検査で再確保しないAction名、または変更するJump割当。
	const FString Jump = "Jump";
	Sweep("apply", [&](int64 Countdown, bool& Actual, bool& Failed)
	      {
		      // 既存Action更新を行う本体Map。
		      FInputMap Map;
		      REQUIRE(Map.Bind(Jump, EKey::Space));
		      // 既存Snapshot履歴を一度ずつ更新するTracker。
		      FInputStateTracker Tracker;
		      // 試験が明示する固定入力値。
		      FRawInput Raw;
		      Raw.Keys[static_cast<size_t>(EKey::Space)] = true;
		      Tracker.Advance(Raw);
		      Map.Update(Tracker.GetSnapshot());
		      try
		      {
			      FInjection Injection{Actual};
			      Testing::SetAllocationFailureCountdown(Countdown);
			      FInputProfileCodec::Apply(Value, Rules, Map, Tracker.GetSnapshot());
		      }
		      catch (...)
		      {
			      Failed = true;
		      }
		      if (Actual)
		      {
			      REQUIRE(Map.WasPressed(Jump));
			      Map.Update(Tracker.GetSnapshot());
			      REQUIRE(Map.IsDown(Jump));
		      }
		      FInputProfileCodec::Apply(Value, Rules, Map, Tracker.GetSnapshot());
		      Map.Update(Tracker.GetSnapshot());
		      REQUIRE(!Map.IsDown(Jump));
		      Raw.Keys[static_cast<size_t>(EKey::K)] = true;
		      Tracker.Advance(Raw);
		      Map.Update(Tracker.GetSnapshot());
		      REQUIRE(Map.WasPressed(Jump));
	      });
}
TEST("Profile codec allocation failure preserves caller values and normal recovery")
{
	// このゲームが許可するActionと予約入力。
	const auto Rules = Policy();
	// この境界で検査する入力値。
	const auto Value = Profile();
	// 照合する長さ付き保存表現。
	const FString Bytes = FInputProfileCodec::Encode(Value, Rules);
	for (uint32 Operation = 0; Operation < 2; ++Operation)
	{
		Sweep(Operation == 0 ? "encode" : "decode", [&](int64 Countdown, bool& Actual, bool& Failed)
		      {
			      try
			      {
				      FInjection Injection{Actual};
				      Testing::SetAllocationFailureCountdown(Countdown);
				      if (Operation == 0)
				      {
					      (void)FInputProfileCodec::Encode(Value, Rules);
				      }
				      else
				      {
					      (void)FInputProfileCodec::Decode(Bytes, Rules);
				      }
			      }
			      catch (...)
			      {
				      Failed = true;
			      }
			      REQUIRE(FInputProfileCodec::Encode(Value, Rules) == Bytes);
			      REQUIRE(FInputProfileCodec::Encode(FInputProfileCodec::Decode(Bytes, Rules), Rules) == Bytes);
		      });
	}
}
TEST("Profile action update suppression and rebind poll allocate no extra memory")
{
	// 既存Action更新を行う本体Map。
	FInputMap Map;
	// 既存Snapshot履歴を一度ずつ更新するTracker。
	FInputStateTracker Tracker;
	// 試験が明示する固定入力値。
	FRawInput Raw;
	Tracker.Advance(Raw);
	FInputProfileCodec::Apply(Profile(), Policy(), Map, Tracker.GetSnapshot());
	// 押下の解放を待つ再割当採取。
	FInputRebindCapture Capture;
	// 区間開始時の確保件数。
	const uint64 Before = Testing::GetTotalTestAllocations();
	// 実際の確保故障注入が到達したか。
	bool Actual = false;
	{
		FInjection Injection{Actual};
		Testing::SetAllocationFailureCountdown(0);
		Map.SuppressUntilRelease(Tracker.GetSnapshot());
		(void)Capture.Begin(EInputBindingDevice::Keyboard, 0, Tracker.GetSnapshot());
		for (uint32 Frame = 0; Frame < 32; ++Frame)
		{
			Raw.Keys[static_cast<size_t>(EKey::K)] = (Frame % 2) == 1;
			Tracker.Advance(Raw);
			Map.Update(Tracker.GetSnapshot());
			Capture.Poll(Tracker.GetSnapshot());
		}
		Capture.Cancel();
	}
	REQUIRE(!Actual);
	REQUIRE(Testing::GetTotalTestAllocations() == Before);
}
