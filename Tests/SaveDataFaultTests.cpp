// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Dxf/SaveStore.h"
#include "Dxf/SaveCodec.h"
#include "Toolbox/JsonWriter.h"
#include "../Source/Toolbox/Private/Toolbox/Testing/AllocationFault.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
// 注入中の確保回数と実注入を、終了処理より前に読み取る。
struct FInjection
{
	// 実際に失敗を発生させたか。
	bool& Actual;
	// 途中例外でも、後続の検査を通常確保へ戻す。
	~FInjection()
	{
		Actual = Actual || Testing::WasAllocationFailureInjected();
		Testing::SetAllocationFailureCountdown(-1);
	}
};
// 保存コアにゲームの構造を固定せず、小さい論理値で境界を検査する。
FSaveDocument Value()
{
	FSaveDocument Result;
	Result.GameId = "fault_game";
	Result.ContentRevision = "fixture-v1";
	Result.SceneKey = "main";
	Result.CheckpointKey = "checkpoint";
	Result.Payload = "{\"score\":42,\"key\":true}";
	return Result;
}
// 一地点ずつ実際の確保を故障化する。非注入終了を記録して探索を終える。
template <typename Function>
void Sweep(const char* Name, Function Run)
{
	bool Completed = false;
	for (int64 Countdown = 0; Countdown < 4096; ++Countdown)
	{
		// 終了後に残る所有領域を比較する。
		const uint64 Before = Testing::GetOutstandingTestAllocations();
		bool Actual = false;
		bool Failed = false;
		try
		{
			FInjection Injection{Actual};
			Run(Countdown);
		}
		catch (...)
		{
			Failed = true;
		}
		REQUIRE(Actual ? Failed : !Failed);
		REQUIRE(Testing::GetOutstandingTestAllocations() == Before);
		Out << FString("SAVE_FAULT ") + Name + " countdown=" + ToString(Countdown) + " injected=" + ToString(Actual ? int32(1) : int32(0)) + " failed=" + ToString(Failed ? int32(1) : int32(0)) + " outstanding_delta=0" << '\n';
		if (!Actual)
		{
			Completed = true;
			break;
		}
	}
	REQUIRE(Completed);
}
} // namespace
TEST("Save fault codec encode and decode preserve caller value")
{
	const FSaveDocument Document = Value();
	const FString Bytes = FSaveCodec::Encode(Document);
	Sweep("encode", [&](int64 Countdown)
	      {
		      Testing::SetAllocationFailureCountdown(Countdown);
		      const FString Encoded = FSaveCodec::Encode(Document);
		      REQUIRE(Encoded == Bytes);
	      });
	Sweep("decode", [&](int64 Countdown)
	      {
		      Testing::SetAllocationFailureCountdown(Countdown);
		      const FSaveDocument Decoded = FSaveCodec::Decode(Bytes);
		      REQUIRE(Decoded.GameId == Document.GameId);
	      });
	REQUIRE(FSaveCodec::Encode(Document) == Bytes);
}
TEST("Save fault stage keeps old face and normal recovery works")
{
	const FPath Root = FPath(DXF_SAVE_TEST_ROOT) / FPath(FString("fault-") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	FSaveStore Store(Root);
	const FSaveDocument Document = Value();
	REQUIRE(Store.Save("manual", Document) == 1);
	Sweep("stage", [&](int64 Countdown)
	      {
		      Testing::SetAllocationFailureCountdown(Countdown);
		      FSaveTransaction Transaction = Store.Stage("manual", Document);
		      // 公開前に破棄し、正常面は変更しない。破棄自体は確保を伴わない。
	      });
	REQUIRE(Store.Load("manual").Document->Generation == 1);
	REQUIRE(Store.Save("manual", Document) == 2);
}
TEST("Save fault transaction destruction makes no allocation")
{
	const FPath Root = FPath(DXF_SAVE_TEST_ROOT) / FPath(FString("destructor-") + FJsonWriter::Unsigned(MonotonicNanoseconds()));
	FSaveStore Store(Root);
	bool Actual = false;
	{
		FSaveTransaction Transaction = Store.Stage("manual", Value());
		Testing::SetAllocationFailureCountdown(0);
	}
	Actual = Testing::WasAllocationFailureInjected();
	Testing::SetAllocationFailureCountdown(-1);
	REQUIRE(!Actual);
	REQUIRE(Store.Load("manual").State == ESaveLoadState::Empty);
	REQUIRE(Store.Save("manual", Value()) == 1);
}
