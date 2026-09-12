#pragma once
#include "Toolbox/UniquePtr.h"
#include "Dxf/ObjectHandle.h"
#include "Toolbox/Atomic.h"
#include "Toolbox/Utility.h"
#include "Toolbox/Vector.h"
namespace Dxf
{
/**
 * 重複しないハンドル所有領域の番号を割り当てる。
 */
FORCEINLINE Toolbox::uint64 AllocateDomain_Internal()
{
	// 次に割り当てる番号。
	static Toolbox::FAtomicCounter Next{1};
	// 処理対象の値。
	const auto Value = Next.FetchAdd(1);
	if (Value == 0)
	{
		throw Toolbox::FException("Handle domain exhausted");
	}
	return Value;
}
/**
 * インスタンスの所有だけを担当し、ゲーム処理のコールバックは呼び出さない。
 */
template <typename T> class TSlotMap
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	TSlotMap() : m_Domain(AllocateDomain_Internal()), m_pDomain(Toolbox::MakeShared<Detail::FHandleDomain>())
	{
		m_pDomain->Resolve = [this](FObjectId Id) -> DObject*
		{
			return Find_Internal(Id);
		};
	}
	/**
	 * 所有する状態を終了し、必要なリソースを解放する。
	 */
	~TSlotMap()
	{
		m_pDomain->Resolve = {};
	}
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	TSlotMap(const TSlotMap&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	TSlotMap& operator=(const TSlotMap&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	TSlotMap(TSlotMap&&) = delete;
	/**
	 * 意図しない所有権の移動や複製を禁止する。
	 */
	TSlotMap& operator=(TSlotMap&&) = delete;
	/**
	 * 要素を登録する。
	 * @param Object オブジェクト。
	 */
	TObjectHandle<T> Insert(Toolbox::TUniquePtr<T> Object)
	{
		if (!Object)
		{
			return {};
		}
		// 要素の位置。
		Toolbox::size_t Index = 0;
		for (; Index < m_Slots.Size(); ++Index)
		{
			if (!m_Slots[Index].Object && m_Slots[Index].Generation != Toolbox::TNumericLimits<Toolbox::uint64>::Max())
			{
				break;
			}
		}
		if (Index == m_Slots.Size())
		{
			m_Slots.EmplaceBack();
		}
		// オブジェクトの格納スロット。
		auto& Slot = m_Slots[Index];
		Slot.Object = Toolbox::Move(Object);
		++m_Size;
		return TObjectHandle<T>(m_pDomain, {m_Domain, Index, Slot.Generation});
	}
	/**
	 * 対象を登録先から取り外す。
	 * @param Handle ハンドル。
	 */
	template <typename U> bool Remove(const TObjectHandle<U>& Handle) noexcept
	{
		// 識別子。
		const auto Id = Handle.GetId();
		if (!Find_Internal(Id))
		{
			return false;
		}
		// 利用者のデストラクターより先に削除を反映する。再登録や配列の拡張に備え、スロット参照を保持しない。
		//
		// 取り外したオブジェクトの所有権。
		auto Removed = Toolbox::Move(m_Slots[Id.Index].Object);
		++m_Slots[Id.Index].Generation;
		--m_Size;
		return true;
	}
	/**
	 * 条件に一致する登録情報を探す。
	 * @param Id 識別子。
	 */
	T* Find_Internal(FObjectId Id) const noexcept
	{
		if (Id.Domain != m_Domain || Id.Index >= m_Slots.Size())
		{
			return nullptr;
		}
		// オブジェクトの格納スロット。
		const auto& Slot = m_Slots[Id.Index];
		return Id.Generation == Slot.Generation ? Slot.Object.Get() : nullptr;
	}
	/**
	 * 現在の対象一覧を独立した値として取得する。
	 */
	Toolbox::TVector<TObjectHandle<T>> Snapshot() const
	{
		// 処理結果。
		Toolbox::TVector<TObjectHandle<T>> Result;
		Result.Reserve(m_Size);
		// 要素の位置を進めて順に処理する。
		for (Toolbox::size_t Index = 0; Index < m_Slots.Size(); ++Index)
		{
			// オブジェクトの格納スロット。
			const auto& Slot = m_Slots[Index];
			if (Slot.Object)
			{
				Result.PushBack(TObjectHandle<T>(m_pDomain, {m_Domain, Index, Slot.Generation}));
			}
		}
		return Result;
	}
	/**
	 * 条件に一致したオブジェクトを取り外す。
	 * @param Predicate 要素を選別する条件。
	 */
	template <typename TPredicate> void RemoveIf_Internal(TPredicate Predicate) noexcept
	{
		// 要素数。
		const auto Count = m_Slots.Size();
		// 要素の位置を進めて順に処理する。
		for (Toolbox::size_t Index = 0; Index < Count; ++Index)
		{
			// オブジェクト。
			T* Object = m_Slots[Index].Object.Get();
			// スロットの世代番号。
			const auto Generation = m_Slots[Index].Generation;
			if (Object && Predicate(*Object))
			{
				Remove(TObjectHandle<T>(m_pDomain, {m_Domain, Index, Generation}));
			}
		}
	}
	/**
	 * 所有する各オブジェクトへ処理を適用する。
	 * @param Function 各要素に適用する処理。
	 */
	template <typename TFunction> void ForEach_Internal(TFunction Function)
	{
		// オブジェクトの格納スロットを順に処理する。
		for (auto& Slot : m_Slots)
		{
			if (Slot.Object)
			{
				Function(*Slot.Object);
			}
		}
	}
	/**
	 * 指定型の最初の有効なオブジェクトを探す。
	 */
	template <typename U> TObjectHandle<U> FindFirst() const noexcept
	{
		static_assert(Toolbox::IsBaseOf<T, U>);
		// 要素の位置を進めて順に処理する。
		for (Toolbox::size_t Index = 0; Index < m_Slots.Size(); ++Index)
		{
			// オブジェクトの格納スロット。
			const auto& Slot = m_Slots[Index];
			if (Slot.Object && Slot.Object->IsHandleAccessible_Internal() && dynamic_cast<U*>(Slot.Object.Get()))
			{
				return TObjectHandle<U>(m_pDomain, {m_Domain, Index, Slot.Generation});
			}
		}
		return {};
	}
	/**
	 * 指定型の有効なオブジェクトをすべて集める。
	 */
	template <typename U> Toolbox::TVector<TObjectHandle<U>> FindAll() const
	{
		static_assert(Toolbox::IsBaseOf<T, U>);
		// 処理結果。
		Toolbox::TVector<TObjectHandle<U>> Result;
		// ハンドルを順に処理する。
		for (auto Handle : Snapshot())
		{
			// 型を確認したハンドル。
			auto Typed = Handle.template Cast<U>();
			if (Typed)
			{
				Result.PushBack(Typed);
			}
		}
		return Result;
	}
	/**
	 * 有効な要素数を取得する。
	 */
	FORCEINLINE Toolbox::size_t Size() const noexcept
	{
		return m_Size;
	}

private:
	/**
	 * オブジェクトの所有権と世代を保持する。
	 */
	struct FSlot
	{
		/**
		 * オブジェクト。
		 */
		Toolbox::TUniquePtr<T> Object;
		/**
		 * スロットの世代番号。
		 */
		Toolbox::uint64 Generation = 1;
	};
	/**
	 * 所有するスロット一覧。
	 */
	Toolbox::TVector<FSlot> m_Slots;
	/**
	 * 有効な要素数。
	 */
	Toolbox::size_t m_Size = 0;
	/**
	 * ハンドルの所有領域。
	 */
	Toolbox::uint64 m_Domain;
	/**
	 * ハンドルの所有領域。
	 */
	Toolbox::TSharedPtr<Detail::FHandleDomain> m_pDomain;
};
} // namespace Dxf
