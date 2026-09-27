#pragma once
#include "Toolbox/Vector.h"
namespace Dxf
{
struct FFixedTickContext;
/**
 * 物理Stepの直後に行う処理。同じ固定更新の物理Stepが成功した後、次の固定更新の前に呼ばれる。
 * World.Stepの中（Solver・索引の更新中）からは呼ばれない。
 */
class IPostPhysicsStep
{
public:
	/**
	 * 物理Stepの直後に呼ばれる。例外は固定更新の失敗として呼出し元へ伝わり、同じ固定更新の残りの予約は実行しない。
	 * @param Context 同じ固定更新の実行環境。
	 */
	virtual void OnPostPhysicsStep_Internal(const FFixedTickContext& Context) = 0;
	/**
	 * 呼ぶ直前に、配送してよい状態か（破棄を要求していないか）を返す。falseなら呼ばない。
	 */
	virtual bool IsPostPhysicsStepAlive_Internal() const noexcept = 0;

protected:
	/**
	 * 予約先からは破棄しない。
	 */
	~IPostPhysicsStep() = default;
};
/**
 * 固定更新1回分の、物理Stepの直後に行う処理の予約。予約は同じ固定更新の中だけで有効で、実行の成否にかかわらず消費する。
 * 予約した順に呼ぶ。所有はせず、破棄は境界まで遅れる（この固定更新の間は解放されない）ことを前提にする。
 * 先に呼んだ処理が後の対象の破棄を要求した場合、その対象は呼ばない（呼ぶ直前に状態を確かめる）。
 */
class FPostPhysicsStepQueue
{
public:
	/**
	 * この固定更新の物理Stepの直後に行う処理を予約する。
	 * @param Step 予約する処理。
	 */
	void Enqueue(IPostPhysicsStep& Step)
	{
		m_Steps.PushBack(&Step);
	}
	/**
	 * 予約した処理を順に実行し、予約を空にする。例外時も予約を空にしてから伝える。
	 * 実行中の予約の追加は、次の固定更新の予約ではなく、この実行の後ろへ加わる（同じ固定更新の中で呼ぶ）。
	 * @param Context 同じ固定更新の実行環境。
	 */
	void Run_Internal(const FFixedTickContext& Context)
	{
		try
		{
			for (Toolbox::size_t Index = 0; Index < m_Steps.Size(); ++Index)
			{
				IPostPhysicsStep* Step = m_Steps[Index];
				if (Step->IsPostPhysicsStepAlive_Internal())
				{
					Step->OnPostPhysicsStep_Internal(Context);
				}
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
	Toolbox::TVector<IPostPhysicsStep*> m_Steps;
};
} // namespace Dxf
