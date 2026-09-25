// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_BINDING_H
#define DXF_UI_BINDING_H
#include "Toolbox/Function.h"
#include "Toolbox/SharedPtr.h"
#include "Toolbox/Vector.h"
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
	    : m_pSource(Toolbox::Move(Other.m_pSource)), m_Id(Toolbox::Exchange(Other.m_Id, 0))
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
			m_Id = Toolbox::Exchange(Other.m_Id, 0);
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
		const Toolbox::uint64 Id = Toolbox::Exchange(m_Id, 0);
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

/**
 * 変更を通知する値。表示用Modelの項目に使い、表示側は購読して一方向に反映する。
 * 同じ値の代入では通知しない。通知中の購読・解除・値の変更に対応する（変更の通知は現在の通知の後に続けて行う）。
 * スレッド安全ではない。所有スレッドだけで操作する。
 */
template <typename T> class TUiProperty
{
public:
	/**
	 * 初期値で作る。
	 * @param Initial 初期値。
	 */
	explicit TUiProperty(T Initial = T{}) : m_pState(Toolbox::MakeShared<FState>(Toolbox::Move(Initial)))
	{
	}
	TUiProperty(const TUiProperty&) = delete;
	TUiProperty& operator=(const TUiProperty&) = delete;
	/**
	 * 現在の値。
	 */
	FORCEINLINE const T& Get() const noexcept
	{
		return m_pState->Value;
	}
	/**
	 * 値を変え、違えば購読者へ通知する。
	 * @param Value 新しい値。
	 */
	void Set(T Value)
	{
		// 通知中にこの値の所有者が破棄されても、通知を終えるまで状態を生かす。
		const auto State = m_pState;
		if (State->Value == Value)
		{
			return;
		}
		State->Value = Toolbox::Move(Value);
		State->Notify_Internal();
	}
	/**
	 * 変更を購読する。登録時点では呼ばない（現在値は呼出し側がGetで反映する）。
	 * @param Callback 変更後の値を受け取る処理。
	 */
	[[nodiscard]] FUiSubscription Subscribe(Toolbox::TFunction<void(const T&)> Callback)
	{
		if (!Callback)
		{
			return {};
		}
		const Toolbox::uint64 Id = ++m_pState->NextId;
		m_pState->Entries.PushBack(
		    {Id, Toolbox::MakeShared<Toolbox::TFunction<void(const T&)>>(Toolbox::Move(Callback))});
		return FUiSubscription(Toolbox::TWeakPtr<Detail::FUiSubscriptionSource>(m_pState), Id);
	}
	/**
	 * 現在の購読数（解除済みを除く）。
	 */
	Toolbox::size_t GetSubscriberCount() const noexcept
	{
		Toolbox::size_t Count = 0;
		for (const auto& Entry : m_pState->Entries)
		{
			Count += Entry.Callback ? 1 : 0;
		}
		return Count;
	}

private:
	/**
	 * 購読の一件。
	 */
	struct FEntry
	{
		/**
		 * 購読の番号。
		 */
		Toolbox::uint64 Id = 0;
		/**
		 * 通知先。解除すると空にする（通知中の配列を詰めないため）。
		 */
		Toolbox::TSharedPtr<Toolbox::TFunction<void(const T&)>> Callback;
	};
	/**
	 * 値と購読の共有状態。購読トークンは弱参照で指す。
	 */
	struct FState final : Detail::FUiSubscriptionSource
	{
		/**
		 * 初期値で作る。
		 * @param Initial 初期値。
		 */
		explicit FState(T Initial) : Value(Toolbox::Move(Initial))
		{
		}
		/**
		 * 購読の番号の通知先を空にする。
		 * @param Id 購読の番号。
		 */
		void Unsubscribe_Internal(Toolbox::uint64 Id) noexcept override
		{
			for (auto& Entry : Entries)
			{
				if (Entry.Id == Id)
				{
					Entry.Callback = {};
				}
			}
			if (NotifyDepth == 0)
			{
				Compact_Internal();
			}
		}
		/**
		 * 解除済みの項目を取り除く。
		 */
		void Compact_Internal() noexcept
		{
			Toolbox::size_t Write = 0;
			for (Toolbox::size_t Read = 0; Read < Entries.Size(); ++Read)
			{
				if (Entries[Read].Callback)
				{
					if (Write != Read)
					{
						Entries[Write] = Toolbox::Move(Entries[Read]);
					}
					++Write;
				}
			}
			while (Entries.Size() > Write)
			{
				Entries.PopBack();
			}
		}
		/**
		 * 現在の購読者へ通知する。通知中に追加された購読には、この通知を送らない。
		 * 通知中の変更は、この通知を終えた後に続けて通知する。
		 */
		void Notify_Internal()
		{
			if (NotifyDepth > 0)
			{
				bPendingNotify = true;
				return;
			}
			do
			{
				bPendingNotify = false;
				// 通知中の自己破棄（所有者の破棄）でも状態を生かしておく参照は、呼出し側のTUiPropertyが持つ。
				++NotifyDepth;
				const Toolbox::size_t Count = Entries.Size();
				try
				{
					for (Toolbox::size_t Index = 0; Index < Count && Index < Entries.Size(); ++Index)
					{
						// 呼出し中の解除で関数が破棄されないよう、共有して保持する。
						const auto Callback = Entries[Index].Callback;
						if (Callback)
						{
							(*Callback)(Value);
						}
					}
				}
				catch (...)
				{
					--NotifyDepth;
					Compact_Internal();
					throw;
				}
				--NotifyDepth;
				Compact_Internal();
			} while (bPendingNotify);
		}
		/**
		 * 現在の値。
		 */
		T Value;
		/**
		 * 購読の一覧（登録順）。
		 */
		Toolbox::TVector<FEntry> Entries;
		/**
		 * 最後に割り当てた購読の番号。
		 */
		Toolbox::uint64 NextId = 0;
		/**
		 * 通知の入れ子の深さ。
		 */
		Toolbox::int32 NotifyDepth = 0;
		/**
		 * 通知中に値が変わったか。
		 */
		bool bPendingNotify = false;
	};
	/**
	 * 共有状態。
	 */
	Toolbox::TSharedPtr<FState> m_pState;
};

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
		// 後始末中の再登録に備えて、取り出してから呼ぶ。
		auto Cleanups = Toolbox::Move(m_Cleanups);
		m_Cleanups = {};
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
		auto Subscriptions = Toolbox::Move(m_Subscriptions);
		m_Subscriptions = {};
		for (Toolbox::size_t Index = Subscriptions.Size(); Index > 0; --Index)
		{
			Subscriptions[Index - 1].Reset();
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
#endif
