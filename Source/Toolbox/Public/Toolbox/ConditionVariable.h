// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_CONDITION_VARIABLE_H
#define TOOLBOX_CONDITION_VARIABLE_H
#include "Toolbox/Mutex.h"
namespace Toolbox
{
/**
 * Mutexで守られた状態変化を待つ条件変数。WaitはMutexを一時解放してから再取得する。
 */
class FConditionVariable
{
public:
	/**
	 * OSの条件変数を初期化する。初期化失敗は例外で通知する。
	 */
	FConditionVariable();
	/**
	 * 待機者がいない状態でOSの条件変数を破棄する。
	 */
	~FConditionVariable();
	/**
	 * 条件変数そのものを複製できないためコピーを禁止する。
	 */
	FConditionVariable(const FConditionVariable&) = delete;
	/**
	 * 条件変数そのものを複製できないためコピー代入を禁止する。
	 */
	FConditionVariable& operator=(const FConditionVariable&) = delete;
	/**
	 * 通知されるまで待つ。呼び出し時にMutexを保持している必要がある。
	 * 疑似起床を許すため、呼び出し側は条件をwhileで再確認する。
	 * @param Mutex 待機条件を保護しているMutex。
	 */
	void Wait(FMutex& Mutex) noexcept;
	/**
	 * 待機中の一つのスレッドを起こす。
	 */
	void NotifyOne() noexcept;
	/**
	 * 待機中の全スレッドを起こす。
	 */
	void NotifyAll() noexcept;

private:
	/**
	 * OS固有条件変数を隠す実装型。
	 */
	struct FImpl;
	/**
	 * OS固有条件変数の所有領域。
	 */
	FImpl* m_pImpl = nullptr;
};
} // namespace Toolbox
#endif
