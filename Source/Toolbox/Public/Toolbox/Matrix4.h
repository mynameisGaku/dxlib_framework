// SPDX-License-Identifier: NOASSERTION
#ifndef TOOLBOX_MATRIX4_H
#define TOOLBOX_MATRIX4_H
#include "Toolbox/Vector3.h"
#include "Toolbox/Array.h"
namespace Toolbox
{
/**
 * 行優先の4×4行列。列ベクトルへ作用し、A*BはBを先に適用する。
 */
struct alignas(16) FMatrix4
{
	/**
	 * 行番号*4+列番号で参照する行列要素。初期値は単位行列。
	 */
	TArray<f32, 16> Values{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	/**
	 * 平行移動行列を作る。
	 * @param Offset 移動量。
	 */
	static FMatrix4 Translation(FVector3 Offset);
	/**
	 * 各軸の拡大縮小行列を作る。
	 * @param Factors 各軸の拡大率。
	 */
	static FMatrix4 Scale(FVector3 Factors);
	/**
	 * ラジアン角の軸回転行列を作る。ゼロ軸は例外で通知する。
	 * @param Axis 回転軸または参照する軸番号。
	 * @param Radians ラジアン単位の回転角。
	 */
	static FMatrix4 Rotation(FVector3 Axis, f32 Radians);
	/**
	 * 四要素をまとめて処理し、行列を合成する。
	 * @param Other 演算または比較の相手。
	 */
	FMatrix4 operator*(const FMatrix4& Other) const noexcept;
	/**
	 * 同次座標を除算して位置を変換する。無限遠は例外で通知する。
	 * @param Point 変換する位置。
	 */
	FVector3 TransformPoint(FVector3 Point) const;
	/**
	 * 平行移動を含めず方向を変換する。
	 * @param Direction 変換または支持点検索の方向。
	 */
	FVector3 TransformDirection(FVector3 Direction) const noexcept;
	/**
	 * 行と列を入れ替える。
	 */
	FMatrix4 Transposed() const noexcept;
	/**
	 * 逆行列を求める。特異または非有限ならfalseを返し、出力は変更しない。
	 * @param Output 結果を書き込む先。
	 * @param Tolerance 許容する数値誤差。
	 */
	bool TryInverse(FMatrix4& Output, f32 Tolerance = 1e-7f) const noexcept;
};
} // namespace Toolbox
#endif
