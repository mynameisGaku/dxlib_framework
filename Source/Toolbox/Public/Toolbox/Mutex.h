// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_MUTEX_H
#define TOOLBOX_MUTEX_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
class FConditionVariable;
/**
 * 同一プロセス内の排他制御。再帰ロックはサポートしない。
 */
class FMutex
{
public:
	/**
	 * OSの排他制御を初期化する。初期化失敗は例外で通知する。
	 */
	FMutex();
	/**
	 * 待機者がいない状態でOSの排他制御を破棄する。
	 */
	~FMutex();
	/**
	 * 排他制御そのものを複製できないためコピーを禁止する。
	 */
	FMutex(const FMutex&) = delete;
	/**
	 * 排他制御そのものを複製できないためコピー代入を禁止する。
	 */
	FMutex& operator=(const FMutex&) = delete;
	/**
	 * Mutexを取得する。別スレッドが保持中なら解放まで待つ。
	 */
	void Lock() noexcept;
	/**
	 * 待機せずMutex取得を試み、取得できた場合だけtrueを返す。
	 */
	bool TryLock() noexcept;
	/**
	 * 呼び出しスレッドが保持しているMutexを解放する。
	 */
	void Unlock() noexcept;

private:
	friend class FConditionVariable;
	/**
	 * OS固有Mutexを隠す実装型。
	 */
	struct FImpl;
	/**
	 * OS固有Mutexの所有領域。
	 */
	FImpl* m_pImpl = nullptr;
};
/**
 * スコープ終了までMutexを保持する。
 */
class FScopedLock
{
public:
	/**
	 * Mutexを取得する。
	 * @param Mutex 保持する排他制御。
	 */
	explicit FScopedLock(FMutex& Mutex) noexcept : m_pMutex(&Mutex)
	{
		m_pMutex->Lock();
	}
	/**
	 * 保持しているMutexを解放する。
	 */
	~FScopedLock()
	{
		m_pMutex->Unlock();
	}
	/**
	 * 二重解放を避けるためコピーを禁止する。
	 */
	FScopedLock(const FScopedLock&) = delete;
	/**
	 * 二重解放を避けるためコピー代入を禁止する。
	 */
	FScopedLock& operator=(const FScopedLock&) = delete;

private:
	/**
	 * スコープ終了時に解放する非所有Mutex。
	 */
	FMutex* m_pMutex;
};
} // namespace Toolbox
#endif
