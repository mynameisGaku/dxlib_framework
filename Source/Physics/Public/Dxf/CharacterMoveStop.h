// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CHARACTER_MOVE_STOP_H
#define DXF_CHARACTER_MOVE_STOP_H
namespace Dxf
{
/**
 * MoveAndSlideが止まった理由。上限や方向不明は到達成功ではなく、未処理の移動は結果のRemainingに残る。
 */
enum class ECharacterMoveStop
{
	/**
	 * 要求された移動がMinMoveDistance未満だった。
	 */
	NoMovement,
	/**
	 * 要求された移動を接触なしにすべて行った。
	 */
	Completed,
	/**
	 * 接触面に沿って向きを変えながら、補正後の移動を最後まで行った（元の終点とは限らない）。
	 */
	Slid,
	/**
	 * 接触面の制約で、これ以上進める成分がない（壁・角・稜線・囲まれた場所）。
	 */
	Blocked,
	/**
	 * 接触したが法線を得られず、接触の手前で止めた。
	 */
	MissingNormal,
	/**
	 * 開始時に接触・重なりがあり、その方向を一つに決められない。移動していない。
	 */
	AmbiguousContact,
	/**
	 * f32への丸めで進めない、または丸めた位置の再検査で接触した。
	 */
	PrecisionLimit,
	/**
	 * 反復上限に達した。
	 */
	IterationLimit,
	/**
	 * 接触の件数が保持の上限を超え、全件を制約として扱えない。
	 */
	ContactLimit,
	/**
	 * 問い合わせの上限に達した。
	 */
	QueryLimit,
};
} // namespace Dxf
#endif
