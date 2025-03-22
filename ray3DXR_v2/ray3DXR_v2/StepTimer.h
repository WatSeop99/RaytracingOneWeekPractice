#pragma once

class StepTimer
{
public:
    typedef void(*LPUPDATEFUNC) (void);

    static const UINT64 TICKS_PER_SECOND = 10000000;

public:
    static inline double TicksToSeconds(UINT64 ticks) { return (double)ticks / TICKS_PER_SECOND; }
    static inline UINT64 SecondsToTicks(double seconds) { return (UINT64)(seconds * TICKS_PER_SECOND); }

public:
    StepTimer();
    ~StepTimer() = default;

    inline UINT64 GetElapsedTicks() const { return m_ElapsedTicks; }
    inline double GetElapsedSeconds() const { return TicksToSeconds(m_ElapsedTicks); }

    inline UINT64 GetTotalTicks() const { return m_TotalTicks; }
    inline double GetTotalSeconds() const { return TicksToSeconds(m_TotalTicks); }

    inline UINT32 GetFrameCount() const { return m_FrameCount; }

    inline UINT32 GetFramesPerSecond() const { return m_FramesPerSecond; }

    inline void SetFixedTimeStep(bool bIsFixedTimestep) { m_bIsFixedTimeStep = bIsFixedTimestep; }

    inline void SetTargetElapsedTicks(UINT64 targetElapsed) { m_TargetElapsedTicks = targetElapsed; }
    inline void SetTargetElapsedSeconds(double targetElapsed) { m_TargetElapsedTicks = SecondsToTicks(targetElapsed); }

    void ResetElapsedTime();

    void Tick(LPUPDATEFUNC pfnUpdate = nullptr);

private:
    LARGE_INTEGER m_QPCFrequency;
    LARGE_INTEGER m_QPCLastTime;
    UINT64 m_QPCMaxDelta;

    UINT64 m_ElapsedTicks = 0;
    UINT64 m_TotalTicks = 0;
    UINT64 m_LeftOverTicks = 0;

    UINT32 m_FrameCount = 0;
    UINT32 m_FramesPerSecond = 0;
    UINT32 m_FramesThisSecond = 0;
    UINT64 m_QPCSecondCounter = 0;

    bool m_bIsFixedTimeStep = false;
    UINT64 m_TargetElapsedTicks = TICKS_PER_SECOND / 60;
};
