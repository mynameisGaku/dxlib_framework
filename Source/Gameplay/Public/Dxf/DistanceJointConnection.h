// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_DISTANCEJOINTCONNECTION_H
#define DXF_GAMEPLAY_DISTANCEJOINTCONNECTION_H
namespace Dxf
{
/**
 * 距離拘束の接続状態。生成と解除は固定更新境界で反映する。
 */
enum class EDistanceJointConnection
{
	/**
	 * 登録前の有効なBody参照を待っている。
	 */
	PendingBodies,
	/**
	 * 現在のWorldに拘束が生存している。
	 */
	Connected,
	/**
	 * 明示的に解除している。
	 */
	Disconnected,
	/**
	 * 接続先または拘束が失効した。明示要求まで再生成しない。
	 */
	EndpointLost
};
} // namespace Dxf
#endif
