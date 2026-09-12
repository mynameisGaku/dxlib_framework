// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_STRING_H
#define TOOLBOX_STRING_H
#include "Toolbox/Vector.h"
#include <stdio.h>
namespace Toolbox
{
/**
 * 所有しない文字列範囲。元の文字列より長く保持してはいけない。
 */
class FStringView
{
public:
	/**
	 * 検索に失敗したことを表す予約値。
	 */
	static constexpr size_t NotFound = TNumericLimits<size_t>::Max();
	/**
	 * 空の文字列を指す非所有ビューを作る。
	 */
	FStringView() = default;
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 */
	FStringView(const char* Text) : m_pData(Text)
	{
		if (Text)
		{
			while (Text[m_Size])
			{
				++m_Size;
			}
		}
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 * @param Length 終端を除いた文字数。
	 */
	constexpr FStringView(const char* Text, size_t Length) : m_pData(Text), m_Size(Length)
	{
	}
	/**
	 * 参照している文字列範囲の先頭を返す。所有権は移さない。
	 */
	const char* Data() const noexcept
	{
		return m_pData;
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
	 * 指定位置の文字要素を返す。位置が範囲内であることを呼び出し側で確認する。
	 * @param Index 文字要素の位置。
	 */
	char operator[](size_t Index) const noexcept
	{
		return m_pData[Index];
	}
	/**
	 * 一致するバイトの位置を返す。見つからなければNotFoundを返す。
	 * @param Character 追加または検索する一文字。
	 */
	size_t Find(char Character) const noexcept
	{
		/**
		 * 現在の要素位置を進めて範囲を走査する。
		 */
		for (size_t I = 0; I < m_Size; ++I)
		{
			if (m_pData[I] == Character)
			{
				return I;
			}
		}
		return NotFound;
	}
	/**
	 * 保持する値または対象が一致するか比較する。
	 */
	bool operator==(FStringView Other) const noexcept
	{
		if (m_Size != Other.m_Size)
		{
			return false;
		}
		/**
		 * 現在の要素位置を進めて範囲を走査する。
		 */
		for (size_t I = 0; I < m_Size; ++I)
		{
			if (m_pData[I] != Other.m_pData[I])
			{
				return false;
			}
		}
		return true;
	}

private:
	/**
	 * 外部の文字列を指す非所有ポインター。
	 */
	const char* m_pData = "";
	/**
	 * 現在の有効な要素数。
	 */
	size_t m_Size = 0;
};
/**
 * 終端文字を持つ所有文字列。長さ指定なら埋め込みNULも保持する。
 */
template <typename TChar> class TBasicString
{
public:
	/**
	 * 文字を含まない所有文字列を作る。
	 */
	TBasicString() = default;
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 */
	TBasicString(const TChar* Text)
	{
		if (Text)
		{
			while (*Text)
			{
				m_Characters.PushBack(*Text++);
			}
		}
		m_Characters.PushBack(TChar{});
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param Text 読み取る文字列。
	 * @param Length 終端を除いた文字数。
	 */
	TBasicString(const TChar* Text, size_t Length)
	{
		if (Length == TNumericLimits<size_t>::Max())
		{
			throw FException("String size overflow");
		}
		m_Characters.Reserve(Length + 1);
		/**
		 * 現在の要素位置を進めて範囲を走査する。
		 */
		for (size_t I = 0; I < Length; ++I)
		{
			m_Characters.PushBack(Text[I]);
		}
		m_Characters.PushBack(TChar{});
	}
	/**
	 * 指定した値を使って初期状態を構築する。
	 * @param First 入力範囲の先頭。
	 * @param Last 入力範囲の終端。
	 */
	template <typename TIterator> TBasicString(TIterator First, TIterator Last)
	{
		for (; First != Last; ++First)
		{
			m_Characters.PushBack(static_cast<TChar>(*First));
		}
		m_Characters.PushBack(TChar{});
	}
	/**
	 * 現在保持している要素数を返す。
	 */
	size_t Size() const noexcept
	{
		return m_Characters.IsEmpty() ? 0 : m_Characters.Size() - 1;
	}
	/**
	 * 要素が一つもないか調べる。
	 */
	bool IsEmpty() const noexcept
	{
		return Size() == 0;
	}
	/**
	 * 終端NUL付き文字列を返す。次の変更まで有効。
	 */
	const TChar* CStr() const noexcept
	{
		return m_Characters.IsEmpty() ? m_Empty : m_Characters.Data();
	}
	/**
	 * 連続した要素領域へのポインターを返す。再確保後は使わない。
	 */
	const TChar* Data() const noexcept
	{
		return CStr();
	}
	/**
	 * 連続した要素領域へのポインターを返す。再確保後は使わない。
	 */
	TChar* Data()
	{
		EnsureTerminator_Internal();
		return m_Characters.Data();
	}
	/**
	 * 先頭を指す反復子を返す。空の場合はEndと一致する。
	 */
	const TChar* Begin() const noexcept
	{
		return CStr();
	}
	/**
	 * 最終要素の次を指す反復子を返す。この位置は参照しない。
	 */
	const TChar* End() const noexcept
	{
		return CStr() + Size();
	}
	/**
	 * range-forが必要とする先頭反復子を返す。
	 */
	const TChar* begin() const noexcept
	{
		return Begin();
	}
	/**
	 * range-forが必要とする終端反復子を返す。
	 */
	const TChar* end() const noexcept
	{
		return End();
	}
	/**
	 * 指定位置の文字要素を返す。位置が範囲内であることを呼び出し側で確認する。
	 * @param Index 文字要素の位置。
	 */
	TChar& operator[](size_t Index)
	{
		return m_Characters[Index];
	}
	/**
	 * 指定位置の文字要素を返す。位置が範囲内であることを呼び出し側で確認する。
	 * @param Index 文字要素の位置。
	 */
	const TChar& operator[](size_t Index) const
	{
		return CStr()[Index];
	}
	/**
	 * 要素数を変更し、増えた領域をゼロまたは既定値で初期化する。
	 * @param Length 終端を除いた文字数。
	 */
	void Resize(size_t Length)
	{
		if (Length == TNumericLimits<size_t>::Max())
		{
			throw FException("String size overflow");
		}
		m_Characters.Resize(Length + 1);
		m_Characters[Length] = TChar{};
	}
	/**
	 * 保持している要素をすべて破棄して空にする。
	 */
	void Clear() noexcept
	{
		m_Characters.Clear();
	}
	/**
	 * 末尾へ値を追加する。再確保時は既存要素への参照が失効する。
	 * @param Character 追加または検索する一文字。
	 */
	void PushBack(TChar Character)
	{
		EnsureTerminator_Internal();
		m_Characters.PushBack(TChar{});
		m_Characters[m_Characters.Size() - 2] = Character;
	}
	/**
	 * 末尾の要素を一つ破棄する。空なら何もしない。
	 */
	void PopBack() noexcept
	{
		if (!IsEmpty())
		{
			m_Characters.PopBack();
			m_Characters.Back() = TChar{};
		}
	}
	/**
	 * 末尾の要素を返す。呼び出し前に空でないことを確認する。
	 */
	TChar Back() const noexcept
	{
		return (*this)[Size() - 1];
	}
	/**
	 * 指定位置から文字列を複製する。開始位置が範囲外なら例外で通知する。
	 * @param Start 取り出しを始める文字位置。
	 * @param Count 処理する要素数。
	 */
	TBasicString Substr(size_t Start, size_t Count = TNumericLimits<size_t>::Max()) const
	{
		if (Start > Size())
		{
			throw FBadAccess();
		}
		return TBasicString(CStr() + Start, Min(Count, Size() - Start));
	}
	/**
	 * 値または文字列を末尾へ連結する。
	 */
	TBasicString& operator+=(TChar Character)
	{
		PushBack(Character);
		return *this;
	}
	/**
	 * 値または文字列を末尾へ連結する。
	 */
	TBasicString& operator+=(const TBasicString& Other)
	{
		if (&Other == this)
		{
			/**
			 * 元の値を保ったまま変更を準備する複製。
			 */
			TBasicString Copy(Other);
			return *this += Copy;
		}
		/**
		 * 変更前の文字数。追加分の書き込み開始位置に使う。
		 */
		const size_t Before = Size();
		if (Other.Size() >= TNumericLimits<size_t>::Max() - Before)
		{
			throw FException("String size overflow");
		}
		Resize(Before + Other.Size());
		/**
		 * 現在の要素位置を進めて範囲を走査する。
		 */
		for (size_t I = 0; I < Other.Size(); ++I)
		{
			m_Characters[Before + I] = Other[I];
		}
		return *this;
	}
	/**
	 * 値または文字列を末尾へ連結する。
	 */
	friend TBasicString operator+(TBasicString Left, const TBasicString& Right)
	{
		Left += Right;
		return Left;
	}
	/**
	 * 保持する値または対象が一致するか比較する。
	 */
	friend bool operator==(const TBasicString& Left, const TBasicString& Right)
	{
		if (Left.Size() != Right.Size())
		{
			return false;
		}
		/**
		 * 現在の要素位置を進めて範囲を走査する。
		 */
		for (size_t I = 0; I < Left.Size(); ++I)
		{
			if (Left[I] != Right[I])
			{
				return false;
			}
		}
		return true;
	}
	/**
	 * 辞書順で値を比較する。
	 */
	friend bool operator<(const TBasicString& Left, const TBasicString& Right)
	{
		/**
		 * 現在の要素位置を進めて範囲を走査する。
		 */
		for (size_t I = 0; I < Min(Left.Size(), Right.Size()); ++I)
		{
			if (Left[I] != Right[I])
			{
				return Left[I] < Right[I];
			}
		}
		return Left.Size() < Right.Size();
	}
	/**
	 * 文字列を所有しない参照へ変換する。元の文字列より長く保持しない。
	 */
	operator FStringView() const
	    requires IsSame<TChar, char>
	{
		return FStringView(CStr(), Size());
	}

private:
	/**
	 * 空の所有領域にも終端文字を一つ用意する。
	 */
	void EnsureTerminator_Internal()
	{
		if (m_Characters.IsEmpty())
		{
			m_Characters.PushBack(TChar{});
		}
	}
	/**
	 * 終端文字を含めて所有する文字領域。
	 */
	TVector<TChar> m_Characters;
	/**
	 * 動的領域がない場合に返す終端文字。
	 */
	inline static constexpr TChar m_Empty[1]{};
};
/**
 * UTF-8のバイト列を所有する文字列。
 */
using FString = TBasicString<char>;
/**
 * OSのワイド文字を所有する文字列。
 */
using FWideString = TBasicString<wchar_t>;
/**
 * 整数を10進数のUTF-8文字列へ変換する。
 * @param Value 処理または保持する値。
 */
template <typename T> FString ToString(T Value)
{
	/**
	 * OS呼び出しや変換に使う一時領域。
	 */
	char Buffer[64];
	if constexpr (T(-1) < T(0))
	{
		snprintf(Buffer, sizeof(Buffer), "%lld", static_cast<long long>(Value));
	}
	else
	{
		snprintf(Buffer, sizeof(Buffer), "%llu", static_cast<unsigned long long>(Value));
	}
	return Buffer;
}
} // namespace Toolbox
#endif
