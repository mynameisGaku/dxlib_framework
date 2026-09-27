// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_CRATE_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_CRATE_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * サンプルの箱の役割。
 */
enum class EInteractionCrate : Toolbox::uint8
{
	/**
	 * 触れる箱（半幅0.4、質量1）。プレイヤーとの接触を規則へ伝え、押し合いでは圧力板まで押せる。
	 */
	Touch,
	/**
	 * 重い箱（半幅0.5、質量50）。押しても摩擦に負けて動かない。
	 */
	Heavy,
	/**
	 * 昇降床に載せた箱（半幅0.3、質量1）。床と一緒に上下する。
	 */
	Lift,
};
/**
 * 箱（Dynamic）。触れる箱は自分の剛体の接触（Solid）を監視し、プレイヤーとのBegin／Endを規則へ伝える。
 * 箱と床の接触点が複数あっても、プレイヤーとの組は一つの組としてBeginが一度になる。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionCrate final : public DGameObject
{
public:
	/**
	 * @param Host シーンの窓口（シーンはオブジェクトより長く生存する）。
	 * @param Role 箱の役割（既定は触れる箱）。
	 */
	explicit TInteractionCrate(IInteractionHost<T>& Host, EInteractionCrate Role = EInteractionCrate::Touch)
	    : m_pHost(&Host), m_Role(Role)
	{
	}
	/**
	 * 箱の役割。
	 */
	FORCEINLINE EInteractionCrate GetRole() const noexcept
	{
		return m_Role;
	}
	/**
	 * 箱の半幅。
	 */
	Toolbox::f32 GetHalf() const noexcept
	{
		return m_Role == EInteractionCrate::Heavy
		           ? InteractionLayout::HeavyCrateHalf
		           : (m_Role == EInteractionCrate::Lift ? InteractionLayout::LiftCrateHalf
		                                                : InteractionLayout::CrateHalf);
	}
	/**
	 * 剛体（描画に使う）。
	 */
	const typename T::FRigid* GetRigid() const noexcept
	{
		return m_Rigid.Get();
	}

protected:
	TResult<void> OnInitialize(const FInitContext&) override
	{
		typename T::FBodyDescription Body;
		const Toolbox::f32 Half = GetHalf();
		Body.Position = T::At(InteractionLayout::CrateX, InteractionLayout::CrateHalf);
		if (m_Role == EInteractionCrate::Heavy)
		{
			Body.Position = T::At(InteractionLayout::HeavyCrateX, Half);
			// 立方体の慣性（質量×(2h)²／6）。
			const Toolbox::f32 Inertia = InteractionLayout::HeavyCrateMass * 4 * Half * Half / 6;
			Body.Mass = InteractionLayout::HeavyCrateMass;
			if constexpr (sizeof(typename T::FVector) == sizeof(Toolbox::FVector3))
			{
				Body.DiagonalInertia = {Inertia, Inertia, Inertia};
			}
			else
			{
				Body.Inertia = Inertia;
			}
		}
		else if (m_Role == EInteractionCrate::Lift)
		{
			// 開始時の昇降床の上面（中心＋半高）に載せる。
			const Toolbox::FVector2 Lift = InteractionLayout::LiftAt(0);
			Body.Position = T::At(Lift.X, Lift.Y + InteractionLayout::PlatformHalfY + Half + 0.01f);
		}
		auto Rigid = AddComponent<typename T::FRigid>(Body);
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		m_Rigid = Rigid.Value();
		auto Collider = AddComponent<typename T::FCollider>(T::Box(Half, Half));
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
		}
		if (m_Role != EInteractionCrate::Touch)
		{
			return {};
		}
		auto Listener = AddComponent<typename T::FListener>();
		if (!Listener)
		{
			return TResult<void>::Failure(Listener.Error());
		}
		Listener.Value().Get()->SetHandler(
		    [this](const typename T::FNotice& Notice)
		    {
			    const auto Player = m_pHost->GetPlayerBody();
			    if (Notice.Kind != EWorldEventKind::Contact || !Player || !(Notice.Other.Body == *Player))
			    {
				    return;
			    }
			    if (Notice.Phase == EWorldEventPhase::Stay)
			    {
				    m_pHost->GetRules().CrateStay();
			    }
			    else
			    {
				    m_pHost->GetRules().CrateContact(Notice.Phase == EWorldEventPhase::Begin);
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
	 * 箱の役割。
	 */
	EInteractionCrate m_Role;
	/**
	 * 箱の剛体。
	 */
	TObjectHandle<typename T::FRigid> m_Rigid;
};
} // namespace Dxf::GameplaySample
#endif
