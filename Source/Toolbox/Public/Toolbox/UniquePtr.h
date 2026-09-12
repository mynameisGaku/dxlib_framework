// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_UNIQUE_PTR_H
#define TOOLBOX_UNIQUE_PTR_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 単一オブジェクトを排他的に所有する。コピー禁止、配列には使用しない。
 */
template <typename T> class TUniquePtr
{
public:
	/**
	 * 所有対象のない空のポインターを作る。
	 */
	TUniquePtr() = default;
	/**
	 * 所有対象のない空の排他ポインターを作る。
	 */
	TUniquePtr(decltype(nullptr)) noexcept
	{
	}
	/**
	 * 渡された単一オブジェクトの所有権を引き受ける。
	 * @param Pointer deleteで破棄する対象のポインター。
	 */
	explicit TUniquePtr(T* Pointer) noexcept : m_pValue(Pointer)
	{
	}
	/**
	 * 意図しない所有権や状態の複製を禁止する。
	 */
	TUniquePtr(const TUniquePtr&) = delete;
	/**
	 * 意図しない所有権や状態の複製を禁止する。
	 */
	TUniquePtr& operator=(const TUniquePtr&) = delete;
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TUniquePtr(TUniquePtr&& Other) noexcept : m_pValue(Other.Release())
	{
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	template <typename U>
	    requires requires(U* Pointer) { static_cast<T*>(Pointer); }
	TUniquePtr(TUniquePtr<U>&& Other) noexcept : m_pValue(Other.Release())
	{
	}
	/**
	 * 保持している値と所有領域を破棄する。
	 */
	~TUniquePtr()
	{
		delete m_pValue;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TUniquePtr& operator=(TUniquePtr&& Other) noexcept
	{
		if (this != &Other)
		{
			Reset(Other.Release());
		}
		return *this;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	template <typename U> TUniquePtr& operator=(TUniquePtr<U>&& Other) noexcept
	{
		Reset(Other.Release());
		return *this;
	}
	/**
	 * 所有している対象を破棄し、空のポインターへ戻す。
	 */
	TUniquePtr& operator=(decltype(nullptr)) noexcept
	{
		Reset();
		return *this;
	}
	/**
	 * 所有している対象のポインターを返す。空ならnullptrを返す。
	 */
	FORCEINLINE T* Get() const noexcept
	{
		return m_pValue;
	}
	/**
	 * 破棄せずに所有権を手放す。返したポインターの管理は呼び出し側へ移る。
	 */
	FORCEINLINE T* Release() noexcept
	{
		return Exchange(m_pValue, nullptr);
	}
	/**
	 * 以前の対象を破棄し、新しいポインターを所有する。同じポインターなら何もしない。
	 * @param Pointer 新しい所有対象。nullptrなら所有権を空にする。
	 */
	void Reset(T* Pointer = nullptr) noexcept
	{
		if (Pointer != m_pValue)
		{
			// 置き換え前の値。公開状態を更新してから後始末する。
			T* Old = Exchange(m_pValue, Pointer);
			delete Old;
		}
	}
	/**
	 * 有効な値または対象があるか調べる。
	 */
	FORCEINLINE explicit operator bool() const noexcept
	{
		return m_pValue != nullptr;
	}
	/**
	 * 保持している対象へアクセスする。呼び出し前に有効性を確認する。
	 */
	FORCEINLINE T& operator*() const noexcept
	{
		return *m_pValue;
	}
	/**
	 * 保持している対象へアクセスする。呼び出し前に有効性を確認する。
	 */
	FORCEINLINE T* operator->() const noexcept
	{
		return m_pValue;
	}
	/**
	 * 保持する値または対象が一致するか比較する。
	 */
	FORCEINLINE bool operator==(decltype(nullptr)) const noexcept
	{
		return !m_pValue;
	}

private:
	/**
	 * 所有または監視している対象へのポインター。
	 */
	T* m_pValue = nullptr;
};
/**
 * 引数を転送して単一オブジェクトを生成し、排他的な所有権を返す。
 * @param Values オブジェクトのコンストラクターへ転送する引数。
 */
template <typename T, typename... Args> TUniquePtr<T> MakeUnique(Args&&... Values)
{
	return TUniquePtr<T>(new T(Forward<Args>(Values)...));
}
} // namespace Toolbox
#endif
