// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Log.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Mutex.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace Toolbox
{
namespace
{
// 出力先と重要度の共有状態。静的初期化順序に依存しないよう初回参照時に作る。
struct FLogState
{
	// 出力先の差し替えと呼出しを直列化する。
	FMutex Mutex;
	// 現在の出力先。nullptrは既定の出力先。
	FLogSink Sink = nullptr;
	// 出力先へ渡す値。
	void* UserData = nullptr;
	// 実行時の最小重要度。
	TAtomic<int32> Level{DXF_LOG_COMPILE_LEVEL};
};
FLogState& State_Internal() noexcept
{
	// 全スレッドで共有する唯一の状態。
	static FLogState State;
	return State;
}
// 出力先の中から発行されたログを捨て、同じMutexの再取得による停止を防ぐ。
thread_local bool bInSink = false;
// ファイルパスから表示用の末尾名を取り出す。
const char* FileName_Internal(const char* Path) noexcept
{
	const char* Name = Path;
	for (const char* Cursor = Path; *Cursor != 0; ++Cursor)
	{
		if (*Cursor == '/' || *Cursor == '\\')
		{
			Name = Cursor + 1;
		}
	}
	return Name;
}
// 既定の出力先。Windowsでは日本語を保つためUTF-16へ変換してデバッガへ送り、標準エラーにも書く。
void DefaultSink_Internal(void*, const FLogRecord& Record) noexcept
{
	// 1行へ整形した結果。
	char Line[MaxLogMessageBytes + 256];
	snprintf(Line, sizeof(Line), "[%s][%s] %s (%s:%d)\n", GetLogLevelName(Record.Level), Record.Category,
	         Record.Message, FileName_Internal(Record.File), static_cast<int>(Record.Line));
#if defined(_WIN32)
	// デバッガ出力用のUTF-16表現。
	wchar_t Wide[MaxLogMessageBytes + 256];
	const int Converted = MultiByteToWideChar(CP_UTF8, 0, Line, -1, Wide, static_cast<int>(sizeof(Wide) / sizeof(Wide[0])));
	if (Converted > 0)
	{
		OutputDebugStringW(Wide);
	}
	else
	{
		OutputDebugStringA(Line);
	}
#endif
	fputs(Line, stderr);
}
} // namespace
void SetLogSink(FLogSink Sink, void* UserData) noexcept
{
	FLogState& State = State_Internal();
	FScopedLock Lock(State.Mutex);
	State.Sink = Sink;
	State.UserData = Sink != nullptr ? UserData : nullptr;
}
void SetLogLevel(ELogLevel Level) noexcept
{
	State_Internal().Level.Store(static_cast<int32>(Level));
}
ELogLevel GetLogLevel() noexcept
{
	return static_cast<ELogLevel>(State_Internal().Level.Load());
}
bool IsLogEnabled(ELogLevel Level) noexcept
{
	return Level != ELogLevel::Off && static_cast<int32>(Level) >= State_Internal().Level.Load();
}
const char* GetLogLevelName(ELogLevel Level) noexcept
{
	switch (Level)
	{
	case ELogLevel::Verbose:
		return "VERBOSE";
	case ELogLevel::Info:
		return "INFO";
	case ELogLevel::Warning:
		return "WARNING";
	case ELogLevel::Error:
		return "ERROR";
	case ELogLevel::Off:
		break;
	}
	return "OFF";
}
void Log_Internal(ELogLevel Level, const char* Category, const char* File, int32 Line, const char* Format, ...) noexcept
{
	if (bInSink || !IsLogEnabled(Level))
	{
		return;
	}
	// 書式展開の結果。確保を伴わない固定領域。
	char Message[MaxLogMessageBytes];
	va_list Arguments;
	va_start(Arguments, Format);
	const int Written = vsnprintf(Message, sizeof(Message), Format != nullptr ? Format : "", Arguments);
	va_end(Arguments);
	FLogRecord Record;
	Record.Level = Level;
	Record.Category = Category != nullptr ? Category : "";
	Record.File = File != nullptr ? File : "";
	Record.Line = Line;
	if (Written < 0)
	{
		// 書式自体が不正な場合も記録は残す。
		snprintf(Message, sizeof(Message), "<invalid log format>");
	}
	else if (static_cast<size_t>(Written) >= sizeof(Message))
	{
		// 切り詰めたことを本文の末尾で示す。UTF-8の途中で切れた文字を残さない。
		size_t End = sizeof(Message) - 4;
		while (End > 0 && (static_cast<unsigned char>(Message[End]) & 0xC0u) == 0x80u)
		{
			--End;
		}
		memcpy(Message + End, "...", 4);
		Record.bTruncated = true;
	}
	Record.Message = Message;
	FLogState& State = State_Internal();
	FScopedLock Lock(State.Mutex);
	bInSink = true;
	if (State.Sink != nullptr)
	{
		State.Sink(State.UserData, Record);
	}
	else
	{
		DefaultSink_Internal(nullptr, Record);
	}
	bInSink = false;
}
} // namespace Toolbox
