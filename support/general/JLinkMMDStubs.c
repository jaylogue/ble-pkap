/**
 * Copyright (c) 2020 Jay Logue
 * All rights reserved.
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
 *
 */

/**
 *   @file
 *         Stub implementations of J-Link monitor mode debugging (MMD) call-back
 *         functions.  These are needed to satisfy the linker when an application
 *         is built with monitor mode debugging enabled.
 */

#if JLINK_MMD

void JLINK_MONITOR_OnExit(void)
{

}

void JLINK_MONITOR_OnEnter(void)
{

}

void JLINK_MONITOR_OnPoll(void)
{

}

#endif // JLINK_MMD
