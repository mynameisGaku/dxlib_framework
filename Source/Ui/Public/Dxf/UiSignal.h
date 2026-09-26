// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_SIGNAL_H
#define DXF_UI_SIGNAL_H
#include "Dxf/UiBinding.h"
namespace Dxf
{
/**
 * 利用者の操作をゲーム側へ返す通知（クリック・値の確定など）。表示の反映（Model→View）では発火しない。
 * 発火中の購読・解除・所有者の破棄に対応する。スレッド安全ではない。
 */
template <typename... TArgs> class TUiSignal
{
public:
	TUiSignal() : m_pState(Toolbox::MakeShared<FState>())
	{
	}
	TUiSignal(const TUiSignal&) = delete;
	TUiSignal& operator=(const TUiSignal&) = delete;
	/**
	 * 通知を購読する。
	 * @param Callback 通知を受け取る処理。
	 */
	[[nodiscard]] FUiSubscription Subscribe(Toolbox::TFunction<void(TArgs...)> Callback)
	{
		if (!Callback)
		{
			return {};
		}
		const Toolbox::uint64 Id = ++m_pState->NextId;
		m_pState->Entries.PushBack(
		    {Id, Toolbox::MakeShared<Toolbox::TFunction<void(TArgs...)>>(Toolbox::Move(Callback))});
		return FUiSubscription(Toolbox::TWeakPtr<Detail::FUiSubscriptionSource>(m_pState), Id);
	}
	/**
	 * 購読者へ通知する。発火中に追加された購読には送らない。
	 * @param Args 通知の引数。
	 */
	void Emit(TArgs... Args)
	{
		// 発火中に所有者が破棄されても、発火を終えるまで状態を生かす。
		const auto State = m_pState;
		++State->EmitDepth;
		const Toolbox::size_t Count = State->Entries.Size();
		try
		{
			for (Toolbox::size_t Index = 0; Index < Count && Index < State->Entries.Size(); ++Index)
			{
				const auto Callback = State->Entries[Index].Callback;
				if (Callback)
				{
					(*Callback)(Args...);
				}
			}
		}
		catch (...)
		{
			--State->EmitDepth;
			State->Compact_Internal();
			throw;
		}
		--State->EmitDepth;
		State->Compact_Internal();
	}
	/**
	 * 現在の購読数。
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
		 * 通知先。解除すると空にする。
		 */
		Toolbox::TSharedPtr<Toolbox::TFunction<void(TArgs...)>> Callback;
	};
	/**
	 * 購読の共有状態。
	 */
	struct FState final : Detail::FUiSubscriptionSource
	{
		/**
		 * 購読の番号の通知先を空にする。
		 * @param Id 購読の番号。
		 */
		void Unsubscribe_Internal(Toolbox::uint64 Id) noexcept override
		{
			// 捕捉の破棄は配列操作の完了後。デストラクタからの再購読でも走査を壊さない。
			Toolbox::TSharedPtr<Toolbox::TFunction<void(TArgs...)>> Retired;
			for (auto& Entry : Entries)
			{
				if (Entry.Id == Id)
				{
					Retired = Toolbox::Move(Entry.Callback);
					break;
				}
			}
			if (EmitDepth == 0)
			{
				Compact_Internal();
			}
		}
		/**
		 * 解除済みの項目を取り除く（発火中は行わない）。
		 */
		void Compact_Internal() noexcept
		{
			if (EmitDepth > 0)
			{
				return;
			}
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
		 * 購読の一覧。
		 */
		Toolbox::TVector<FEntry> Entries;
		/**
		 * 最後に割り当てた購読の番号。
		 */
		Toolbox::uint64 NextId = 0;
		/**
		 * 発火の入れ子の深さ。
		 */
		Toolbox::int32 EmitDepth = 0;
	};
	/**
	 * 共有状態。
	 */
	Toolbox::TSharedPtr<FState> m_pState;
};
} // namespace Dxf
#endif
