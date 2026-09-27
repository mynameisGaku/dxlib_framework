// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_PICKUP_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_PICKUP_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * 取得物。プレイヤーが入った最初の通知で一度だけ取得し、自分のGameObjectの破棄を要求する（Sensorは境界で削除される）。
 * 破棄を要求した後は通知を受けない（同じ組の二度目のBeginでもう一度取得しない）。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionPickup final : public DGameObject
{
public:
	/**
	 * @param Host シーンの窓口。
	 * @param Index 配置の番号。
	 */
	TInteractionPickup(IInteractionHost<T>& Host, Toolbox::int32 Index) : m_pHost(&Host), m_Index(Index)
	{
	}
	/**
	 * 位置（描画に使う）。
	 */
	typename T::FVector GetPosition() const noexcept
	{
		return T::At(InteractionLayout::PickupX[m_Index], InteractionLayout::PickupY);
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FTriggerDescription Description;
		Description.Shape = T::Ball(InteractionLayout::PickupRadius).Shape;
		Description.Position = GetPosition();
		auto Trigger = AddComponent<typename T::FTrigger>(Description);
		if (!Trigger)
		{
			return TResult<void>::Failure(Trigger.Error());
		}
		Trigger.Value().Get()->SetEnterHandler(
		    [this](typename T::FBodyId Body)
		    {
			    const auto Player = m_pHost->GetPlayerBody();
			    if (IsDestroyRequested() || !Player || !(Body == *Player))
			    {
				    return;
			    }
			    m_pHost->GetRules().Collect();
			    Destroy();
		    });
		return {};
	}

private:
	/**
	 * シーンの窓口。
	 */
	IInteractionHost<T>* m_pHost;
	/**
	 * 配置の番号。
	 */
	Toolbox::int32 m_Index;
};
} // namespace Dxf::GameplaySample
#endif
