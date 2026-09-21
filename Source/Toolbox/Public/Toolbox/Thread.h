// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_THREAD_H
#define TOOLBOX_THREAD_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
// Windows.hのYieldマクロと宣言が衝突するため一時的に退避する。
#ifdef Yield
#pragma push_macro("Yield")
#undef Yield
#define TOOLBOX_YIELD_MACRO_RESTORED
#endif
/**
 * OSスレッドを所有し、開始とJoinを管理する。
 */
using FThreadEntry = void (*)(void* Context);
/**
 * 単一のOSスレッドを所有する。破棄時は実行中ならJoinする。
 * 同じFThreadインスタンスのStart・Join・移動・破棄は外側で単独所有する。
 */
class FThread
{
public:
	/**
	 * OSスレッドをまだ所有しない空の状態を作る。
	 */
	FThread() noexcept = default;
	/**
	 * 所有するスレッドが残っていればJoinしてOS資源を解放する。
	 */
	~FThread();
	/**
	 * OSスレッドの所有権を複製できないためコピーを禁止する。
	 */
	FThread(const FThread&) = delete;
	/**
	 * OSスレッドの所有権を複製できないためコピー代入を禁止する。
	 */
	FThread& operator=(const FThread&) = delete;
	/**
	 * 実行中Threadの所有権を移す。元の値は空になる。
	 * @param Other 所有権を移すThread。
	 */
	FThread(FThread&& Other) noexcept;
	/**
	 * 現在のThreadをJoinした後、別Threadの所有権を受け取る。
	 * @param Other 所有権を移すThread。
	 */
	FThread& operator=(FThread&& Other) noexcept;
	/**
	 * 新しいスレッドを開始する。既に実行中またはOS生成失敗ならfalse。
	 * Entryから外へ出た例外はOS境界で捕捉してThreadを終了する。
	 * @param Entry スレッドで実行する関数。
	 * @param Context Entryへ渡す利用者コンテキスト。
	 */
	bool Start(FThreadEntry Entry, void* Context);
	/**
	 * Join待ちが必要なスレッドを所有しているか返す。
	 */
	bool IsJoinable() const noexcept;
	/**
	 * 実行終了を待ち、OSハンドルを解放する。未開始なら何もしない。
	 * 所有しているThread自身から呼び出してはいけない。
	 */
	void Join() noexcept;
	/**
	 * 呼び出し中スレッドをプロセス内で比較できる整数として返す。
	 */
	static uint64 CurrentThreadId() noexcept;
	/**
	 * OSが報告する論理実行スレッド数を返す。取得失敗時は1。
	 */
	static uint32 HardwareThreadCount() noexcept;
	/**
	 * 実行権を同優先度の別スレッドへ譲る。待機条件の代用にはしない。
	 */
	static void Yield() noexcept;

private:
	/**
	 * OS固有Threadハンドルと実行入口を隠す実装型。
	 */
	struct FImpl;
	/**
	 * OS固有Thread資源の排他的所有領域。未開始時はnullptrを許す。
	 */
	FImpl* m_pImpl = nullptr;
};
// 退避したYieldマクロを元に戻す。
#ifdef TOOLBOX_YIELD_MACRO_RESTORED
#pragma pop_macro("Yield")
#undef TOOLBOX_YIELD_MACRO_RESTORED
#endif
} // namespace Toolbox
#endif
