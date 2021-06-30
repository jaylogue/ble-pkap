/*
 *
 *    Copyright (c) Jay Logue
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *   @file
 *         Simple implementation of monotonic and realtime clocks for the
 *         Nordic nRF5 platform.  Based on the app_timer library included
 *         in the nRF5 SDK.
 */

#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include <app_timer.h>

#include <nRF5SysTime.h>

namespace nrf5utils {

namespace {

/**
 * Number of ticks of the RTC counter needed to make it roll over.
 *
 * This is 2^24 on all nRF52 systems.
 */
constexpr uint32_t kRTCIntervalTicks = (RTC_COUNTER_COUNTER_Msk + 1);

/**
 * Half the number of ticks of the RTC counter needed to make it roll over.
 *
 * This is 2^23 on all nRF52 systems.
 */
constexpr uint32_t kRTCHalfIntervalTicks = kRTCIntervalTicks / 2;

/**
 * An app_timer that fires every RTC half-interval.
 */
APP_TIMER_DEF(sRTCHalfIntervalTimer);

/**
 * Number of RTC half-intervals that have elapsed since the system started.
 */
volatile uint32_t sHalfRTCIntervals = 0;

/**
 * The real time (in seconds/ns since the Unix epoch) at the moment the system started.
 *
 * A value of 0,0 indicates that real time is unknown.
 */
struct timespec sRealTimeBase;

/**
 * Called whenever the RTC half-interval timer fires.
 */
void RTCHalfIntervalTimerHandler(void * context)
{
    sHalfRTCIntervals++;
}

/**
 * Adds two normalized timespec values.
 */
void AddTimeSpec(struct timespec & a, const struct timespec & b)
{
    a.tv_sec += b.tv_sec;
    a.tv_nsec += b.tv_nsec;
    if (a.tv_nsec > 1000000000)
    {
        a.tv_sec += 1;
        a.tv_nsec -= 1000000000;
    }
    else if (a.tv_nsec < 0)
    {
        a.tv_sec -= 1;
        a.tv_nsec += 1000000000;
    }
}

} // unnamed namespace


/**
 * Initializes the System Time module.
 *
 * This method must be called after the app_timer module has been initialized.  To ensure the maximum
 * tolerance for interrupt latency, Init() should be called early during the system boot process.
 *
 * Note that, to aid with logging during startup, it is allowed to call any of the GetSystemTime()
 * methods *before* the Init() method has been called method.
 */
ret_code_t SysTime::Init(void)
{
    ret_code_t res;

    res = app_timer_create(&sRTCHalfIntervalTimer, APP_TIMER_MODE_REPEATED, RTCHalfIntervalTimerHandler);
    if (res != NRF_SUCCESS)
        return res;

    res = app_timer_start(sRTCHalfIntervalTimer, kRTCHalfIntervalTicks, NULL);
    return res;
}

/**
 * Stops the System Time module and releases any associated resources.
 */
void SysTime::Shutdown(void)
{
    app_timer_stop(sRTCHalfIntervalTimer);
}

/**
 * Returns the elapsed time in seconds since the system started.
 */
uint32_t SysTime::GetSystemTime(void)
{
    uint64_t totalRTCTicks = GetSystemTime_RTCTicks();
    uint64_t totalLFCLKCycles = totalRTCTicks * (APP_TIMER_CONFIG_RTC_FREQUENCY + 1);
    uint64_t sysTime = totalLFCLKCycles >> 15; // equal to totalLFCLKCycles / 32768
    return static_cast<uint32_t>(sysTime);
}

/**
 * Returns the elapsed time in milliseconds since the system started.
 */
uint64_t SysTime::GetSystemTime_MS(void)
{
    uint64_t totalRTCTicks = GetSystemTime_RTCTicks();
    uint64_t totalLFCLKCycles = totalRTCTicks * (APP_TIMER_CONFIG_RTC_FREQUENCY + 1);
    uint64_t sysTimeMS = (totalLFCLKCycles * 1000) >> 15; // equal to (totalLFCLKCycles * 1000) / 32768
    return sysTimeMS;
}

/**
 * Returns the elapsed time in microeconds since the system started.
 */
uint64_t SysTime::GetSystemTime_US(void)
{
    uint64_t totalRTCTicks = GetSystemTime_RTCTicks();
    uint64_t totalLFCLKCycles = totalRTCTicks * (APP_TIMER_CONFIG_RTC_FREQUENCY + 1);
    uint64_t sysTimeUS = (totalLFCLKCycles * 15625) >> 9; // (totalLFCLKCycles / 32768.0) * 1000000
    return sysTimeUS;
}

/**
 * Returns the elapsed time in nanoeconds since the system started.
 */
uint64_t SysTime::GetSystemTime_NS(void)
{
    uint64_t totalRTCTicks = GetSystemTime_RTCTicks();
    uint64_t totalLFCLKCycles = totalRTCTicks * (APP_TIMER_CONFIG_RTC_FREQUENCY + 1);
    uint64_t sysTimeNS = (totalLFCLKCycles * 1953125) >> 6; // (totalLFCLKCycles / 32768.0) * 1000000000
    return sysTimeNS;
}

/**
 * Returns the elapsed time since the system started in ticks of the RTC counter.
 */
uint64_t SysTime::GetSystemTime_RTCTicks(void)
{
    // Read the number of RTC half-intervals that have occurred since the clock was initialized.
    uint32_t halfRTCIntervals = sHalfRTCIntervals;

    // Read the current value of the RTC counter. (NOTE: This should occur *after* the above code).
    uint32_t relativeRTCTicks = app_timer_cnt_get();

    // If the number of RTC half-intervals is odd (implying that the timer for the second half-interval
    // has fired) adjust the tick count down such that it represents the number ticks since start of the
    // second half-interval.  The subtraction is performed modulo the width of the RTC counter to handle
    // the case where the RTC counter has rolled over to 0.
    if ((halfRTCIntervals & 1) == 1)
    {
        relativeRTCTicks = (relativeRTCTicks - kRTCHalfIntervalTicks) & RTC_COUNTER_COUNTER_Msk;
    }

    // Compute the total elapsed time, in RTC ticks.
    uint64_t sysTimeRTCTicks = static_cast<uint64_t>(halfRTCIntervals) * kRTCHalfIntervalTicks + relativeRTCTicks;

    return sysTimeRTCTicks;
}

/**
 * Returns the elapsed time since the system started in seconds and nanoseconds.
 */
void SysTime::GetSystemTime(struct timespec & sysTime)
{
    uint64_t totalRTCTicks = GetSystemTime_RTCTicks();
    uint64_t totalLFCLKCycles = totalRTCTicks * (APP_TIMER_CONFIG_RTC_FREQUENCY + 1);

    sysTime.tv_sec = static_cast<time_t>(totalLFCLKCycles >> 15); // totalLFCLKCycles / 32768
    sysTime.tv_nsec = static_cast<long int>(((totalLFCLKCycles & 0x7FFF) * 1953125) >> 6); // ((totalLFCLKCycles % 32768) / 32768.0) * 1000000000
}

/**
 * Returns the current time in seconds since the Unix epoch.
 *
 * @param[in]  realTimeSec               Reference to a variable to receive the current time.
 *
 * @retval     NRF_SUCCESS               If the current time is available.
 * @retval     NRF_ERROR_INVALID_STATE   If the current time is not available.
 */
ret_code_t SysTime::GetRealTime(time_t & realTimeSec)
{
    if (!IsRealTimeSet())
        return NRF_ERROR_INVALID_STATE;

    struct timespec realTime;
    GetRealTime(realTime);

    realTimeSec = realTime.tv_sec;

    return NRF_SUCCESS;
}

/**
 * Returns the current time in seconds and nanoseconds since the Unix epoch.
 *
 * @param[in]  realTime                  Reference to a variable to receive the current time.
 *
 * @retval     NRF_SUCCESS               If the current time is available.
 * @retval     NRF_ERROR_INVALID_STATE   If the current time is not available.
 */
ret_code_t SysTime::GetRealTime(struct timespec & realTime)
{
    if (!IsRealTimeSet())
        return NRF_ERROR_INVALID_STATE;

    GetSystemTime(realTime);
    AddTimeSpec(realTime, sRealTimeBase);

    return NRF_SUCCESS;
}

/**
 * Sets the current time.
 *
 * @param[in]  realTime                  Reference to a variable containing the current time
 *                                       in seconds and nanoseconds since the Unix epoch.
 *
 * @retval     NRF_SUCCESS               If the current time was set successfully.
 * @retval     NRF_ERROR_INVALID_PARAM   If the given current time was not valid.
 */
ret_code_t SysTime::SetRealTime(const struct timespec & realTime)
{
    if (realTime.tv_sec < 0 || realTime.tv_nsec < 0 || realTime.tv_nsec >= 1000000000)
        return NRF_ERROR_INVALID_PARAM;

    GetSystemTime(sRealTimeBase);

    sRealTimeBase.tv_sec  = -sRealTimeBase.tv_sec;
    sRealTimeBase.tv_nsec = -sRealTimeBase.tv_nsec;

    AddTimeSpec(sRealTimeBase, realTime);

    return NRF_SUCCESS;
}

/**
 * Unsets the current time.
 *
 * After calling UnsetRealTime(), calls to GetRealTime() will fail with NRF_ERROR_INVALID_STATE.
 */
void SysTime::UnsetRealTime(void)
{
    sRealTimeBase.tv_sec = 0;
    sRealTimeBase.tv_nsec = 0;
}

/**
 * Returns true if the current time is available.
 */
bool SysTime::IsRealTimeSet(void)
{
    return sRealTimeBase.tv_sec != 0 || sRealTimeBase.tv_nsec != 0;
}

} // namespace nrf5utils
