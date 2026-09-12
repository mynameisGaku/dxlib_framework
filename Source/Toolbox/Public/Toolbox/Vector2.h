// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_VECTOR2_H
#define TOOLBOX_VECTOR2_H
#include "Toolbox/Utility.h"
namespace Toolbox
{
/**
 * 2D衝突判定へ渡す座標またはフレーム全体の変位。
 */
struct FVector2
{
	/**
	 * X軸成分。
	 */
	f32 X = 0;
	/**
	 * Y軸成分。
	 */
	f32 Y = 0;
	/**
	 * 指定した二成分を保持する。
	 * @param InX X軸の値。
	 * @param InY Y軸の値。
	 */
	FORCEINLINE constexpr FVector2(f32 InX = 0, f32 InY = 0) noexcept : X(InX), Y(InY)
	{
	}
	/**
	 * 全成分が有限かを返す。
	 */
	FORCEINLINE bool IsValid() const noexcept
	{
		return IsFinite(X) && IsFinite(Y);
	}
};
} // namespace Toolbox
#endif
