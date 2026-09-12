// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_ALGORITHM_H
#define TOOLBOX_ALGORITHM_H
#include "Toolbox/Vector.h"
namespace Toolbox
{
/**
 * 二つの値を順番に比較するキー。構造化束縛にも使用できる。
 */
template <typename A, typename B> struct TPair
{
	/**
	 * 比較または格納する組の第一成分。
	 */
	A First;
	/**
	 * 比較または格納する組の第二成分。
	 */
	B Second;
	/**
	 * 辞書順で値を比較する。
	 */
	FORCEINLINE bool operator<(const TPair& Other) const
	{
		return First < Other.First || (!(Other.First < First) && Second < Other.Second);
	}
	/**
	 * 保持する値または対象が一致するか比較する。
	 */
	bool operator==(const TPair&) const = default;
};
/**
 * 二つの初期値から組の各要素型を推論する。
 */
template <typename A, typename B> TPair(A, B) -> TPair<A, B>;
/**
 * 範囲内の最初の一致を返す。見つからなければLastを返す。
 * @param First 検索範囲の先頭。
 * @param Last 検索範囲の終端。
 * @param Value 一致を調べる値。
 */
template <typename It, typename V> It Find(It First, It Last, const V& Value)
{
	for (; First != Last; ++First)
	{
		if (*First == Value)
		{
			break;
		}
	}
	return First;
}
/**
 * 範囲内で指定値と一致する要素数を数える。
 * @param First 入力範囲の先頭。
 * @param Last 入力範囲の終端。
 * @param Value 処理または保持する値。
 */
template <typename It, typename V> size_t Count(It First, It Last, const V& Value)
{
	// 条件と一致した要素数の累計。
	size_t Total = 0;
	for (; First != Last; ++First)
	{
		if (*First == Value)
		{
			++Total;
		}
	}
	return Total;
}
/**
 * 入力範囲の要素を出力先へ順番に代入する。出力領域は事前に用意する。
 * @param First 入力範囲の先頭。
 * @param Last 入力範囲の終端。
 * @param Output 処理結果を書き込む領域。
 */
template <typename It, typename Out> Out Copy(It First, It Last, Out Output)
{
	for (; First != Last; ++First, ++Output)
	{
		*Output = *First;
	}
	return Output;
}
/**
 * 安定なマージソート。同順位の入力順を保ち、比較回数をO(n log n)に抑える。
 * @param First 入力範囲の先頭。
 * @param Last 入力範囲の終端。
 * @param Less 左の要素を先に並べる場合にtrueを返す比較関数。
 */
template <typename T, typename Compare> void StableSort(T* First, T* Last, Compare Less)
{
	if (First == Last)
	{
		return;
	}
	// 並べ替える要素の総数。
	const size_t Size = static_cast<size_t>(Last - First);
	if (Size < 2)
	{
		return;
	}
	// 左右のソート範囲を分ける位置。
	const size_t Middle = Size / 2;
	StableSort(First, First + Middle, Less);
	StableSort(First + Middle, Last, Less);
	// 左右の要素を安定順に統合する一時領域。
	TVector<T> Merged;
	Merged.Reserve(Size);
	// 左半分の未処理要素の位置。
	size_t Left = 0;
	// 右半分の未処理要素の位置。
	size_t Right = Middle;
	while (Left < Middle && Right < Size)
	{
		if (Less(First[Right], First[Left]))
		{
			Merged.PushBack(Move(First[Right++]));
		}
		else
		{
			Merged.PushBack(Move(First[Left++]));
		}
	}
	while (Left < Middle)
	{
		Merged.PushBack(Move(First[Left++]));
	}
	while (Right < Size)
	{
		Merged.PushBack(Move(First[Right++]));
	}
	// 現在の要素位置を進めて範囲を走査する。
	for (size_t I = 0; I < Size; ++I)
	{
		First[I] = Move(Merged[I]);
	}
}
/**
 * 指定された順序に従って範囲を並べ替える。
 * @param First 入力範囲の先頭。
 * @param Last 入力範囲の終端。
 * @param Less 左の要素を先に並べる場合にtrueを返す比較関数。
 */
template <typename T, typename Compare> void Sort(T* First, T* Last, Compare Less)
{
	StableSort(First, Last, Less);
}
/**
 * 小なり演算子の昇順で範囲を安定に並べ替える。
 * @param First 並べ替える範囲の先頭。
 * @param Last 並べ替える範囲の終端。
 */
template <typename T> void Sort(T* First, T* Last)
{
	StableSort(First, Last,
	           [](const T& A, const T& B)
	           {
		           return A < B;
	           });
}
/**
 * 条件に合う要素を順序を保って除去する。
 * @param Values 条件に合う要素を削除するコンテナー。
 * @param Match 除去対象ならtrueを返す条件。
 */
template <typename Container, typename Predicate> size_t EraseIf(Container& Values, Predicate Match)
{
	// 取り除いた要素数の累計。
	size_t Removed = 0;
	// 削除後の位置を引き継ぎながら反復子を進める。
	for (auto It = Values.Begin(); It != Values.End();)
	{
		if (Match(*It))
		{
			It = Values.Erase(It);
			++Removed;
		}
		else
		{
			++It;
		}
	}
	return Removed;
}
} // namespace Toolbox
#endif
