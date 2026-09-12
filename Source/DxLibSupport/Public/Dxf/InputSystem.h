#pragma once
#include "Dxf/InputSource.h"
#include "Dxf/InputStateTracker.h"

namespace Dxf
{
/**
 * 入力の取得とフレーム間の状態遷移を管理する型。
 */
class FInputSystem
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Source 入力の取得元。
	 */
	explicit FInputSystem(IInputSource& Source) : m_pSource(&Source)
	{
	}
	/**
	 * 入力または管理対象の状態を更新する。
	 */
	TResult<void> Update()
	{
		/**
		 * デバイスから取得した入力状態。
		 */
		auto Raw = m_pSource->Poll();
		if (!Raw)
		{
			return TResult<void>::Failure(Raw.Error());
		}
		m_Tracker.Advance(Raw.Value());
		return {};
	}
	/**
	 * フレーム内で固定する入力情報を取得する。
	 */
	const FInputSnapshot& GetSnapshot() const noexcept
	{
		return m_Tracker.GetSnapshot();
	}

private:
	/**
	 * 入力の取得元。
	 */
	IInputSource* m_pSource;
	/**
	 * 前回入力との差分を保持する追跡器。
	 */
	FInputStateTracker m_Tracker;
};
} // namespace Dxf
