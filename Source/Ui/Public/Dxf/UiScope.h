// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SCOPE_H
#define DXF_UI_SCOPE_H
#include "Dxf/UiSubscription.h"
#include "Toolbox/Function.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 接続期間に結び付けた購読と後始末の集まり。Clearで登録の逆順に解放する。
 */
class FUiScope
{
public:
	FUiScope() = default;
	FUiScope(const FUiScope&) = delete;
	FUiScope& operator=(const FUiScope&) = delete;
	/**
	 * 保持しているものを解放する。
	 */
	~FUiScope()
	{
		Clear();
	}
	/**
	 * 購読を所有する。
	 * @param Subscription 購読トークン。
	 */
	void Add(FUiSubscription Subscription)
	{
		if (Subscription.IsActive())
		{
			m_Subscriptions.PushBack(Toolbox::Move(Subscription));
		}
	}
	/**
	 * 解放時に呼ぶ後始末を登録する。
	 * @param Cleanup 後始末（例外を投げないこと）。
	 */
	void AddCleanup(Toolbox::TFunction<void()> Cleanup)
	{
		if (Cleanup)
		{
			m_Cleanups.PushBack(Toolbox::Move(Cleanup));
		}
	}
	/**
	 * 後始末を登録の逆順に呼び、購読を解除する。
	 */
	void Clear() noexcept
	{
		// 利用者コードより先に所有物を取り出す。後始末がこのScopeを破棄してもthisへ戻らない。
		auto Subscriptions = Toolbox::Move(m_Subscriptions);
		auto Cleanups = Toolbox::Move(m_Cleanups);
		for (Toolbox::size_t Index = Subscriptions.Size(); Index > 0; --Index)
		{
			Subscriptions[Index - 1].Reset();
		}
		for (Toolbox::size_t Index = Cleanups.Size(); Index > 0; --Index)
		{
			try
			{
				Cleanups[Index - 1]();
			}
			catch (...)
			{
			}
		}
	}
	/**
	 * 保持している購読の数。
	 */
	FORCEINLINE Toolbox::size_t GetSubscriptionCount() const noexcept
	{
		return m_Subscriptions.Size();
	}
	/**
	 * 何も保持していないか。
	 */
	FORCEINLINE bool IsEmpty() const noexcept
	{
		return m_Subscriptions.IsEmpty() && m_Cleanups.IsEmpty();
	}

private:
	/**
	 * 購読。
	 */
	Toolbox::TVector<FUiSubscription> m_Subscriptions;
	/**
	 * 後始末。
	 */
	Toolbox::TVector<Toolbox::TFunction<void()>> m_Cleanups;
};
} // namespace Dxf
// namespace Dxf
#endif
