// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SUBSCRIPTION_H
#define DXF_UI_SUBSCRIPTION_H
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
namespace Detail
{
/**
 * 購読を受け付ける発行元の共通窓口。購読の解除だけを型に依らず呼べるようにする。
 */
class FUiSubscriptionSource
{
public:
	virtual ~FUiSubscriptionSource() = default;
	/**
	 * 購読を解除する。通知中でも安全で、以後その購読へは通知しない。
	 * @param Id 購読の番号。
	 */
	virtual void Unsubscribe_Internal(Toolbox::uint64 Id) noexcept = 0;
};
} // namespace Detail
// namespace Detail
/**
 * 購読の所有トークン。破棄・Resetで購読を解除する。発行元が先に破棄されても安全（何もしない）。
 * コピーはできず、移動だけができる。
 */
class FUiSubscription
{
public:
	FUiSubscription() = default;
	/**
	 * 発行元と番号から作る（発行元の内部だけが使う）。
	 * @param Source 発行元の共有状態。
	 * @param Id 購読の番号。
	 */
	FUiSubscription(Toolbox::TWeakPtr<Detail::FUiSubscriptionSource> Source, Toolbox::uint64 Id) noexcept
	    : m_pSource(Toolbox::Move(Source)), m_Id(Id)
	{
	}
	FUiSubscription(const FUiSubscription&) = delete;
	FUiSubscription& operator=(const FUiSubscription&) = delete;
	/**
	 * 所有を移す。移動元は空になる。
	 * @param Other 移動元。
	 */
	FUiSubscription(FUiSubscription&& Other) noexcept
	    : m_pSource(Toolbox::Move(Other.m_pSource)), m_Id(Toolbox::Exchange(Other.m_Id, Toolbox::uint64(0)))
	{
	}
	/**
	 * 現在の購読を解除し、移動元の所有を引き受ける。
	 * @param Other 移動元。
	 */
	FUiSubscription& operator=(FUiSubscription&& Other) noexcept
	{
		if (this != &Other)
		{
			Reset();
			m_pSource = Toolbox::Move(Other.m_pSource);
			m_Id = Toolbox::Exchange(Other.m_Id, Toolbox::uint64(0));
		}
		return *this;
	}
	/**
	 * 購読を解除する。
	 */
	~FUiSubscription()
	{
		Reset();
	}
	/**
	 * 購読を解除して空にする。何度呼んでもよい。
	 */
	void Reset() noexcept
	{
		if (m_Id == 0)
		{
			return;
		}
		// 解除の対象（破棄済みなら空）。
		const auto Source = m_pSource.Lock();
		const Toolbox::uint64 Id = Toolbox::Exchange(m_Id, Toolbox::uint64(0));
		m_pSource = {};
		if (Source)
		{
			Source->Unsubscribe_Internal(Id);
		}
	}
	/**
	 * 購読中か（発行元が破棄済みでも、解除するまではtrue）。
	 */
	FORCEINLINE bool IsActive() const noexcept
	{
		return m_Id != 0;
	}

private:
	/**
	 * 発行元の共有状態。
	 */
	Toolbox::TWeakPtr<Detail::FUiSubscriptionSource> m_pSource;
	/**
	 * 購読の番号（0は空）。
	 */
	Toolbox::uint64 m_Id = 0;
};
} // namespace Dxf
// namespace Dxf
#endif
