// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_VECTOR_H
#define TOOLBOX_VECTOR_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 連続した所有領域。再確保は参照を無効化し、確保失敗は例外で通知する。
 */
template <typename T> class TVector
{
public:
	/**
	 * 指定した値を使って初期状態を構築する。
	 */
	TVector() = default;
	/**
	 * 指定した値を使って初期状態を構築する。
	 */
	explicit TVector(size_t Count)
	{
		try
		{
			Resize(Count);
		}
		catch (...)
		{
			Clear();
			Free_Internal(m_pData);
			throw;
		}
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 */
	TVector(TInitializerList<T> Values)
	{
		try
		{
			Reserve(Values.size());
			for (const auto& Value : Values)
			{
				EmplaceBack(Value);
			}
		}
		catch (...)
		{
			Clear();
			Free_Internal(m_pData);
			throw;
		}
	}
	/**
	 * 保持する値をコピーする。
	 */
	TVector(const TVector& Other)
	{
		try
		{
			Reserve(Other.Size());
			for (const T& Value : Other)
			{
				EmplaceBack(Value);
			}
		}
		catch (...)
		{
			Clear();
			Free_Internal(m_pData);
			throw;
		}
	}
	/**
	 * 元の値から所有領域または状態を移す。
	 */
	TVector(TVector&& Other) noexcept
	{
		Swap(Other);
	}
	/**
	 * 保持している値と所有領域を破棄する。
	 */
	~TVector()
	{
		Clear();
		Free_Internal(m_pData);
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TVector& operator=(const TVector& Other)
	{
		if (this != &Other)
		{
			/**
			 * 元の値を保ったまま変更を準備する複製。
			 */
			TVector Copy(Other);
			Swap(Copy);
		}
		return *this;
	}
	/**
	 * 以前の値を後始末し、新しい値または所有権を受け取る。
	 */
	TVector& operator=(TVector&& Other) noexcept
	{
		if (this != &Other)
		{
			/**
			 * 交換または代入が終わるまで元の値を保持する一時領域。
			 */
			TVector Temporary(Move(Other));
			Swap(Temporary);
		}
		return *this;
	}
	/**
	 * 二つの値または所有領域を交換する。
	 * @param Other コピー・移動・比較の相手となる値。
	 */
	void Swap(TVector& Other) noexcept
	{
		Toolbox::Swap(m_pData, Other.m_pData);
		Toolbox::Swap(m_Size, Other.m_Size);
		Toolbox::Swap(m_Capacity, Other.m_Capacity);
	}
	/**
	 * 指定数を保持できる領域を確保する。再確保時は要素への参照が失効する。
	 * @param Capacity 確保する要素数の下限。
	 */
	void Reserve(size_t Capacity)
	{
		if (Capacity <= m_Capacity)
		{
			return;
		}
		if (Capacity > TNumericLimits<size_t>::Max() / sizeof(T))
		{
			throw FException("Vector capacity overflow");
		}
		/**
		 * 再確保先の領域。移動完了までは元の領域と分けて管理する。
		 */
		T* NewData = static_cast<T*>(::operator new(Capacity * sizeof(T), std::align_val_t(alignof(T))));
		/**
		 * 新しい領域で構築が完了した要素数。失敗時の後始末に使う。
		 */
		size_t Constructed = 0;
		try
		{
			for (; Constructed < m_Size; ++Constructed)
			{
				if constexpr (__is_nothrow_constructible(T, T&&) || !__is_constructible(T, const T&))
				{
					new (NewData + Constructed) T(Move(m_pData[Constructed]));
				}
				else
				{
					new (NewData + Constructed) T(m_pData[Constructed]);
				}
			}
		}
		catch (...)
		{
			while (Constructed)
			{
				NewData[--Constructed].~T();
			}
			Free_Internal(NewData);
			throw;
		}
		/**
		 * 再確保前に存在していた要素数。
		 */
		const size_t PreviousSize = m_Size;
		Clear();
		Free_Internal(m_pData);
		m_pData = NewData;
		m_Size = PreviousSize;
		m_Capacity = Capacity;
	}
	/**
	 * 末尾の値を構築する。再確保前に引数を退避し、コピー専用型にも対応する。
	 * @param Values 追加する値のコンストラクターへ転送する引数。
	 */
	template <typename... Args> T& EmplaceBack(Args&&... Values)
	{
		if (m_Size == m_Capacity)
		{
			// 引数が現在の要素を参照していても、再確保の前に値を保持する。
			/**
			 * 構築または転送する対象の値。
			 */
			T Value(Forward<Args>(Values)...);
			/**
			 * 計算のオーバーフローを起こさない上限。
			 */
			const size_t Maximum = TNumericLimits<size_t>::Max() / sizeof(T);
			if (m_Size == Maximum)
			{
				throw FException("Vector capacity overflow");
			}
			Reserve(m_Capacity > Maximum / 2 ? Maximum : (m_Capacity ? m_Capacity * 2 : 4));
			if constexpr (__is_nothrow_constructible(T, T&&) || !__is_constructible(T, const T&))
			{
				new (m_pData + m_Size) T(Move(Value));
			}
			else
			{
				new (m_pData + m_Size) T(Value);
			}
		}
		else
		{
			new (m_pData + m_Size) T(Forward<Args>(Values)...);
		}
		return m_pData[m_Size++];
	}
	/**
	 * 末尾へ値を追加する。再確保時は既存要素への参照が失効する。
	 * @param Value 処理または保持する値。
	 */
	void PushBack(const T& Value)
	{
		EmplaceBack(Value);
	}
	/**
	 * 末尾へ値を追加する。再確保時は既存要素への参照が失効する。
	 * @param Value 処理または保持する値。
	 */
	void PushBack(T&& Value)
	{
		EmplaceBack(Move(Value));
	}
	/**
	 * 末尾の要素を一つ破棄する。空なら何もしない。
	 */
	void PopBack() noexcept
	{
		if (m_Size)
		{
			m_pData[--m_Size].~T();
		}
	}
	/**
	 * 保持している要素をすべて破棄して空にする。
	 */
	void Clear() noexcept
	{
		while (m_Size)
		{
			PopBack();
		}
	}
	/**
	 * 要素数を変更し、増えた領域をゼロまたは既定値で初期化する。
	 * @param Count 処理する要素数。
	 */
	void Resize(size_t Count)
	{
		if (Count < m_Size)
		{
			while (m_Size > Count)
			{
				PopBack();
			}
			return;
		}
		Reserve(Count);
		/**
		 * 変更前の値または要素数。
		 */
		const size_t Previous = m_Size;
		try
		{
			while (m_Size < Count)
			{
				new (m_pData + m_Size) T();
				++m_Size;
			}
		}
		catch (...)
		{
			while (m_Size > Previous)
			{
				PopBack();
			}
			throw;
		}
	}
	/**
	 * 指定した要素を取り除き、後続の要素を詰める。
	 * @param Position 削除する要素を指す反復子。
	 */
	T* Erase(T* Position)
	{
		/**
		 * 現在処理している要素の位置。
		 */
		const size_t Index = static_cast<size_t>(Position - m_pData);
		for (size_t I = Index; I + 1 < m_Size; ++I)
		{
			m_pData[I] = Move(m_pData[I + 1]);
		}
		PopBack();
		return m_pData + Index;
	}
	/**
	 * 現在保持している要素数を返す。
	 */
	size_t Size() const noexcept
	{
		return m_Size;
	}
	/**
	 * 要素が一つもないか調べる。
	 */
	bool IsEmpty() const noexcept
	{
		return m_Size == 0;
	}
	/**
	 * 連続した要素領域へのポインターを返す。再確保後は使わない。
	 */
	T* Data() noexcept
	{
		return m_pData;
	}
	/**
	 * 連続した要素領域へのポインターを返す。再確保後は使わない。
	 */
	const T* Data() const noexcept
	{
		return m_pData;
	}
	/**
	 * 先頭を指す反復子を返す。空の場合はEndと一致する。
	 */
	T* Begin() noexcept
	{
		return m_pData;
	}
	/**
	 * 先頭を指す反復子を返す。空の場合はEndと一致する。
	 */
	const T* Begin() const noexcept
	{
		return m_pData;
	}
	/**
	 * 最終要素の次を指す反復子を返す。この位置は参照しない。
	 */
	T* End() noexcept
	{
		return m_Size ? m_pData + m_Size : m_pData;
	}
	/**
	 * 最終要素の次を指す反復子を返す。この位置は参照しない。
	 */
	const T* End() const noexcept
	{
		return m_Size ? m_pData + m_Size : m_pData;
	}
	// range-forの言語プロトコル。
	/**
	 * range-forが必要とする先頭反復子を返す。
	 */
	T* begin() noexcept
	{
		return Begin();
	}
	/**
	 * range-forが必要とする先頭反復子を返す。
	 */
	const T* begin() const noexcept
	{
		return Begin();
	}
	/**
	 * range-forが必要とする終端反復子を返す。
	 */
	T* end() noexcept
	{
		return End();
	}
	/**
	 * range-forが必要とする終端反復子を返す。
	 */
	const T* end() const noexcept
	{
		return End();
	}
	/**
	 * 指定した位置またはキーの要素へアクセスする。
	 */
	T& operator[](size_t Index) noexcept
	{
		return m_pData[Index];
	}
	/**
	 * 指定した位置またはキーの要素へアクセスする。
	 */
	const T& operator[](size_t Index) const noexcept
	{
		return m_pData[Index];
	}
	/**
	 * 末尾の要素を返す。呼び出し前に空でないことを確認する。
	 */
	T& Back() noexcept
	{
		return m_pData[m_Size - 1];
	}
	/**
	 * 末尾の要素を返す。呼び出し前に空でないことを確認する。
	 */
	const T& Back() const noexcept
	{
		return m_pData[m_Size - 1];
	}
	/**
	 * 保持する値または対象が一致するか比較する。
	 */
	bool operator==(const TVector& Other) const
	{
		if (m_Size != Other.m_Size)
		{
			return false;
		}
		for (size_t I = 0; I < m_Size; ++I)
		{
			if (!(m_pData[I] == Other.m_pData[I]))
			{
				return false;
			}
		}
		return true;
	}

private:
	/**
	 * 確保時と同じアラインメント指定で領域を解放する。
	 */
	static void Free_Internal(T* Pointer) noexcept
	{
		::operator delete(Pointer, std::align_val_t(alignof(T)));
	}
	/**
	 * 連続領域の先頭。再確保時に置き換わる。
	 */
	T* m_pData = nullptr;
	/**
	 * 現在の有効な要素数。
	 */
	size_t m_Size = 0;
	/**
	 * 再確保せずに保持できる要素数。
	 */
	size_t m_Capacity = 0;
};
} // namespace Toolbox
#endif
