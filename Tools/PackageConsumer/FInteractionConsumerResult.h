// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PACKAGE_INTERACTION_CONSUMER_RESULT_H
#define DXF_PACKAGE_INTERACTION_CONSUMER_RESULT_H
#include "Toolbox/Utility.h"

/**
 * 外部のGameplay利用者とNative Applicationが共通のシーンから受け取る検証結果。
 */
struct FInteractionConsumerResult
{
	/**
	 * 床追従、ジャンプ、通知中の破棄、領域からの退出を全て確認したか。
	 */
	bool bComplete = false;
	/**
	 * 成功した取得通知の数。自身を破棄するため1だけになる。
	 */
	Toolbox::int32 Pickups = 0;
	/**
	 * 破棄要求後に同じ所有者へ届いた通知数。0を維持する。
	 */
	Toolbox::int32 LatePickupCalls = 0;
	/**
	 * キャラクターのListenerが受け取ったTrigger Begin数。
	 */
	Toolbox::int32 Begins = 0;
	/**
	 * キャラクターのListenerが受け取ったTrigger Stay数。
	 */
	Toolbox::int32 Stays = 0;
	/**
	 * 取得物のCollider削除によるEnd数。
	 */
	Toolbox::int32 RemovedEnds = 0;
	/**
	 * 位置を復帰させた後、常設領域から出た通知数。
	 */
	Toolbox::int32 ZoneExits = 0;
	/**
	 * 支持速度を一度だけ継承してジャンプしたか。
	 */
	bool bJumped = false;
	/**
	 * 60回目の固定更新で得たキャラクターのX座標。
	 */
	Toolbox::f32 CarryX = 0;
};
#endif
