#include "framework.h"
#include "StepTimer.h"

StepTimer::StepTimer()
{
    QueryPerformanceFrequency(&m_QPCFrequency);
    QueryPerformanceCounter(&m_QPCLastTime);

    // Initialize max delta to 1/10 of a second.
    m_QPCMaxDelta = m_QPCFrequency.QuadPart / 10;
}

void StepTimer::ResetElapsedTime()
{
    QueryPerformanceCounter(&m_QPCLastTime);

    m_LeftOverTicks = 0;
    m_FramesPerSecond = 0;
    m_FramesThisSecond = 0;
    m_QPCSecondCounter = 0;
}

void StepTimer::Tick(LPUPDATEFUNC pfnUpdate)
{
    LARGE_INTEGER currentTime;

    QueryPerformanceCounter(&currentTime);

    UINT64 timeDelta = currentTime.QuadPart - m_QPCLastTime.QuadPart;

    m_QPCLastTime = currentTime;
    m_QPCSecondCounter += timeDelta;

    if (timeDelta > m_QPCMaxDelta)
    {
        timeDelta = m_QPCMaxDelta;
    }

    timeDelta *= TICKS_PER_SECOND;
    timeDelta /= m_QPCFrequency.QuadPart;

    UINT32 lastFrameCount = m_FrameCount;

    if (m_bIsFixedTimeStep)
    {
        if (abs((int)(timeDelta - m_TargetElapsedTicks)) < TICKS_PER_SECOND / 4000)
        {
            timeDelta = m_TargetElapsedTicks;
        }

        m_LeftOverTicks += timeDelta;

        while (m_LeftOverTicks >= m_TargetElapsedTicks)
        {
            m_ElapsedTicks = m_TargetElapsedTicks;
            m_TotalTicks += m_TargetElapsedTicks;
            m_LeftOverTicks -= m_TargetElapsedTicks;
            m_FrameCount++;

            if (pfnUpdate)
            {
                pfnUpdate();
            }
        }
    }
    else
    {
        m_ElapsedTicks = timeDelta;
        m_TotalTicks += timeDelta;
        m_LeftOverTicks = 0;
        m_FrameCount++;

        if (pfnUpdate)
        {
            pfnUpdate();
        }
    }

    if (m_FrameCount != lastFrameCount)
    {
        ++m_FramesThisSecond;
    }

    if (m_QPCSecondCounter >= (UINT64)m_QPCFrequency.QuadPart)
    {
        m_FramesPerSecond = m_FramesThisSecond;
        m_FramesThisSecond = 0;
        m_QPCSecondCounter %= m_QPCFrequency.QuadPart;
    }
}
