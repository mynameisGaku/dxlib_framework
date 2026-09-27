// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_HAZARD_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_HAZARD_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * 危険領域（穴の下）。プレイヤーが入ると、シーンへ最後のチェックポイントへの復帰を求める。
 * 復帰は物理Stepの後の通知の中で行う（Worldは更新中ではない）。支持・速度・補間の履歴は復帰で初期化される。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionHazard final : public DGameObject
{
public:
	/**
	 * @param Host シーンの窓口。
	 */
	explicit TInteractionHazard(IInteractionHost<T>& Host) : m_pHost(&Host)
	{
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FTriggerDescription Description;
		Description.Shape = T::Box(InteractionLayout::HazardHalfX, InteractionLayout::HazardHalfY).Shape;
		Description.Position = T::At(InteractionLayout::HazardX, InteractionLayout::HazardY);
		auto Trigger = AddComponent<typename T::FTrigger>(Description);
		if (!Trigger)
		{
			return TResult<void>::Failure(Trigger.Error());
		}
		Trigger.Value().Get()->SetEnterHandler(
		    [this](typename T::FBodyId Body)
		    {
			    const auto Player = m_pHost->GetPlayerBody();
			    if (Player && Body == *Player)
			    {
				    m_pHost->RespawnPlayer();
			    }
		    });
		return {};
	}

private:
	/**
	 * シーンの窓口。
	 */
	IInteractionHost<T>* m_pHost;
};
} // namespace Dxf::GameplaySample
#endif
