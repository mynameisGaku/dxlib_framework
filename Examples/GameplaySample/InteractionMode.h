// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_MODE_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_MODE_H
#include "Toolbox/Utility.h"
namespace Dxf::GameplaySample
{
/**
 * 接触・Triggerのサンプルのキャラクターの遊び方（形状と押し合い）。既定は円／球・押し合いなし（従来の挙動）。
 * 設定画面のボタンで切り替え、キャラクターへの反映はTInteractionStanceが毎フレーム行う。
 */
class FInteractionMode
{
public:
	/**
	 * キャラクターがカプセル（中心線が上向き）か。falseなら円／球。
	 */
	FORCEINLINE bool IsCapsule() const noexcept
	{
		return m_bCapsule;
	}
	/**
	 * Dynamicの箱を押し、近づく箱に押されるか。
	 */
	FORCEINLINE bool IsPush() const noexcept
	{
		return m_bPush;
	}
	/**
	 * 円／球とカプセルを切り替える。
	 */
	FORCEINLINE void ToggleShape() noexcept
	{
		m_bCapsule = !m_bCapsule;
	}
	/**
	 * 押し合いの有無を切り替える。
	 */
	FORCEINLINE void TogglePush() noexcept
	{
		m_bPush = !m_bPush;
	}

private:
	/**
	 * カプセルか。
	 */
	bool m_bCapsule = false;
	/**
	 * 押し合うか。
	 */
	bool m_bPush = false;
};
/**
 * 遊び方の表示用の文字列を作る（例: "shape Capsule  push On  [C] crouch"）。
 * @param Mode 遊び方。
 * @param Out 出力先。
 */
void InteractionModeText(const FInteractionMode& Mode, char (&Out)[256]) noexcept;
} // namespace Dxf::GameplaySample
#endif
