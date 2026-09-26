// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_PROPERTY_H
#define DXF_UI_PROPERTY_H
#include "Dxf/UiSubscription.h"
#include "Toolbox/Function.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
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
			// 捕捉の破棄は配列操作の完了後。デストラクタからの再購読でも走査を壊さない。
			Toolbox::TSharedPtr<Toolbox::TFunction<void(const T&)>> Retired;
			for (auto& Entry : Entries)
			{
				if (Entry.Id == Id)
				{
					Retired = Toolbox::Move(Entry.Callback);
					break;
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
					// 再入によるSetは次の通知波で反映し、この波の値は固定する。
					const T Published = Value;
					for (Toolbox::size_t Index = 0; Index < Count && Index < Entries.Size(); ++Index)
					{
						// 呼出し中の解除で関数が破棄されないよう、共有して保持する。
						const auto Callback = Entries[Index].Callback;
						if (Callback)
						{
							(*Callback)(Published);
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
} // namespace Dxf
// namespace Dxf
#endif
