/*
 *
 *    Copyright (c) 2021 Jay Logue
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
 *         Simple implementation of clock and real-time clocks for the
 *         Nordic nRF5 platform, based on the nRF5 RTC module and app_timer
 *         library.
 */


#ifndef NRF5CLOCK_H
#define NRF5CLOCK_H

namespace nrf5utils {

/**
 * Implements system and real-time clocks for the Nordic nRF5 platform.
 *
 * The SysTime module implements two clocks: a system clock and a real-time clock. The system
 * clock is a monotonic clock that counts time since system boot (specifically, since the RTC
 * counter is started).  The real-time clock is derived from the system clock, but counts time
 * since the Unix epoch (January 1, 1970 00:00:00 UTC).  For the real-time clock to be available,
 * current time must be set from an outside source (this module does not provide a time
 * synchronization protocol).  Both clocks rely on the Nordic RTC hardware module and the app_timer
 * SDK library.
 *
 * The values returned by the GetSystemTime() and GetRealTime() methods are guaranteed to
 * be monotonic so long as the system's RTC hardware is not reset.  The clocks will continue
 * to return accurate time (within the limits of the hardware clock source) even if RTC
 * interrupts are disabled, provided that the total time interrupts are disabled is less than
 * half the cycle period of the RTC counter.  (At the fasted possible tick rate, this allows
 * interrupts to be disabled for up to 256 seconds).
 *
 * The SysTime module relies on the Nordic RTC hardware to have been initialized prior to its
 * use.  The module consumes a single app_timer instance, but does not directly access or
 * reconfigure the RTC hardware, nor consume any of the RTC compare registers.  The SysTime
 * module is compatible with the Nordic SoftDevice.
 */
class SysTime final
{
public:
    static ret_code_t Init(void);
    static void Shutdown(void);

    static uint32_t GetSystemTime(void);
    static uint64_t GetSystemTime_MS(void);
    static uint32_t GetSystemTime_MS32(void);
    static uint64_t GetSystemTime_US(void);
    static uint64_t GetSystemTime_NS(void);
    static uint64_t GetSystemTime_RTCTicks();
    static void GetSystemTime(struct timespec & sysTime);

    static ret_code_t GetRealTime(time_t & timeUS);
    static ret_code_t GetRealTime(struct timespec & time);

    static ret_code_t SetRealTime(const struct timespec & realTime);
    static void UnsetRealTime(void);

    static bool IsRealTimeSet(void);
};

/**
 * Returns the elapsed time in milliseconds as a 32-bit integer.
 *
 * Note that this value wraps after 49.7 days.
 */
inline uint32_t SysTime::GetSystemTime_MS32(void)
{
    return static_cast<uint32_t>(GetSystemTime_MS());
}

} // namespace nrf5utils


#endif // NRF5CLOCK_H
