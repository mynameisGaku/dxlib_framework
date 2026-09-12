#pragma once
#include "Toolbox/SharedPtr.h"
#include "Toolbox/String.h"
#include "Toolbox/Map.h"
namespace Dxf
{
/**
 * 弱参照によるリソースの再利用を管理する型。
 */
template <typename TResource> class TResourceCache
{
public:
	/**
	 * 条件に一致する登録情報を探す。
	 * @param Key 検索または入力のキー。
	 */
	Toolbox::TSharedPtr<TResource> Find(const Toolbox::FString& Key) const
	{
		// 検索結果のイテレーター。
		const auto It = m_Entries.Find(Key);
		// 共有するリソース。
		auto Resource = It != m_Entries.End() ? It->Second.Lock() : nullptr;
		return Resource && Resource->GetHandle_Internal() >= 0 ? Resource : nullptr;
	}
	/**
	 * 要素を登録する。
	 * @param Key 検索または入力のキー。
	 * @param Resource 共有するリソース。
	 */
	void Insert(Toolbox::FString Key, const Toolbox::TSharedPtr<TResource>& Resource)
	{
		if (m_Entries.Size() >= 128)
		{
			CollectUnused();
		}
		m_Entries[Toolbox::Move(Key)] = Resource;
	}
	/**
	 * 参照されていないキャッシュ項目を除去する。
	 */
	void CollectUnused()
	{
		Toolbox::EraseIf(m_Entries,
		                 [](const auto& Entry)
		                 {
			                 return Entry.Second.IsExpired();
		                 });
	}
	/**
	 * 蓄積した内容を消去する。
	 */
	void Clear() noexcept
	{
		m_Entries.Clear();
	}

private:
	/**
	 * キーと登録情報の対応表。
	 */
	Toolbox::TMap<Toolbox::FString, Toolbox::TWeakPtr<TResource>> m_Entries;
};
} // namespace Dxf
