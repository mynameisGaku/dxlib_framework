// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_PLATFORM_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_PLATFORM_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * 動く床の種類。
 */
enum class EInteractionPlatform : Toolbox::uint8
{
	/**
	 * 穴の上を横に往復する床。
	 */
	Sliding,
	/**
	 * 床Bと上の段の間を上下する床。
	 */
	Lift,
	/**
	 * 上の段の間で回る床（2Dは傾き、3Dは鉛直軸回り）。
	 */
	Turning
};
/**
 * 動く床（Kinematic）。配置の経路（固定更新の累計秒数の関数）に沿って動き、乗ったキャラクターは床の運動に追従する。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionPlatform final : public DGameObject
{
public:
	/**
	 * @param Kind 床の種類。
	 */
	explicit TInteractionPlatform(EInteractionPlatform Kind) noexcept : m_Kind(Kind)
	{
	}
	/**
	 * 経路の時刻での位置・向き。
	 * @param Seconds 経路の時刻。
	 */
	typename T::FPose PoseAt(Toolbox::f64 Seconds) const
	{
		if (m_Kind == EInteractionPlatform::Turning)
		{
			return T::Turn(Seconds);
		}
		const Toolbox::FVector2 At = m_Kind == EInteractionPlatform::Sliding ? InteractionLayout::PlatformAt(Seconds)
		                                                                     : InteractionLayout::LiftAt(Seconds);
		typename T::FPose Pose;
		Pose.Position = T::At(At.X, At.Y);
		return Pose;
	}
	/**
	 * 床の種類。
	 */
	FORCEINLINE EInteractionPlatform GetKind() const noexcept
	{
		return m_Kind;
	}
	/**
	 * 床の半幅（描画に使う）。
	 */
	Toolbox::FVector2 GetHalf() const noexcept
	{
		if (m_Kind == EInteractionPlatform::Turning)
		{
			return {InteractionLayout::TurnHalf, InteractionLayout::PlatformHalfY};
		}
		return {m_Kind == EInteractionPlatform::Sliding ? InteractionLayout::PlatformHalfX
		                                                : InteractionLayout::LiftHalfX,
		        InteractionLayout::PlatformHalfY};
	}
	/**
	 * 運動のComponent。
	 */
	const typename T::FMover* GetMover() const noexcept
	{
		return m_Mover.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FMoverDescription Description;
		const Toolbox::FVector2 Half = GetHalf();
		auto Collider = T::Box(Half.X, Half.Y);
		if constexpr (sizeof(typename T::FVector) == sizeof(Toolbox::FVector3))
		{
			// 3Dの回る床は正方形の円盤に近い板（奥行きも同じ半幅）。
			if (m_Kind == EInteractionPlatform::Turning)
			{
				Collider.Shape = Toolbox::FOBB{{0, 0, 0}, {Half.X, Half.Y, Half.X}};
			}
		}
		Description.Colliders.PushBack(Collider);
		Description.Pose = PoseAt(0);
		auto Mover = AddComponent<typename T::FMover>(Description);
		if (!Mover)
		{
			return TResult<void>::Failure(Mover.Error());
		}
		m_Mover = Mover.Value();
		Mover.Value().Get()->SetPath(
		    [this](Toolbox::f64 Seconds)
		    {
			    return PoseAt(Seconds);
		    });
		return {};
	}

private:
	/**
	 * 床の種類。
	 */
	EInteractionPlatform m_Kind;
	/**
	 * 運動のComponent。
	 */
	TObjectHandle<typename T::FMover> m_Mover;
};
} // namespace Dxf::GameplaySample
#endif
