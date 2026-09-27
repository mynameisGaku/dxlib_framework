// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_HUD_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_HUD_H
#include "InteractionRules.h"
#include "SampleHud.h"
namespace Dxf::GameplaySample
{
/**
 * 規則の状態（取得数・扉・圧力板・箱との接触・チェックポイント・復帰）の1行。
 * @param Rules 規則。
 * @param Text 書き込み先。
 */
void InteractionRulesText(const FInteractionRules& Rules, char (&Text)[256]);
/**
 * 動く床への追従の状態（乗っている床・運ばれたか・挟まれ・上限超過・受け継ぎ・床の点の速度）の1行。
 * @param Step キャラクターの直前の固定更新の結果。
 * @param Seconds 固定更新の秒数（床の点の速度を求める）。
 * @param Text 書き込み先。
 */
template <typename TStep> void InteractionSupportText(const TStep& Step, Toolbox::f64 Seconds, char (&Text)[256])
{
	const auto& Moved = Step.CarryRequested;
	const Toolbox::f64 Scale = Seconds > 0 ? 1.0 / Seconds : 0.0;
	char Velocity[96];
	if constexpr (sizeof(Moved) == sizeof(Toolbox::f32) * 2)
	{
		snprintf(Velocity, sizeof(Velocity), "(%.2f, %.2f)", Moved.X * Scale, Moved.Y * Scale);
	}
	else
	{
		snprintf(Velocity, sizeof(Velocity), "(%.2f, %.2f, %.2f)", Moved.X * Scale, Moved.Y * Scale, Moved.Z * Scale);
	}
	snprintf(Text, sizeof(Text), "support %s  carried %s%s%s%s  floor velocity %s  carry stop %s",
	         Step.Carrier ? "moving floor" : "none", Step.bCarried ? "yes" : "no",
	         Step.bCarryBlocked ? " [blocked]" : "", Step.bCarryRejected ? " [rejected]" : "",
	         Step.bInheritedGroundVelocity ? " [inherited]" : "", Velocity,
	         Step.bCarried ? MoveStopName(Step.Carry.Stop) : "-");
}
} // namespace Dxf::GameplaySample
#endif
