// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Complex.h"
#include "Toolbox/Vector3.h"
namespace Toolbox
{
// 二個の複素数をSSE2で同時に乗算し、奇数末尾はスカラーで処理する。
// @param A 左側の入力配列。
// @param B 右側の入力配列。
// @param Output 積の出力配列。入力との完全な重なりを許可する。
// @param Count 乗算する複素数の個数。
void MultiplyComplex(const FComplex32* A, const FComplex32* B, FComplex32* Output, size_t Count)
{
	if (Count && (!A || !B || !Output))
	{
		throw FException("Null complex array");
	}
	// 次に乗算する複素数の番号。
	size_t Index = 0;
#if TOOLBOX_SIMD_SSE2
	static_assert(sizeof(FComplex32) == 2 * sizeof(f32));
	for (; Count - Index >= 2; Index += 2)
	{
		// 左側二要素を実部・虚部の順で並べたレーン。
		const __m128 Left = _mm_set_ps(A[Index + 1].Imaginary, A[Index + 1].Real, A[Index].Imaginary, A[Index].Real);
		// 右側二要素を実部・虚部の順で並べたレーン。
		const __m128 Right = _mm_set_ps(B[Index + 1].Imaginary, B[Index + 1].Real, B[Index].Imaginary, B[Index].Real);
		// 各要素の実部を隣接レーンへ複製した係数。
		const __m128 Real = _mm_shuffle_ps(Left, Left, _MM_SHUFFLE(2, 2, 0, 0));
		// 各要素の虚部を隣接レーンへ複製した係数。
		const __m128 Imaginary = _mm_shuffle_ps(Left, Left, _MM_SHUFFLE(3, 3, 1, 1));
		// 交差項のため実部と虚部を交換した右側レーン。
		const __m128 Swapped = _mm_shuffle_ps(Right, Right, _MM_SHUFFLE(2, 3, 0, 1));
		// 虚数単位の二乗による符号反転を適用した二要素の積。
		const __m128 Product =
		    _mm_add_ps(_mm_mul_ps(Real, Right), _mm_mul_ps(_mm_mul_ps(Imaginary, Swapped), _mm_set_ps(1, -1, 1, -1)));
		// レーンを公開型へ戻すための整列済み一時領域。
		alignas(16) f32 Values[4];
		_mm_store_ps(Values, Product);
		Output[Index] = {Values[0], Values[1]};
		Output[Index + 1] = {Values[2], Values[3]};
	}
#endif
	for (; Index < Count; ++Index)
	{
		Output[Index] = A[Index] * B[Index];
	}
}
// 倍精度の実部と虚部をSSE2の二レーンで同時に計算する。
// @param A 左側の入力配列。
// @param B 右側の入力配列。
// @param Output 積の出力配列。
// @param Count 乗算する複素数の個数。
void MultiplyComplex(const FComplex64* A, const FComplex64* B, FComplex64* Output, size_t Count)
{
	if (Count && (!A || !B || !Output))
	{
		throw FException("Null complex array");
	}
	// 配列を先頭から一要素ずつ処理する。
	for (size_t Index = 0; Index < Count; ++Index)
	{
#if TOOLBOX_SIMD_SSE2
		// 左側の実部と虚部。
		const __m128d Left = _mm_set_pd(A[Index].Imaginary, A[Index].Real);
		// 右側の実部と虚部。
		const __m128d Right = _mm_set_pd(B[Index].Imaginary, B[Index].Real);
		// 実部の減算と虚部の加算をまとめた複素積。
		const __m128d Product = _mm_add_pd(
		    _mm_mul_pd(_mm_unpacklo_pd(Left, Left), Right),
		    _mm_mul_pd(_mm_mul_pd(_mm_unpackhi_pd(Left, Left), _mm_shuffle_pd(Right, Right, 1)), _mm_set_pd(1, -1)));
		// 二つの倍精度レーンを書き戻す一時領域。
		alignas(16) f64 Values[2];
		_mm_store_pd(Values, Product);
		Output[Index] = {Values[0], Values[1]};
#else
		Output[Index] = A[Index] * B[Index];
#endif
	}
}
} // namespace Toolbox
