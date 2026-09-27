// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_CHECKPOINT_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_CHECKPOINT_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * チェックポイント。プレイヤーが入ると、その番号を復帰先として規則へ記録する（小さい番号へは戻らない）。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionCheckpoint final : public DGameObject
{
public:
	/**
	 * @param Host シーンの窓口。
	 * @param Index チェックポイントの番号。
	 */
	TInteractionCheckpoint(IInteractionHost<T>& Host, Toolbox::int32 Index) : m_pHost(&Host), m_Index(Index)
	{
	}
	/**
	 * 位置（描画に使う）。
	 */
	typename T::FVector GetPosition() const noexcept
	{
		return T::At(InteractionLayout::CheckpointX[m_Index], InteractionLayout::CheckpointY[m_Index]);
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FTriggerDescription Description;
		Description.Shape = T::Box(InteractionLayout::CheckpointHalf, InteractionLayout::CheckpointHalf).Shape;
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
			    if (Player && Body == *Player)
			    {
				    m_pHost->GetRules().ReachCheckpoint(m_Index);
			    }
		    });
		return {};
	}

private:
	/**
	 * シーンの窓口。
	 */
	IInteractionHost<T>* m_pHost;
	/**
	 * チェックポイントの番号。
	 */
	Toolbox::int32 m_Index;
};
} // namespace Dxf::GameplaySample
#endif
