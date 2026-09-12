#pragma once
#include "Dxf/Result.h"
#include "Toolbox/Algorithm.h"
#include "Toolbox/Utility.h"
#include "Toolbox/Optional.h"
namespace Dxf
{
/**
 * フレームの経過時間と一時停止状態を保持する。
 */
struct FFrameTime
{
	/**
	 * 時間倍率を反映した経過秒数。
	 */
	Toolbox::f64 DeltaSeconds = 0.0;
	/**
	 * 時間倍率を反映する前の経過秒数。
	 */
	Toolbox::f64 UnscaledDeltaSeconds = 0.0;
	/**
	 * 開始からの累積秒数。
	 */
	Toolbox::f64 ElapsedSeconds = 0.0;
	/**
	 * 更新済みフレームの番号。
	 */
	Toolbox::uint64 FrameIndex = 0;
	/**
	 * 一時停止しているか。
	 */
	bool bPaused = false;
};
/**
 * 単調増加時刻からフレームの経過時間を計算する。
 */
class FFrameClock
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param MaxDeltaSeconds 一度に進める時間の上限秒数。
	 */
	explicit FFrameClock(Toolbox::f64 MaxDeltaSeconds = 0.25) : m_MaxDeltaSeconds(MaxDeltaSeconds)
	{
		if (!Toolbox::IsFinite(MaxDeltaSeconds) || MaxDeltaSeconds <= 0.0)
		{
			m_MaxDeltaSeconds = 0.25;
		}
	}
	/**
	 * 現在時刻からフレームの経過時間を計算する。
	 * @param NowSeconds 単調増加する現在時刻の秒数。
	 */
	TResult<FFrameTime> Sample(Toolbox::f64 NowSeconds)
	{
		if (!Toolbox::IsFinite(NowSeconds) || NowSeconds < 0.0 || (m_Previous && NowSeconds < *m_Previous))
		{
			return TResult<FFrameTime>::Failure(EErrorCode::InvalidArgument,
			                                    "Clock sample must be finite and monotonic");
		}
		/**
		 * 前回からの経過秒数。
		 */
		const Toolbox::f64 Delta = m_Previous ? NowSeconds - *m_Previous : 0.0;
		m_Previous = NowSeconds;
		m_ElapsedSeconds += Delta;
		return TResult<FFrameTime>::Success(
		    {Toolbox::Min(Delta, m_MaxDeltaSeconds), Delta, m_ElapsedSeconds, m_FrameIndex++, false});
	}

private:
	/**
	 * 前回の状態。
	 */
	Toolbox::TOptional<Toolbox::f64> m_Previous;
	/**
	 * 一度に進める時間の上限秒数。
	 */
	Toolbox::f64 m_MaxDeltaSeconds;
	/**
	 * 開始からの累積秒数。
	 */
	Toolbox::f64 m_ElapsedSeconds = 0.0;
	/**
	 * 更新済みフレームの番号。
	 */
	Toolbox::uint64 m_FrameIndex = 0;
};
/**
 * シーンごとの時間倍率と一時停止を制御する。
 */
class FSceneClock
{
public:
	/**
	 * 時間の進行倍率を設定する。
	 * @param Scale 拡大率。
	 */
	bool SetTimeScale(Toolbox::f64 Scale) noexcept
	{
		if (!Toolbox::IsFinite(Scale) || Scale < 0.0 || Scale > 100.0)
		{
			return false;
		}
		m_TimeScale = Scale;
		return true;
	}
	/**
	 * 一時停止しているかを設定する。
	 * @param bPaused 一時停止するか。
	 */
	void SetPaused(bool bPaused) noexcept
	{
		m_bPaused = bPaused;
	}
	/**
	 * 一時停止しているかを調べる。
	 */
	bool IsPaused() const noexcept
	{
		return m_bPaused;
	}
	/**
	 * 一時停止と時間倍率を反映して時計を進める。
	 * @param Frame 現在のフレーム情報。
	 */
	FFrameTime Advance(FFrameTime Frame) noexcept
	{
		Frame.bPaused = m_bPaused;
		Frame.DeltaSeconds = m_bPaused ? 0.0 : Frame.DeltaSeconds * m_TimeScale;
		m_ElapsedSeconds += Frame.DeltaSeconds;
		Frame.ElapsedSeconds = m_ElapsedSeconds;
		return Frame;
	}

private:
	/**
	 * 時間の進行倍率。
	 */
	Toolbox::f64 m_TimeScale = 1.0;
	/**
	 * 開始からの累積秒数。
	 */
	Toolbox::f64 m_ElapsedSeconds = 0.0;
	/**
	 * 一時停止しているか。
	 */
	bool m_bPaused = false;
};
} // namespace Dxf
