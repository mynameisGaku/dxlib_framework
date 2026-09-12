// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_SHARED_PTR_H
#define TOOLBOX_SHARED_PTR_H
#include "Toolbox/Atomic.h"
namespace Toolbox
{
namespace Detail
{
/**
 * オブジェクトの寿命と弱参照の寿命を別々に数える管理領域。
 */
struct FSharedControl
{
	/**
	 * 対象を所有する参照の数。
	 */
	FAtomicCounter Strong{1};
	/**
	 * 管理領域を保持する弱参照数。強参照が残る間の保持分を1つ含む。
	 */
	FAtomicCounter Weak{1};
	/**
	 * 型を消去して保持する破棄対象。
	 */
	const void* Object = nullptr;
	/**
	 * 構築時の型で対象を破棄する関数。const所有にも対応する。
	 */
	void (*Destroy)(const void*) = nullptr;
	/**
	 * 弱参照を減らし、最後なら管理領域を破棄する。
	 */
	void ReleaseWeak() noexcept
	{
		if (Weak.FetchAdd(uint64(-1)) == 1)
		{
			delete this;
		}
	}
	/**
	 * 強参照を減らし、最後の所有者なら対象を一度だけ破棄する。
	 */
	void ReleaseStrong() noexcept
	{
		if (Strong.FetchAdd(uint64(-1)) == 1)
		{
			Destroy(Object);
			ReleaseWeak();
		}
	}
};
} // namespace Detail
template <typename T> class TWeakPtr;
/**
 * 参照カウントで所有権を共有する。循環参照にはTWeakPtrを使用する。
 */
template <typename T> class TSharedPtr
{
	template <typename U> friend class TSharedPtr;
	template <typename U> friend class TWeakPtr;

public:
	/**
	 * 所有対象のない空のポインターを作る。
	 */
	TSharedPtr() = default;
	/**
	 * 所有対象のない空の共有ポインターを作る。
	 */
	TSharedPtr(decltype(nullptr)) noexcept
	{
	}
	/**
	 * ポインターの所有権を引き受け、参照カウントを作成する。
	 * @param Pointer 引き受ける単一オブジェクト。確保失敗時もこの処理が破棄する。
	 */
	template <typename U>
	    requires requires(U* Pointer) { static_cast<T*>(Pointer); }
	explicit TSharedPtr(U* Pointer) : m_pValue(Pointer)
	{
		if (!Pointer)
		{
			return;
		}
		try
		{
			m_pControl = new Detail::FSharedControl;
		}
		catch (...)
		{
			delete Pointer;
			throw;
		}
		m_pControl->Object = Pointer;
		m_pControl->Destroy = [](const void* Object)
		{
			delete static_cast<const U*>(Object);
		};
	}
	/**
	 * 同じ対象を参照し、対応する参照カウントを増やす。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TSharedPtr(const TSharedPtr& Other) noexcept : m_pValue(Other.m_pValue), m_pControl(Other.m_pControl)
	{
		AddRef_Internal();
	}
	/**
	 * 変換可能な型の共有ポインターから同じ対象の所有権を共有する。
	 * @param Other 所有権を共有する元のポインター。
	 */
	template <typename U>
	    requires requires(U* Pointer) { static_cast<T*>(Pointer); }
	TSharedPtr(const TSharedPtr<U>& Other) noexcept : m_pValue(Other.m_pValue), m_pControl(Other.m_pControl)
	{
		AddRef_Internal();
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TSharedPtr(TSharedPtr&& Other) noexcept
	    : m_pValue(Exchange(Other.m_pValue, nullptr)), m_pControl(Exchange(Other.m_pControl, nullptr))
	{
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	template <typename U>
	    requires requires(U* Pointer) { static_cast<T*>(Pointer); }
	TSharedPtr(TSharedPtr<U>&& Other) noexcept
	    : m_pValue(Exchange(Other.m_pValue, nullptr)), m_pControl(Exchange(Other.m_pControl, nullptr))
	{
	}
	/**
	 * 強参照を手放す。最後の所有者の場合だけ対象を破棄する。
	 */
	~TSharedPtr()
	{
		if (m_pControl)
		{
			m_pControl->ReleaseStrong();
		}
	}
	/**
	 * 以前の強参照を手放し、受け取った共有所有権へ置き換える。
	 * @param Other 新たに保持する共有ポインター。
	 */
	TSharedPtr& operator=(TSharedPtr Other) noexcept
	{
		Swap(Other);
		return *this;
	}
	/**
	 * 二つの値または所有領域を交換する。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	void Swap(TSharedPtr& Other) noexcept
	{
		Toolbox::Swap(m_pValue, Other.m_pValue);
		Toolbox::Swap(m_pControl, Other.m_pControl);
	}
	/**
	 * 保持中の値や所有権を解放し、空の状態へ戻す。
	 */
	void Reset() noexcept
	{
		/**
		 * 所有権を手放すための空の値。
		 */
		TSharedPtr Empty;
		Swap(Empty);
	}
	/**
	 * 所有している対象のポインターを返す。空ならnullptrを返す。
	 */
	T* Get() const noexcept
	{
		return m_pValue;
	}
	/**
	 * 保持している対象へアクセスする。呼び出し前に有効性を確認する。
	 */
	T* operator->() const noexcept
	{
		return m_pValue;
	}
	/**
	 * 保持している対象へアクセスする。呼び出し前に有効性を確認する。
	 */
	T& operator*() const noexcept
	{
		return *m_pValue;
	}
	/**
	 * 有効な値または対象があるか調べる。
	 */
	explicit operator bool() const noexcept
	{
		return m_pValue != nullptr;
	}
	/**
	 * 保持する値または対象が一致するか比較する。
	 */
	bool operator==(decltype(nullptr)) const noexcept
	{
		return !m_pValue;
	}
	/**
	 * 保持する値または対象が一致するか比較する。
	 */
	template <typename U> bool operator==(const TSharedPtr<U>& Other) const noexcept
	{
		return m_pValue == Other.Get();
	}
	/**
	 * 現在の強参照数を返す。別スレッドの操作で直後に変化し得る。
	 */
	uint64 UseCount() const noexcept
	{
		return m_pControl ? m_pControl->Strong.Load() : 0;
	}

private:
	/**
	 * 管理領域がある場合だけ参照カウントを増やす。
	 */
	void AddRef_Internal() noexcept
	{
		if (m_pControl)
		{
			m_pControl->Strong.FetchAdd(1);
		}
	}
	/**
	 * 所有または監視している対象へのポインター。
	 */
	T* m_pValue = nullptr;
	/**
	 * 対象とは別の寿命を持つ参照カウント管理領域。
	 */
	Detail::FSharedControl* m_pControl = nullptr;
};
/**
 * 所有権を持たず寿命を監視する。Lock成功時だけ対象へアクセスできる。
 */
template <typename T> class TWeakPtr
{
public:
	/**
	 * 監視対象のない空の弱ポインターを作る。
	 */
	TWeakPtr() = default;
	/**
	 * 対象を所有せず、共有ポインターの寿命を監視する。
	 * @param Other 監視を始める対象の共有ポインター。
	 */
	template <typename U>
	    requires requires(U* Pointer) { static_cast<T*>(Pointer); }
	TWeakPtr(const TSharedPtr<U>& Other) noexcept : m_pValue(Other.m_pValue), m_pControl(Other.m_pControl)
	{
		AddRef_Internal();
	}
	/**
	 * 同じ対象を参照し、対応する参照カウントを増やす。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TWeakPtr(const TWeakPtr& Other) noexcept : m_pValue(Other.m_pValue), m_pControl(Other.m_pControl)
	{
		AddRef_Internal();
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TWeakPtr(TWeakPtr&& Other) noexcept
	    : m_pValue(Exchange(Other.m_pValue, nullptr)), m_pControl(Exchange(Other.m_pControl, nullptr))
	{
	}
	/**
	 * 弱参照を手放す。最後の参照なら管理領域を破棄する。
	 */
	~TWeakPtr()
	{
		if (m_pControl)
		{
			m_pControl->ReleaseWeak();
		}
	}
	/**
	 * 以前の監視を解除し、受け取った弱参照へ置き換える。
	 * @param Other 新たに保持する弱ポインター。
	 */
	TWeakPtr& operator=(TWeakPtr Other) noexcept
	{
		Toolbox::Swap(m_pValue, Other.m_pValue);
		Toolbox::Swap(m_pControl, Other.m_pControl);
		return *this;
	}
	/**
	 * 対象の強参照がなくなったか調べる。アクセスにはLockを使う。
	 */
	bool IsExpired() const noexcept
	{
		return !m_pControl || m_pControl->Strong.Load() == 0;
	}
	/**
	 * 対象が生存している場合だけ強参照を取得する。失効済みなら空を返す。
	 */
	TSharedPtr<T> Lock() const noexcept
	{
		/**
		 * 処理結果を組み立てる一時領域。
		 */
		TSharedPtr<T> Result;
		if (!m_pControl)
		{
			return Result;
		}
		/**
		 * 比較交換で確保しようとしている現在の強参照数。
		 */
		uint64 Count = m_pControl->Strong.Load();
		while (Count != 0)
		{
			if (m_pControl->Strong.CompareExchange(Count, Count + 1))
			{
				Result.m_pValue = m_pValue;
				Result.m_pControl = m_pControl;
				break;
			}
		}
		return Result;
	}
	/**
	 * 保持中の値や所有権を解放し、空の状態へ戻す。
	 */
	void Reset() noexcept
	{
		*this = TWeakPtr();
	}

private:
	/**
	 * 管理領域がある場合だけ参照カウントを増やす。
	 */
	void AddRef_Internal() noexcept
	{
		if (m_pControl)
		{
			m_pControl->Weak.FetchAdd(1);
		}
	}
	/**
	 * 所有または監視している対象へのポインター。
	 */
	T* m_pValue = nullptr;
	/**
	 * 対象とは別の寿命を持つ参照カウント管理領域。
	 */
	Detail::FSharedControl* m_pControl = nullptr;
};
/**
 * 引数を転送してオブジェクトを生成し、共有所有権を返す。
 * @param Values オブジェクトのコンストラクターへ転送する引数。
 */
template <typename T, typename... Args> TSharedPtr<T> MakeShared(Args&&... Values)
{
	return TSharedPtr<T>(new T(Forward<Args>(Values)...));
}
} // namespace Toolbox
#endif
