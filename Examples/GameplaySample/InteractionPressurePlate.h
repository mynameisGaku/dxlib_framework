// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_PRESSURE_PLATE_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_PRESSURE_PLATE_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * 圧力板。入っているBody（プレイヤー・箱など。Colliderの数ではなくBodyの数）を規則へ伝え、扉の開閉を決めさせる。
 * 複数のColliderを持つBodyや、複数の当事者の出入りでも、全員が出るまで空にならない。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionPressurePlate final : public DGameObject
{
public:
	/**
	 * @param Host シーンの窓口。
	 */
	explicit TInteractionPressurePlate(IInteractionHost<T>& Host) : m_pHost(&Host)
	{
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FTriggerDescription Description;
		Description.Shape = T::Box(InteractionLayout::PlateHalfX, InteractionLayout::PlateHalfY).Shape;
		Description.Position = T::At(InteractionLayout::PlateX, InteractionLayout::PlateY);
		auto Trigger = AddComponent<typename T::FTrigger>(Description);
		if (!Trigger)
		{
			return TResult<void>::Failure(Trigger.Error());
		}
		m_Trigger = Trigger.Value();
		Trigger.Value().Get()->SetEnterHandler(
		    [this](typename T::FBodyId)
		    {
			    Update_Internal();
		    });
		Trigger.Value().Get()->SetExitHandler(
		    [this](typename T::FBodyId, EWorldEventEndReason)
		    {
			    Update_Internal();
		    });
		return {};
	}

private:
	/**
	 * 入っているBodyの数を規則へ伝える。
	 */
	void Update_Internal()
	{
		if (const typename T::FTrigger* Trigger = m_Trigger.Get())
		{
			m_pHost->GetRules().SetPlateOccupants(static_cast<Toolbox::int32>(Trigger->GetOccupantCount()));
		}
	}
	/**
	 * シーンの窓口。
	 */
	IInteractionHost<T>* m_pHost;
	/**
	 * 領域。
	 */
	TObjectHandle<typename T::FTrigger> m_Trigger;
};
} // namespace Dxf::GameplaySample
#endif
