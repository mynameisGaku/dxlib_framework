// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_ESAVELOADSTATE_H
#define DXF_ESAVELOADSTATE_H
namespace Dxf
{
/**
 * 二つの確定面を読んだ結果。将来バージョン・両面破損・競合は別の例外となる。
 */
enum class ESaveLoadState
{
	/**
	 * 確定面が一つもない。
	 */
	Empty,
	/**
	 * 最新の正常面を読めた。
	 */
	Loaded,
	/**
	 * 不正な面を避け、残る正常保存から回復した。
	 */
	RecoveredPrevious
};
} // namespace Dxf
#endif
