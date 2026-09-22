// SPDX-License-Identifier: NOASSERTION
#include "Support/Test.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Log.h"
#include "Toolbox/Thread.h"
#include <string.h>
using namespace Toolbox;
namespace
{
// 受け取った記録を複製して保持するテスト用出力先。
struct FCapturedLog
{
	ELogLevel Level = ELogLevel::Info;
	FString Category;
	FString Message;
	FString File;
	int32 Line = 0;
	bool bTruncated = false;
};
struct FLogCapture
{
	TVector<FCapturedLog> Records;
	FAtomicCounter Count;
	bool bLogFromSink = false;
};
void CaptureSink_Internal(void* UserData, const FLogRecord& Record)
{
	auto& Capture = *static_cast<FLogCapture*>(UserData);
	Capture.Count.FetchAdd(1);
	FCapturedLog Item;
	Item.Level = Record.Level;
	Item.Category = Record.Category;
	Item.Message = Record.Message;
	Item.File = Record.File;
	Item.Line = Record.Line;
	Item.bTruncated = Record.bTruncated;
	Capture.Records.PushBack(Toolbox::Move(Item));
	if (Capture.bLogFromSink)
	{
		// 出力先の中からの記録は捨てられ、同じMutexで停止しない。
		DXF_LOG_ERROR("Nested", "must be dropped");
	}
}
// テストごとに出力先と重要度を差し替え、終了時に既定へ戻す。
class FScopedLogCapture
{
public:
	explicit FScopedLogCapture(FLogCapture& Capture, ELogLevel Level = ELogLevel::Verbose) noexcept
	    : m_PreviousLevel(GetLogLevel())
	{
		SetLogSink(&CaptureSink_Internal, &Capture);
		SetLogLevel(Level);
	}
	~FScopedLogCapture()
	{
		SetLogSink(nullptr, nullptr);
		SetLogLevel(m_PreviousLevel);
	}
	FScopedLogCapture(const FScopedLogCapture&) = delete;
	FScopedLogCapture& operator=(const FScopedLogCapture&) = delete;
private:
	ELogLevel m_PreviousLevel;
};
// 引数が評価された回数を数える。無効な重要度では呼ばれないことを確認する。
int32 CountEvaluation_Internal(int32& Counter)
{
	++Counter;
	return Counter;
}
} // namespace
TEST("log records level category formatted message and source location")
{
	FLogCapture Capture;
	FScopedLogCapture Scope(Capture);
	const int32 ExpectedLine = __LINE__ + 1;
	DXF_LOG_WARNING("Scene", "scope %u/%d failed: %s", 3u, 7, "retire");
	REQUIRE(Capture.Records.Size() == 1);
	const auto& Record = Capture.Records[0];
	REQUIRE(Record.Level == ELogLevel::Warning);
	REQUIRE(Record.Category == "Scene");
	REQUIRE(Record.Message == "scope 3/7 failed: retire");
	REQUIRE(Record.Line == ExpectedLine);
	REQUIRE(strstr(Record.File.CStr(), "LogTests.cpp") != nullptr);
	REQUIRE(!Record.bTruncated);
	REQUIRE(strcmp(GetLogLevelName(ELogLevel::Error), "ERROR") == 0);
}
TEST("log runtime level filters records without evaluating arguments")
{
	FLogCapture Capture;
	FScopedLogCapture Scope(Capture, ELogLevel::Warning);
	int32 Evaluated = 0;
	DXF_LOG_INFO("Filter", "value %d", CountEvaluation_Internal(Evaluated));
	DXF_LOG_VERBOSE("Filter", "value %d", CountEvaluation_Internal(Evaluated));
	REQUIRE(Evaluated == 0);
	REQUIRE(Capture.Records.IsEmpty());
	DXF_LOG_ERROR("Filter", "value %d", CountEvaluation_Internal(Evaluated));
	REQUIRE(Evaluated == 1);
	REQUIRE(Capture.Records.Size() == 1);
	REQUIRE(!IsLogEnabled(ELogLevel::Info) && IsLogEnabled(ELogLevel::Warning));
	SetLogLevel(ELogLevel::Off);
	DXF_LOG_ERROR("Filter", "value %d", CountEvaluation_Internal(Evaluated));
	REQUIRE(Evaluated == 1);
	REQUIRE(Capture.Records.Size() == 1);
	REQUIRE(!IsLogEnabled(ELogLevel::Off));
}
TEST("log truncates long messages at a utf8 boundary and marks them")
{
	FLogCapture Capture;
	FScopedLogCapture Scope(Capture);
	// 3バイト文字だけの長い本文。途中で切れた文字を残さず"..."で終わる。
	FString Long;
	for (int32 Index = 0; Index < 600; ++Index)
	{
		Long += "日";
	}
	DXF_LOG_INFO("Truncate", "%s", Long.CStr());
	REQUIRE(Capture.Records.Size() == 1);
	const auto& Record = Capture.Records[0];
	REQUIRE(Record.bTruncated);
	const size_t Size = Record.Message.Size();
	REQUIRE(Size < MaxLogMessageBytes);
	REQUIRE(Size >= 3 && strcmp(Record.Message.CStr() + Size - 3, "...") == 0);
	REQUIRE((Size - 3) % 3 == 0);
}
TEST("log drops records emitted from inside the sink instead of deadlocking")
{
	FLogCapture Capture;
	Capture.bLogFromSink = true;
	FScopedLogCapture Scope(Capture);
	DXF_LOG_INFO("Outer", "first");
	DXF_LOG_INFO("Outer", "second");
	REQUIRE(Capture.Records.Size() == 2);
	REQUIRE(Capture.Records[1].Message == "second");
}
TEST("log serializes sink calls from several threads")
{
	FLogCapture Capture;
	FScopedLogCapture Scope(Capture);
	FThread Threads[4];
	for (auto& Thread : Threads)
	{
		REQUIRE(Thread.Start(
		    [](void*)
		    {
			    for (int32 Index = 0; Index < 100; ++Index)
			    {
				    // VerboseはRelease構成でコンパイル時に除去されるため、両構成で残るInfoを使う。
				    DXF_LOG_INFO("Thread", "record %d", static_cast<int>(Index));
			    }
		    },
		    nullptr));
	}
	for (auto& Thread : Threads)
	{
		Thread.Join();
	}
	// 出力先はMutexで直列化されるため、同期なしのTVectorへの追加でも件数が一致する。
	REQUIRE(Capture.Count.Load() == 400);
	REQUIRE(Capture.Records.Size() == 400);
}
