// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_POST_H
#define DXF_UI_POST_H
#include "Toolbox/Function.h"
#include "Toolbox/Mutex.h"
#include "Toolbox/SharedPtr.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
namespace Detail
{
/**
 * 所有スレッドへ処理を届ける列。任意のスレッドから積み、所有スレッドで取り出して実行する。
 * 処理（と捕捉したデータ）の実行・破棄はロックの外で行う。
 */
class FUiPostQueue
{
public:
	/**
	 * 処理を積む。閉じた列には積まない。
	 * @param Callback 処理。
	 */
	bool Push(Toolbox::TFunction<void()> Callback)
	{
		if (!Callback)
		{
			return false;
		}
		Toolbox::FScopedLock Lock(m_Mutex);
		if (m_bClosed)
		{
			// 捕捉したデータの破棄はロックの外で行う（呼出し側の引数として関数を抜けるときに破棄される）。
			return false;
		}
		m_Items.PushBack(Toolbox::Move(Callback));
		return true;
	}
	/**
	 * 積まれた処理をすべて取り出す（ロックの中では移すだけ）。
	 * @param Out 取り出し先（追記）。
	 */
	void Drain(Toolbox::TVector<Toolbox::TFunction<void()>>& Out)
	{
		Toolbox::TVector<Toolbox::TFunction<void()>> Items;
		{
			Toolbox::FScopedLock Lock(m_Mutex);
			Items.Swap(m_Items);
		}
		for (auto& Item : Items)
		{
			Out.PushBack(Toolbox::Move(Item));
		}
	}
	/**
	 * 列を閉じ、積まれた処理を実行せずに捨てる（破棄はロックの外）。
	 */
	void Close() noexcept
	{
		Toolbox::TVector<Toolbox::TFunction<void()>> Items;
		{
			Toolbox::FScopedLock Lock(m_Mutex);
			m_bClosed = true;
			Items.Swap(m_Items);
		}
	}
	/**
	 * 積まれている数。
	 */
	Toolbox::size_t GetPendingCount()
	{
		Toolbox::FScopedLock Lock(m_Mutex);
		return m_Items.Size();
	}

private:
	/**
	 * 列の排他。
	 */
	Toolbox::FMutex m_Mutex;
	/**
	 * 積まれた処理。
	 */
	Toolbox::TVector<Toolbox::TFunction<void()>> m_Items;
	/**
	 * 閉じたか。
	 */
	bool m_bClosed = false;
};
} // namespace Detail

/**
 * ルートへ処理を届ける窓口の写し。非同期処理が持ち、完了時に所有スレッドのUIを変更するために使う。
 * ルートが破棄された後は何もしない（Postがfalse）。
 */
class FUiPostHandle
{
public:
	FUiPostHandle() = default;
	/**
	 * 列から作る（ルートの内部だけが使う）。
	 * @param Queue 列。
	 */
	explicit FUiPostHandle(Toolbox::TWeakPtr<Detail::FUiPostQueue> Queue) noexcept : m_pQueue(Toolbox::Move(Queue))
	{
	}
	/**
	 * 処理を積む。ルートの次の更新の境界に所有スレッドで実行する。
	 * @param Callback 処理。
	 */
	bool Post(Toolbox::TFunction<void()> Callback) const
	{
		const auto Queue = m_pQueue.Lock();
		return Queue && Queue->Push(Toolbox::Move(Callback));
	}

private:
	/**
	 * 列。
	 */
	Toolbox::TWeakPtr<Detail::FUiPostQueue> m_pQueue;
};
} // namespace Dxf
#endif
