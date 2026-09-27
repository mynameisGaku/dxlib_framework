// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_LOW_CEILING_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_LOW_CEILING_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * 上の段の低い天井（Staticの箱）。立ったカプセルは通れず、しゃがめば通れる。天井の下では立ち上がれない。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionLowCeiling final : public DGameObject
{
public:
	/**
	 * 天井の中心。
	 */
	static typename T::FVector Center() noexcept
	{
		return T::At(InteractionLayout::LowCeilingX,
		             InteractionLayout::LowCeilingBottom + InteractionLayout::LowCeilingHalfY);
	}

protected:
	/**
	 * Staticの剛体と箱のColliderを付ける。
	 * @param Context 初期化の実行環境。
	 */
	TResult<void> OnInitialize(const FInitContext& Context) override
	{
		(void)Context;
		auto Rigid = AddComponent<typename T::FRigid>(T::StaticBody(Center()));
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		auto Collider = AddComponent<typename T::FCollider>(
		    T::Box(InteractionLayout::LowCeilingHalfX, InteractionLayout::LowCeilingHalfY));
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
		}
		return {};
	}
};
} // namespace Dxf::GameplaySample
#endif
