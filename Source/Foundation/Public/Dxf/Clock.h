#pragma once
#include "Dxf/Result.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
namespace Dxf
{
struct FFrameTime
{
    double DeltaSeconds = 0.0;
    double UnscaledDeltaSeconds = 0.0;
    double ElapsedSeconds = 0.0;
    std::uint64_t FrameIndex = 0;
    bool bPaused = false;
};
class FFrameClock
{
public:
    explicit FFrameClock(double MaxDeltaSeconds = 0.25) : m_MaxDeltaSeconds(MaxDeltaSeconds)
    {
        if (!std::isfinite(MaxDeltaSeconds) || MaxDeltaSeconds <= 0.0) { m_MaxDeltaSeconds = 0.25; }
    }
    TResult<FFrameTime> Sample(double NowSeconds)
    {
        if (!std::isfinite(NowSeconds) || NowSeconds < 0.0 || (m_Previous && NowSeconds < *m_Previous))
        {
            return TResult<FFrameTime>::Failure(EErrorCode::InvalidArgument, "Clock sample must be finite and monotonic");
        }
        const double Delta = m_Previous ? NowSeconds - *m_Previous : 0.0;
        m_Previous = NowSeconds;
        m_ElapsedSeconds += Delta;
        return TResult<FFrameTime>::Success({std::min(Delta, m_MaxDeltaSeconds), Delta, m_ElapsedSeconds, m_FrameIndex++, false});
    }
private:
    std::optional<double> m_Previous;
    double m_MaxDeltaSeconds;
    double m_ElapsedSeconds = 0.0;
    std::uint64_t m_FrameIndex = 0;
};
class FSceneClock
{
public:
    bool SetTimeScale(double Scale) noexcept
    {
        if (!std::isfinite(Scale) || Scale < 0.0 || Scale > 100.0) { return false; }
        m_TimeScale = Scale;
        return true;
    }
    void SetPaused(bool bPaused) noexcept { m_bPaused = bPaused; }
    bool IsPaused() const noexcept { return m_bPaused; }
    FFrameTime Advance(FFrameTime Frame) noexcept
    {
        Frame.bPaused = m_bPaused;
        Frame.DeltaSeconds = m_bPaused ? 0.0 : Frame.DeltaSeconds * m_TimeScale;
        m_ElapsedSeconds += Frame.DeltaSeconds;
        Frame.ElapsedSeconds = m_ElapsedSeconds;
        return Frame;
    }
private:
    double m_TimeScale = 1.0;
    double m_ElapsedSeconds = 0.0;
    bool m_bPaused = false;
};
}
