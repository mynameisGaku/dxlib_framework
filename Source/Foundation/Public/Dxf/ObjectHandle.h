#pragma once
#include "Dxf/Object.h"
#include "Toolbox/Utility.h"
#include "Toolbox/Function.h"
#include "Toolbox/SharedPtr.h"
namespace Dxf
{
/**
 * 所有領域・スロット・世代でオブジェクトを識別する。
 */
struct FObjectId
{
	/**
	 * ハンドルの所有領域。
	 */
	Toolbox::uint64 Domain = 0;
	/**
	 * 要素の位置。
	 */
	Toolbox::size_t Index = Toolbox::TNumericLimits<Toolbox::size_t>::Max();
	/**
	 * スロットの世代番号。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 識別情報が等しいかを比較する。
	 */
	bool operator==(const FObjectId&) const = default;
};
namespace Detail
{
/**
 * 所有領域内の識別子をオブジェクトへ解決する。
 */
struct FHandleDomain
{
	/**
	 * 識別子からオブジェクトを解決する処理。
	 */
	Toolbox::TFunction<DObject*(FObjectId)> Resolve;
};
} // namespace Detail
template <typename T> class TSlotMap;
/**
 * 所有領域・スロット・世代を検証する非所有参照。メインスレッドで使用する。
 */
template <typename T> class TObjectHandle
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 */
	TObjectHandle() = default;
	/**
	 * 世代と型を検証し、有効なオブジェクトのポインターを取得する。
	 */
	T* Get() const noexcept
	{
		/**
		 * ハンドルの所有領域。
		 */
		const auto Domain = m_pDomain.Lock();
		/**
		 * オブジェクト。
		 */
		DObject* Object = Domain && Domain->Resolve ? Domain->Resolve(m_Id) : nullptr;
		return Object && Object->IsHandleAccessible_Internal() ? dynamic_cast<T*>(Object) : nullptr;
	}
	/**
	 * 処理または参照が有効かを返す。
	 */
	explicit operator bool() const noexcept
	{
		return Get() != nullptr;
	}
	/**
	 * 識別子を取得する。
	 */
	FObjectId GetId() const noexcept
	{
		return m_Id;
	}
	/**
	 * 指定型として参照できるハンドルを返す。
	 */
	template <typename U> TObjectHandle<U> Cast() const noexcept
	{
		return TObjectHandle<U>(m_pDomain, m_Id);
	}
	/**
	 * 識別情報が等しいかを比較する。
	 * @param Other 操作相手。
	 */
	bool operator==(const TObjectHandle& Other) const noexcept
	{
		return m_Id == Other.m_Id;
	}

private:
	template <typename> friend class TSlotMap;
	template <typename> friend class TObjectHandle;
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Domain ハンドルの所有領域。
	 * @param Id 識別子。
	 */
	TObjectHandle(Toolbox::TWeakPtr<Detail::FHandleDomain> Domain, FObjectId Id)
	    : m_pDomain(Toolbox::Move(Domain)), m_Id(Id)
	{
	}
	/**
	 * ハンドルの所有領域。
	 */
	Toolbox::TWeakPtr<Detail::FHandleDomain> m_pDomain;
	/**
	 * 識別子。
	 */
	FObjectId m_Id;
};
} // namespace Dxf
