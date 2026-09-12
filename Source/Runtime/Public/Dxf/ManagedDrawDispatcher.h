#pragma once
#include "Dxf/LifecycleObject.h"
#include "Dxf/SlotMap.h"
#include "Toolbox/Algorithm.h"
namespace Dxf
{
/**
 * 生成順で描画を通知する。見た目の描画順序はRenderQueue2Dが整える。
 */
template <typename T> class TManagedDrawDispatcher
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Storage オブジェクトの格納先。
	 */
	explicit TManagedDrawDispatcher(TSlotMap<T>& Storage) : m_pStorage(&Storage)
	{
	}
	/**
	 * 対象の描画を要求する。
	 * @param Context 処理に必要な実行環境。
	 */
	TResult<void> Draw_Internal(FRenderContext& Context)
	{
		/**
		 * 所有するオブジェクト群。
		 */
		auto Objects = m_pStorage->Snapshot();
		Toolbox::Sort(Objects.Begin(), Objects.End(),
		              [&](const auto& A, const auto& B)
		              {
			              return m_pStorage->Find_Internal(A.GetId())->GetCreationOrder_Internal() <
			                     m_pStorage->Find_Internal(B.GetId())->GetCreationOrder_Internal();
		              });
		/**
		 * ハンドルを順に処理する。
		 */
		for (auto Handle : Objects)
		{
			/**
			 * オブジェクト。
			 */
			T* Object = m_pStorage->Find_Internal(Handle.GetId());
			if (!Object)
			{
				continue;
			}
			/**
			 * 処理結果。
			 */
			auto Result = Object->Draw_Internal(Context);
			if (!Result)
			{
				return Result;
			}
		}
		return {};
	}

private:
	/**
	 * オブジェクトの格納先。
	 */
	TSlotMap<T>* m_pStorage;
};
} // namespace Dxf
