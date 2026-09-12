// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/Vector3.h"
namespace Toolbox
{
#if TOOLBOX_SIMD_SSE2
namespace
{
/**
 * ベクトルを四レーンのSIMDレジスターへ移す。
 */
__m128 Load(FVector3 Value) noexcept
{
	return _mm_set_ps(0, Value.Z, Value.Y, Value.X);
}
/**
 * SIMDの有効な三成分を座標へ戻す。
 */
FVector3 Store(__m128 Value) noexcept
{
	/**
	 * SIMDレーンを書き戻す整列済み領域。
	 */
	alignas(16) f32 Components[4];
	_mm_store_ps(Components, Value);
	return {Components[0], Components[1], Components[2]};
}
} // namespace
#endif
FVector3 FVector3::operator+(FVector3 Other) const noexcept
{
#if TOOLBOX_SIMD_SSE2
	return Store(_mm_add_ps(Load(*this), Load(Other)));
#else
	return {X + Other.X, Y + Other.Y, Z + Other.Z};
#endif
}
FVector3 FVector3::operator-(FVector3 Other) const noexcept
{
#if TOOLBOX_SIMD_SSE2
	return Store(_mm_sub_ps(Load(*this), Load(Other)));
#else
	return {X - Other.X, Y - Other.Y, Z - Other.Z};
#endif
}
FVector3 FVector3::operator*(f32 Scale) const noexcept
{
#if TOOLBOX_SIMD_SSE2
	return Store(_mm_mul_ps(Load(*this), _mm_set1_ps(Scale)));
#else
	return {X * Scale, Y * Scale, Z * Scale};
#endif
}
f32 Dot(FVector3 A, FVector3 B) noexcept
{
#if TOOLBOX_SIMD_SSE2
	/**
	 * 対応する成分の積。
	 */
	const FVector3 Product = Store(_mm_mul_ps(Load(A), Load(B)));
	return Product.X + Product.Y + Product.Z;
#else
	return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
#endif
}
FVector3 Cross(FVector3 A, FVector3 B) noexcept
{
#if TOOLBOX_SIMD_SSE2
	/**
	 * 演算の左側に使用する値。
	 */
	const __m128 Left = Load(A);
	/**
	 * 演算の右側に使用する値。
	 */
	const __m128 Right = Load(B);
	return Store(_mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(Left, Left, _MM_SHUFFLE(3, 0, 2, 1)),
	                                   _mm_shuffle_ps(Right, Right, _MM_SHUFFLE(3, 1, 0, 2))),
	                        _mm_mul_ps(_mm_shuffle_ps(Left, Left, _MM_SHUFFLE(3, 1, 0, 2)),
	                                   _mm_shuffle_ps(Right, Right, _MM_SHUFFLE(3, 0, 2, 1)))));
#else
	return {A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X};
#endif
}
} // namespace Toolbox
