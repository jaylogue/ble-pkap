/**
 * Copyright (c) 2021 Jay Logue
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
 *         Extremely simple type-safe event dispatch implementation.
 */

#include <SimpleEventObserver.h>

namespace SimpleEventObserver {
namespace internal {

void ObserverListBase::Add(ObserverBase & obs)
{
    ObserverBase ** insertPos;
    Remove(obs);
    for (insertPos = &mObservers; 
         *insertPos != nullptr && (*insertPos)->mPriority <= obs.mPriority;
         insertPos = &(*insertPos)->mNext);
    obs.mNext = *insertPos;
    *insertPos = &obs;
}

void ObserverListBase::Remove(ObserverBase & obs)
{
    ObserverBase ** removePos;
    for (removePos = &mObservers; *removePos != nullptr; removePos = &(*removePos)->mNext)
        if (*removePos == &obs)
        {
            *removePos = obs.mNext;
            break;
        }
    obs.mNext = nullptr;
}

} // namespace internal
} // namespace SimpleEventObserver
