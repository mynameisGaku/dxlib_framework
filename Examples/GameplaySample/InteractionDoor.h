// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_DOOR_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_DOOR_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * 扉（Kinematic）。規則が開けておくと決めている間は上へ、そうでなければ下へ、上限の速さで動く。
 * 規則の時間（扉の保持）は、このオブジェクトの固定更新で一回だけ進める（子の運動のComponentより先に呼ばれる）。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionDoor final : public DGameObject
{
public:
	/**
	 * @param Host シーンの窓口。
	 */
	explicit TInteractionDoor(IInteractionHost<T>& Host) : m_pHost(&Host)
	{
	}
	/**
	 * 運動のComponent（描画に使う）。
	 */
	const typename T::FMover* GetMover() const noexcept
	{
		return m_Mover.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FMoverDescription Description;
		Description.Colliders.PushBack(T::Box(InteractionLayout::DoorHalfX, InteractionLayout::DoorHalfY));
		Description.Pose.Position = T::At(InteractionLayout::DoorX, InteractionLayout::DoorClosedY);
		Description.MaxLinearSpeed = InteractionLayout::DoorSpeed;
		auto Mover = AddComponent<typename T::FMover>(Description);
		if (!Mover)
		{
			return TResult<void>::Failure(Mover.Error());
		}
		m_Mover = Mover.Value();
		Mover.Value().Get()->SetPath(
		    [this](Toolbox::f64)
		    {
			    typename T::FPose Pose;
			    Pose.Position =
			        T::At(InteractionLayout::DoorX, m_pHost->GetRules().IsDoorOpen() ? InteractionLayout::DoorOpenY
			                                                                         : InteractionLayout::DoorClosedY);
			    return Pose;
		    });
		return {};
	}
	void OnFixedTick(const FFixedTickContext& Context) override
	{
		m_pHost->GetRules().Advance(Context.DeltaSeconds);
	}

private:
	/**
	 * シーンの窓口。
	 */
	IInteractionHost<T>* m_pHost;
	/**
	 * 運動のComponent。
	 */
	TObjectHandle<typename T::FMover> m_Mover;
};
} // namespace Dxf::GameplaySample
#endif
