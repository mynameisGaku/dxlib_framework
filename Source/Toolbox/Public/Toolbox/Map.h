// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_MAP_H
#define TOOLBOX_MAP_H
#include "Toolbox/Algorithm.h"
namespace Toolbox
{
/**
 * 小規模なキー集合用の連想配列。検索は線形、追加時は参照を無効化し得る。
 */
template <typename K, typename V> class TMap
{
public:
	/**
	 * キーと対応する値を保持する一項目。
	 */
	using FEntry = TPair<K, V>;
	/**
	 * キーが一致する項目を返す。見つからなければEndを返す。
	 * @param Key 値を識別するキー。
	 */
	FEntry* Find(const K& Key)
	{
		/**
		 * 登録されたキーと値の項目を順に調べる。
		 */
		for (auto& Item : m_Entries)
		{
			if (Item.First == Key)
			{
				return &Item;
			}
		}
		return End();
	}
	/**
	 * キーが一致する項目を返す。見つからなければEndを返す。
	 * @param Key 値を識別するキー。
	 */
	const FEntry* Find(const K& Key) const
	{
		/**
		 * 登録されたキーと値の項目を順に調べる。
		 */
		for (const auto& Item : m_Entries)
		{
			if (Item.First == Key)
			{
				return &Item;
			}
		}
		return End();
	}
	/**
	 * キーに対応する値を返す。未登録なら値を既定構築して追加する。
	 * @param Key 取得または新規登録するキー。
	 */
	V& operator[](const K& Key)
	{
		/**
		 * 一致した要素または走査中の位置。
		 */
		auto It = Find(Key);
		if (It == End())
		{
			return m_Entries.EmplaceBack(FEntry{Key, V{}}).Second;
		}
		return It->Second;
	}
	/**
	 * キーに対応する値を返す。見つからなければ例外で通知する。
	 * @param Key 値を識別するキー。
	 */
	V& At(const K& Key)
	{
		/**
		 * 一致した要素または走査中の位置。
		 */
		auto It = Find(Key);
		if (It == End())
		{
			throw FBadAccess();
		}
		return It->Second;
	}
	/**
	 * 指定した要素を取り除き、後続の要素を詰める。
	 * @param Key 値を識別するキー。
	 */
	size_t Erase(const K& Key)
	{
		/**
		 * 一致した要素または走査中の位置。
		 */
		auto It = Find(Key);
		if (It == End())
		{
			return 0;
		}
		m_Entries.Erase(It);
		return 1;
	}
	/**
	 * 指定項目を削除し、詰めた後の次の項目を返す。
	 * @param It この連想配列の削除対象を指す反復子。
	 */
	FEntry* Erase(FEntry* It)
	{
		return m_Entries.Erase(It);
	}
	/**
	 * 保持している要素をすべて破棄して空にする。
	 */
	void Clear() noexcept
	{
		m_Entries.Clear();
	}
	/**
	 * 現在保持している要素数を返す。
	 */
	size_t Size() const noexcept
	{
		return m_Entries.Size();
	}
	/**
	 * 要素が一つもないか調べる。
	 */
	bool IsEmpty() const noexcept
	{
		return m_Entries.IsEmpty();
	}
	/**
	 * 先頭を指す反復子を返す。空の場合はEndと一致する。
	 */
	FEntry* Begin() noexcept
	{
		return m_Entries.Begin();
	}
	/**
	 * 先頭を指す反復子を返す。空の場合はEndと一致する。
	 */
	const FEntry* Begin() const noexcept
	{
		return m_Entries.Begin();
	}
	/**
	 * 最終要素の次を指す反復子を返す。この位置は参照しない。
	 */
	FEntry* End() noexcept
	{
		return m_Entries.End();
	}
	/**
	 * 最終要素の次を指す反復子を返す。この位置は参照しない。
	 */
	const FEntry* End() const noexcept
	{
		return m_Entries.End();
	}
	/**
	 * range-forが必要とする先頭反復子を返す。
	 */
	FEntry* begin() noexcept
	{
		return Begin();
	}
	/**
	 * range-forが必要とする先頭反復子を返す。
	 */
	const FEntry* begin() const noexcept
	{
		return Begin();
	}
	/**
	 * range-forが必要とする終端反復子を返す。
	 */
	FEntry* end() noexcept
	{
		return End();
	}
	/**
	 * range-forが必要とする終端反復子を返す。
	 */
	const FEntry* end() const noexcept
	{
		return End();
	}

private:
	/**
	 * キーと値を挿入順に保持する領域。
	 */
	TVector<FEntry> m_Entries;
};
/**
 * 重複を許さない小規模な値集合。順序は挿入順で、検索は線形。
 */
template <typename T> class TSet
{
public:
	/**
	 * 同じ値がまだない場合だけ集合へ追加する。
	 * @param Value 処理または保持する値。
	 */
	void Insert(const T& Value)
	{
		if (Toolbox::Find(m_Values.Begin(), m_Values.End(), Value) == m_Values.End())
		{
			m_Values.PushBack(Value);
		}
	}
	/**
	 * 指定した要素を取り除き、後続の要素を詰める。
	 * @param Value 処理または保持する値。
	 */
	size_t Erase(const T& Value)
	{
		/**
		 * 一致した要素または走査中の位置。
		 */
		auto It = Toolbox::Find(m_Values.Begin(), m_Values.End(), Value);
		if (It == m_Values.End())
		{
			return 0;
		}
		m_Values.Erase(It);
		return 1;
	}
	/**
	 * 現在保持している要素数を返す。
	 */
	size_t Size() const noexcept
	{
		return m_Values.Size();
	}
	/**
	 * 要素が一つもないか調べる。
	 */
	bool IsEmpty() const noexcept
	{
		return m_Values.IsEmpty();
	}

private:
	/**
	 * 重複しない値を保持する領域。
	 */
	TVector<T> m_Values;
};
} // namespace Toolbox
#endif
