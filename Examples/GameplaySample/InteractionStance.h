// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_STANCE_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_STANCE_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * 遊び方（形状・押し合い）としゃがむ入力を、キャラクターの公開APIで反映する。
 * 円／球→カプセルは半高0のカプセル（同じ形）にしてから立った高さへ伸ばし、カプセル→円／球は半高0へ縮めてから戻す
 * （どちらも足元を保つ）。伸ばせない（低い天井の下など）間は低いまま、毎フレーム立ち上がりを試す。
 * @param Character プレイヤーのキャラクター。
 * @param Mode 遊び方。
 * @param bCrouch しゃがむ入力が押されているか。
 */
template <typename TCharacter>
void ApplyInteractionStance(TCharacter& Character, const FInteractionMode& Mode, bool bCrouch)
{
	auto Settings = Character.GetSettings();
	Settings.bPushDynamicBodies = Mode.IsPush();
	Settings.bReceiveDynamicPush = Mode.IsPush();
	Settings.MaxPushImpulse = InteractionLayout::PushImpulse;
	if (!Mode.IsCapsule())
	{
		if (Settings.Shape == ECharacterShape::Capsule && Settings.HalfHeight > 0)
		{
			Character.SetSettings(Settings);
			// 縮める変更は常にできる（Worldを受け取る前なら何もしない）。
			if (!Character.TrySetCapsuleHalfHeight(0))
			{
				return;
			}
			Settings = Character.GetSettings();
		}
		Settings.Shape = ECharacterShape::Round;
		Settings.HalfHeight = 0;
		Character.SetSettings(Settings);
		return;
	}
	if (Settings.Shape != ECharacterShape::Capsule)
	{
		Settings.Shape = ECharacterShape::Capsule;
		Settings.HalfHeight = 0;
	}
	Character.SetSettings(Settings);
	const Toolbox::f64 Wanted = bCrouch ? InteractionLayout::CrouchHalfHeight : InteractionLayout::StandHalfHeight;
	if (Character.GetSettings().HalfHeight != Wanted)
	{
		(void)Character.TrySetCapsuleHalfHeight(Wanted);
	}
}
/**
 * 毎フレーム、遊び方としゃがむ入力（[C]）をプレイヤーへ反映する（一時停止中は反映しない）。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionStance final : public DGameObject
{
public:
	/**
	 * シーンの窓口を受け取る。
	 * @param Host シーンの窓口（シーンはオブジェクトより長く生存する）。
	 */
	explicit TInteractionStance(IInteractionHost<T>& Host) noexcept : m_pHost(&Host)
	{
	}

protected:
	/**
	 * 遊び方としゃがむ入力を反映する。
	 * @param Context フレーム更新の実行環境。
	 */
	void OnTick(const FTickContext& Context) override
	{
		typename T::FPlayer* Player = m_pHost->GetPlayerObject();
		if (Player == nullptr || !Player->IsInitialized())
		{
			return;
		}
		ApplyInteractionStance(Player->GetCharacter(), m_pHost->GetMode(), Context.Input.IsDown(EKey::C));
	}

private:
	/**
	 * シーンの窓口。
	 */
	IInteractionHost<T>* m_pHost;
};
} // namespace Dxf::GameplaySample
#endif
