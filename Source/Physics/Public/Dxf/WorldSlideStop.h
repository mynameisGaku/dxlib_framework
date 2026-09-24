// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_WORLD_SLIDE_STOP_H
#define DXF_WORLD_SLIDE_STOP_H
namespace Dxf
{
/**
 * ComputeSlideMoveが最終中心を選んだ理由。通常の停止は例外ではない。
 */
enum class EWorldSlideStop
{
	/**
	 * 開始中心と希望終点が同じで、開始時の接触もなかった。
	 */
	NoMovement,
	/**
	 * 最初の移動が非交差で、希望終点へ到達した。
	 */
	ReachedDesiredEnd,
	/**
	 * 最初の接触で方向を1回補正し、その滑り経路を非交差で走り終えた。元の希望終点とは限らない。
	 */
	SlideCompleted,
	/**
	 * 障害物で止まった。最初の接触で補正後の移動が残らない場合と、滑り経路で接触した場合。
	 */
	Blocked,
	/**
	 * 最初の移動の開始時に接触・重なりがあった。開始中心のまま返し、離脱・押し出しは行わない。
	 */
	InitialContact,
	/**
	 * 最初の接触に有効な法線がなかった。接触の手前で止め、方向補正は行わない。
	 */
	MissingNormal,
	/**
	 * f32へ丸めた候補の再検査で接触した、または補正後の終点が同じ中心へ丸められ、これ以上進めない。
	 */
	PrecisionLimit,
};
} // namespace Dxf
#endif
