#pragma once
#include "Toolbox/Vector.h"
namespace Dxf
{
struct FFixedTickContext;
/**
 * 物理Stepの直前に行う処理。同じ固定更新のすべての固定更新フック（生成・登録を含む）の後に呼ばれる。
 */
class IPrePhysicsStep
{
public:
	/**
	 * 物理Stepの直前に呼ばれる。例外は固定更新の失敗として呼出し元へ伝わる。
	 * @param Context 同じ固定更新の実行環境。
	 */
	virtual void OnPrePhysicsStep_Internal(const FFixedTickContext& Context) = 0;

protected:
	/**
	 * 予約先からは破棄しない。
	 */
	~IPrePhysicsStep() = default;
};
/**
 * 固定更新1回分の、物理Stepの直前に行う処理の予約。予約は同じ固定更新の中だけで有効で、実行の成否にかかわらず消費する。
 * 予約した順に呼ぶ。所有はせず、予約した対象はその固定更新の間は破棄されない（破棄は境界まで遅れる）ことを前提にする。
 */
class FPrePhysicsStepQueue
{
public:
	/**
	 * この固定更新の物理Stepの直前に行う処理を予約する。
	 * @param Step 予約する処理。
	 */
	void Enqueue(IPrePhysicsStep& Step)
	{
		m_Steps.PushBack(&Step);
	}
	/**
	 * 予約した処理を順に実行し、予約を空にする。例外時も予約を空にしてから伝える。
	 * @param Context 同じ固定更新の実行環境。
	 */
	void Run_Internal(const FFixedTickContext& Context)
	{
		try
		{
			for (Toolbox::size_t Index = 0; Index < m_Steps.Size(); ++Index)
			{
				m_Steps[Index]->OnPrePhysicsStep_Internal(Context);
			}
		}
		catch (...)
		{
			m_Steps.Clear();
			throw;
		}
		m_Steps.Clear();
	}
	/**
	 * 予約を実行せずに空にする（固定更新を打ち切る場合）。
	 */
	FORCEINLINE void Clear_Internal() noexcept
	{
		m_Steps.Clear();
	}
	/**
	 * 予約した処理の数を返す。
	 */
	FORCEINLINE Toolbox::size_t Size() const noexcept
	{
		return m_Steps.Size();
	}

private:
	/**
	 * 予約した処理（所有しない）。
	 */
	Toolbox::TVector<IPrePhysicsStep*> m_Steps;
};
} // namespace Dxf
