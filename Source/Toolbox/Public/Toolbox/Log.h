// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_LOG_H
#define TOOLBOX_LOG_H
#include "Toolbox/Compiler.h"
#include "Toolbox/Utility.h"
/**
 * printf形式の引数検査。GCC/Clangでは書式と引数の不一致を警告にする。
 */
#if defined(__clang__) || defined(__GNUC__)
#define DXF_LOG_PRINTF_FORMAT(FormatIndex, FirstArgument) __attribute__((format(printf, FormatIndex, FirstArgument)))
#else
#define DXF_LOG_PRINTF_FORMAT(FormatIndex, FirstArgument)
#endif
/**
 * コンパイル時に残す最小の重要度。0=Verbose、1=Info、2=Warning、3=Error、4=すべて除去。
 * 未指定時はDebug構成でVerbose、Release構成（NDEBUG）でInfo。除去された呼出しは引数も評価しない。
 */
#ifndef DXF_LOG_COMPILE_LEVEL
#if defined(NDEBUG)
#define DXF_LOG_COMPILE_LEVEL 1
#else
#define DXF_LOG_COMPILE_LEVEL 0
#endif
#endif
namespace Toolbox
{
/**
 * ログの重要度。値が大きいほど重要。
 */
enum class ELogLevel : uint8
{
	/**
	 * 詳細な追跡。通常はDebug構成だけで出力する。
	 */
	Verbose,
	/**
	 * 状態遷移など、動作の把握に使う情報。
	 */
	Info,
	/**
	 * 処理は続くが、想定外の入力や劣化がある。
	 */
	Warning,
	/**
	 * 要求を完了できなかった失敗。
	 */
	Error,
	/**
	 * 実行時にすべてのログを止める設定値。記録の重要度には使わない。
	 */
	Off
};
/**
 * 出力先へ渡す1件の記録。文字列は呼出し中だけ有効で、出力先は保持しない。
 */
struct FLogRecord
{
	/**
	 * 重要度。
	 */
	ELogLevel Level = ELogLevel::Info;
	/**
	 * 発生元の分類名。例えば"Scene"や"Renderer"。
	 */
	const char* Category = "";
	/**
	 * 書式展開済みのUTF-8本文。上限を超えた場合は末尾を"..."にして切り詰める。
	 */
	const char* Message = "";
	/**
	 * 呼出し元のソースファイル。
	 */
	const char* File = "";
	/**
	 * 呼出し元の行番号。
	 */
	int32 Line = 0;
	/**
	 * 本文が上限で切り詰められたか。
	 */
	bool bTruncated = false;
};
/**
 * ログの出力先。例外を送出しないこと。出力先の中で発行したログは再入防止のため捨てる。
 * @param UserData SetLogSinkへ渡した値。
 * @param Record 呼出し中だけ有効な記録。
 */
using FLogSink = void (*)(void* UserData, const FLogRecord& Record);
/**
 * 1件の本文の最大バイト数。終端文字を含む。
 */
inline constexpr size_t MaxLogMessageBytes = 1024;
/**
 * 出力先を差し替える。nullptrは既定の出力先（WindowsではVisual Studioの出力と標準エラー）に戻す。
 * 出力先の呼出しは内部Mutexで直列化し、複数スレッドの記録が1行の中で混ざらない。
 * @param Sink 新しい出力先。
 * @param UserData 出力先へ毎回渡す値。出力先を戻すまで生存させる。
 */
void SetLogSink(FLogSink Sink, void* UserData) noexcept;
/**
 * 実行時に出力する最小の重要度を設定する。既定はDXF_LOG_COMPILE_LEVELと同じ。
 * @param Level この重要度以上を出力する。Offはすべて止める。
 */
void SetLogLevel(ELogLevel Level) noexcept;
/**
 * 実行時に出力する最小の重要度。
 */
ELogLevel GetLogLevel() noexcept;
/**
 * 指定した重要度を現在出力するか。
 * @param Level 調べる重要度。
 */
bool IsLogEnabled(ELogLevel Level) noexcept;
/**
 * 重要度の短い表示名。例えば"INFO"。
 * @param Level 表示する重要度。
 */
const char* GetLogLevelName(ELogLevel Level) noexcept;
/**
 * マクロから呼ぶ記録処理。書式展開はスタック上の固定領域で行い、確保や例外を伴わない。
 * @param Level 重要度。
 * @param Category 発生元の分類名。
 * @param File 呼出し元のソースファイル。
 * @param Line 呼出し元の行番号。
 * @param Format printf形式のUTF-8書式。
 */
void Log_Internal(ELogLevel Level, const char* Category, const char* File, int32 Line, const char* Format,
                  ...) noexcept DXF_LOG_PRINTF_FORMAT(5, 6);
} // namespace Toolbox
/**
 * 重要度と分類名を指定して記録する。書式と引数はprintf形式。
 * コンパイル時に除去された重要度や、実行時に無効な重要度では引数を評価しない。
 */
#define DXF_LOG(Level, Category, ...)                                                                             \
	do                                                                                                             \
	{                                                                                                              \
		if constexpr (static_cast<int>(Level) >= DXF_LOG_COMPILE_LEVEL)                                            \
		{                                                                                                          \
			if (::Toolbox::IsLogEnabled(Level))                                                                    \
			{                                                                                                      \
				::Toolbox::Log_Internal(Level, Category, __FILE__, __LINE__, __VA_ARGS__);                         \
			}                                                                                                      \
		}                                                                                                          \
	} while (false)
/**
 * 詳細な追跡を記録する。
 */
#define DXF_LOG_VERBOSE(Category, ...) DXF_LOG(::Toolbox::ELogLevel::Verbose, Category, __VA_ARGS__)
/**
 * 状態遷移などの情報を記録する。
 */
#define DXF_LOG_INFO(Category, ...) DXF_LOG(::Toolbox::ELogLevel::Info, Category, __VA_ARGS__)
/**
 * 処理を続けられる想定外の状態を記録する。
 */
#define DXF_LOG_WARNING(Category, ...) DXF_LOG(::Toolbox::ELogLevel::Warning, Category, __VA_ARGS__)
/**
 * 要求を完了できなかった失敗を記録する。
 */
#define DXF_LOG_ERROR(Category, ...) DXF_LOG(::Toolbox::ELogLevel::Error, Category, __VA_ARGS__)
#endif
