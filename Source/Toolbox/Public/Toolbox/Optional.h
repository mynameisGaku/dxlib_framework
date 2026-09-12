// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_OPTIONAL_H
#define TOOLBOX_OPTIONAL_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 値がない状態を持つ所有領域。空の値の取得はFBadAccessを送出する。
 */
template <typename T> class TOptional
{
public:
	/**
	 * 値を持たない空の状態を構築する。
	 */
	TOptional() noexcept = default;
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Value 処理または保持する値。
	 */
	TOptional(const T& Value)
	    requires(__is_constructible(T, const T&))
	{
		Emplace(Value);
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Value 処理または保持する値。
	 */
	TOptional(T&& Value)
	    requires(__is_constructible(T, T &&))
	{
		Emplace(Move(Value));
	}
	/**
	 * 保持する値をコピーする。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TOptional(const TOptional& Other)
	    requires __is_constructible
	(T, const T&)
	{
		if (Other)
		{
			Emplace(*Other);
		}
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	TOptional(TOptional&& Other) noexcept(__is_nothrow_constructible(T, T&&))
	    requires(__is_constructible(T, T &&))
	{
		if (Other)
		{
			Emplace(Move(*Other));
		}
	}
	/**
	 * 保持している値と所有領域を破棄する。
	 */
	~TOptional()
	{
		Reset();
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TOptional& operator=(const TOptional& Other)
	    requires __is_constructible
	(T, const T&)
	{
		if (this != &Other)
		{
			if (Other)
			{
				Assign_Internal(*Other);
			}
			else
			{
				Reset();
			}
		}
		return *this;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TOptional& operator=(TOptional&& Other) noexcept(__is_nothrow_constructible(T, T&&) &&
	                                                 (!__is_assignable(T&, T&&) || __is_nothrow_assignable(T&, T&&)))
	    requires(__is_constructible(T, T &&))
	{
		if (this != &Other)
		{
			if (Other)
			{
				Assign_Internal(Move(*Other));
			}
			else
			{
				Reset();
			}
		}
		return *this;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TOptional& operator=(const T& Value)
	    requires(__is_constructible(T, const T&))
	{
		Assign_Internal(Value);
		return *this;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TOptional& operator=(T&& Value)
	    requires(__is_constructible(T, T &&))
	{
		Assign_Internal(Move(Value));
		return *this;
	}
	/**
	 * 以前の値を破棄して新たに構築する。構築失敗時は空になる。
	 * @param Values 構築引数。破棄される現在の保持値への参照を渡さない。
	 */
	template <typename... Args> T& Emplace(Args&&... Values)
	{
		Reset();
		// 構築または転送する対象の値。
		T* Value = new (m_Storage) T(Forward<Args>(Values)...);
		m_bPresent = true;
		return *Value;
	}
	/**
	 * 保持中の値や所有権を解放し、空の状態へ戻す。
	 */
	void Reset() noexcept
	{
		if (m_bPresent)
		{
			Pointer_Internal()->~T();
			m_bPresent = false;
		}
	}
	/**
	 * 構築済みの値が存在するか調べる。
	 */
	FORCEINLINE bool HasValue() const noexcept
	{
		return m_bPresent;
	}
	/**
	 * 有効な値または対象があるか調べる。
	 */
	FORCEINLINE explicit operator bool() const noexcept
	{
		return m_bPresent;
	}
	/**
	 * 構築済みの値を返す。空ならFBadAccessを送出する。
	 */
	T& Value()
	{
		if (!m_bPresent)
		{
			throw FBadAccess();
		}
		return *Pointer_Internal();
	}
	/**
	 * 構築済みの値を返す。空ならFBadAccessを送出する。
	 */
	const T& Value() const
	{
		if (!m_bPresent)
		{
			throw FBadAccess();
		}
		return *Pointer_Internal();
	}
	/**
	 * 保持している対象へアクセスする。呼び出し前に有効性を確認する。
	 */
	T& operator*()
	{
		return Value();
	}
	/**
	 * 保持している対象へアクセスする。呼び出し前に有効性を確認する。
	 */
	const T& operator*() const
	{
		return Value();
	}
	/**
	 * 保持している対象へアクセスする。呼び出し前に有効性を確認する。
	 */
	T* operator->()
	{
		return &Value();
	}
	/**
	 * 保持している対象へアクセスする。呼び出し前に有効性を確認する。
	 */
	const T* operator->() const
	{
		return &Value();
	}

private:
	/**
	 * 代入可能なら値を更新し、構築だけが可能な型は保持領域を作り直す。
	 *
	 * 再構築が失敗した場合は空になる。自分の保持値を指定した場合は変更しない。
	 * @param Value 新たに保持する値。

	 */
	template <typename U> void Assign_Internal(U&& Value)
	{
		if (m_bPresent && Pointer_Internal() == &Value)
		{
			return;
		}
		if constexpr (__is_assignable(T&, U&&))
		{
			if (m_bPresent)
			{
				**this = Forward<U>(Value);
				return;
			}
		}
		Emplace(Forward<U>(Value));
	}
	/**
	 * 構築済みオブジェクトが置かれる内部領域を指す。
	 */
	FORCEINLINE T* Pointer_Internal() noexcept
	{
		return reinterpret_cast<T*>(m_Storage);
	}
	/**
	 * 構築済みオブジェクトが置かれる内部領域を指す。
	 */
	FORCEINLINE const T* Pointer_Internal() const noexcept
	{
		return reinterpret_cast<const T*>(m_Storage);
	}
	/**
	 * 対象型のアラインメントを満たす未初期化の所有領域。
	 */
	alignas(T) unsigned char m_Storage[sizeof(T)];
	/**
	 * 内部領域に構築済みの値があるか。
	 */
	bool m_bPresent = false;
};
} // namespace Toolbox
#endif
