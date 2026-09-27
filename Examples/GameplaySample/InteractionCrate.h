// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_GAMEPLAY_SAMPLE_INTERACTION_CRATE_H
#define DXF_GAMEPLAY_SAMPLE_INTERACTION_CRATE_H
#include "InteractionTraits.h"
namespace Dxf::GameplaySample
{
/**
 * 触れる箱（Dynamic）。自分の剛体の接触（Solid）を監視し、プレイヤーとのBegin／Endを規則へ伝える。
 * 箱と床の接触点が複数あっても、プレイヤーとの組は一つの組としてBeginが一度になる。
 * @tparam T 次元の型。
 */
template <typename T> class TInteractionCrate final : public DGameObject
{
public:
	/**
	 * @param Host シーンの窓口（シーンはオブジェクトより長く生存する）。
	 */
	explicit TInteractionCrate(IInteractionHost<T>& Host) : m_pHost(&Host)
	{
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
		Body.Position = T::At(InteractionLayout::CrateX, InteractionLayout::CrateHalf);
		auto Rigid = AddComponent<typename T::FRigid>(Body);
		if (!Rigid)
		{
			return TResult<void>::Failure(Rigid.Error());
		}
		m_Rigid = Rigid.Value();
		auto Collider =
		    AddComponent<typename T::FCollider>(T::Box(InteractionLayout::CrateHalf, InteractionLayout::CrateHalf));
		if (!Collider)
		{
			return TResult<void>::Failure(Collider.Error());
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
	 * 箱の剛体。
	 */
	TObjectHandle<typename T::FRigid> m_Rigid;
};
} // namespace Dxf::GameplaySample
#endif
