//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

// Helper class for animation and simulation timing.
class StepTimer
{
public:
    typedef void(*LPUPDATEFUNC) (void);

    // Integer format represents time using 10,000,000 ticks per second.
    static const UINT64 TICKS_PER_SECOND = 10000000;

public:
    static double TicksToSeconds(UINT64 ticks) { return (double)ticks / TICKS_PER_SECOND; }
    static UINT64 SecondsToTicks(double seconds) { return (UINT64)(seconds * TICKS_PER_SECOND); }

public:
    StepTimer();
    ~StepTimer() = default;

    // Get elapsed time since the previous Update call.
    inline UINT64 GetElapsedTicks() const { return m_ElapsedTicks; }
    inline double GetElapsedSeconds() const { return TicksToSeconds(m_ElapsedTicks); }

    // Get total time since the start of the program.
    inline UINT64 GetTotalTicks() const { return m_TotalTicks; }
    inline double GetTotalSeconds() const { return TicksToSeconds(m_TotalTicks); }

    // Get total number of updates since start of the program.
    inline UINT32 GetFrameCount() const { return m_FrameCount; }

    // Get the current framerate.
    inline UINT32 GetFramesPerSecond() const { return m_FramesPerSecond; }

    // Set whether to use fixed or variable timestep mode.
    inline void SetFixedTimeStep(bool bIsFixedTimestep) { m_bIsFixedTimeStep = bIsFixedTimestep; }

    // Set how often to call Update when in fixed timestep mode.
    inline void SetTargetElapsedTicks(UINT64 targetElapsed) { m_TargetElapsedTicks = targetElapsed; }
    inline void SetTargetElapsedSeconds(double targetElapsed) { m_TargetElapsedTicks = SecondsToTicks(targetElapsed); }

    // After an intentional timing discontinuity (for instance a blocking IO operation)
    // call this to avoid having the fixed timestep logic attempt a set of catch-up 
    // Update calls.

    void ResetElapsedTime();

    // Update timer state, calling the specified Update function the appropriate number of times.
    void Tick(LPUPDATEFUNC pfnUpdate = nullptr);

private:
    // Source timing data uses QPC units.
    LARGE_INTEGER m_QPCFrequency;
    LARGE_INTEGER m_QPCLastTime;
    UINT64 m_QPCMaxDelta;

    // Derived timing data uses a canonical tick format.
    UINT64 m_ElapsedTicks = 0;
    UINT64 m_TotalTicks = 0;
    UINT64 m_LeftOverTicks = 0;

    // Members for tracking the framerate.
    UINT32 m_FrameCount = 0;
    UINT32 m_FramesPerSecond = 0;
    UINT32 m_FramesThisSecond = 0;
    UINT64 m_QPCSecondCounter = 0;

    // Members for configuring fixed timestep mode.
    bool m_bIsFixedTimeStep = false;
    UINT64 m_TargetElapsedTicks = TICKS_PER_SECOND / 60;
};
