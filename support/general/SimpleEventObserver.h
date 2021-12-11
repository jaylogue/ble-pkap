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
 *         Extremely simple type-safe event observer/dispatch implementation.
 */

#ifndef SIMPLEEVENTOBSERVER_H
#define SIMPLEEVENTOBSERVER_H

/** Use function pointers for event handler functions
 * 
 * Compile-time option to force the use of simple function pointers for storing
 * observer handler functions. This reduces the memory cost of observers, but
 * limits handler functions to being global/static functions or lambdas without
 * captures.
 * 
 * If not set, observer handler functions are stored in std::function<> objects.
 */
#ifndef SIMPLE_EVENT_OBSERVER_FUNCTION_POINTER_ONLY
#define SIMPLE_EVENT_OBSERVER_FUNCTION_POINTER_ONLY 0
#endif

#include <stdint.h>

#if !SIMPLE_EVENT_OBSERVER_FUNCTION_POINTER_ONLY
#include <functional>
#endif

namespace SimpleEventObserver {

template<typename... EventArgs> class Event;

// Private implementation types... 
namespace internal {

class ObserverBase;

class ObserverListBase
{
    friend class ObserverBase;

protected:
    ObserverBase * mObservers;

    void Add(ObserverBase & obs);
    void Remove(ObserverBase & obs);
};

class ObserverBase
{
    friend class ObserverListBase;
    template<typename... EventArgs> friend class SimpleEventObserver::Event;

public:
    inline void Connect(void)
    {
        mObserverList->Add(*this);
    }

    inline void Disconnect(void)
    {
        mObserverList->Remove(*this);
    }

protected:
    ObserverListBase * mObserverList;
    ObserverBase * mNext;
    int mPriority;

    inline ObserverBase(ObserverListBase * observerList, int priority)
    : mObserverList(observerList), mNext(nullptr), mPriority(priority)
    {
        Connect();
    }

    inline ~ObserverBase(void)
    {
        Disconnect();
    }
};

} // namespace internal

/** Simple, type-safe event observer/dispatcher
 *
 * # Declaring an Event
 *
 * To make an event type available for observation, instantiate an Event<...> object with
 * template parameters matching the arguments to the observer's event handler function.
 * This object maintains the list of registered observers for the associated event and
 * serves as a point for obsevers to register their interest.  The Event object can be
 * declared at global scope, or within a class, as appropriate for the use case.
 *
 *     class MyEventGenerator
 *     {
 *     public:
 *         SimpleEventObserver::Event<int, const char *> MyEvent;
 *     }
 *
 * # Observing an Event
 * 
 * To receive callbacks when an event occurs, use the SIMPLE_EVENT_OBSERVER() macro
 * to register an observer handler function.  
 * 
 *     void myEventHandlerFunct(int aValue, const char * aMessage)
 *     {
 *         ... handle event ...
 *     }
 *
 *     SIMPLE_EVENT_OBSERVER(myEventObserver, MyEventGenerator::MyEvent,
 *                           0, // observer priority
 *                           myEventHandlerFunct);
 * 
 * This constructs an instance of the Event<>::Observer class, which holds the pointer to
 * the observer's handler function. This object can be instantiated at global scope,
 * or within a function or class. Events will continue to be delivered to the handler
 * function until the Observer object is destroyed, or the Observer::Disconnect() method
 * is called.
 * 
 * The handler argument can also be a lambda expression:
 * 
 *     SIMPLE_EVENT_OBSERVER(myEventObserver, MyEventGenerator::MyEvent, 0,
 *         [](int aValue, const char * aMessage) {
 *             ... handle event ...
 *         }
 *     );
 * 
 * The lamdba expression can have capture arguments, although this requires the 
 * SIMPLE_EVENT_OBSERVER_FUNCTION_POINTER_ONLY compile-time option be set to 0.
 * 
 * Note that the use of capture arguments in a handler lambda expression can result
 * in a call to dynamically allocate memory when the observer is instantiated. This
 * behavior depends on the overall size of the captured values and varies based on
 * the C++ runtime implementation. Generally speaking, a small number of reference
 * capture arguments (<=2) should never result in the allocation of memory. 
 * 
 * # Raising an Event
 * 
 * When an event occurs, the originating code can deliver the event to the registered
 * observers by calling the Event<...>::RaiseEvent() method.
 * 
 *     {
 *         ...
 *         MyEvent.RaiseEvent(42, "Something interesting happened");
 *         ...
 *     }
 * 
 * The Event<...>::HasObservers() method can be used to check for the presense or absense
 * of observers prior to raising an event.
 * 
 * # Observer Priorities
 * 
 * The third parameter to the SIMPLE_EVENT_OBSERVER() macro is an integer priority value.
 * When an event is raised, the registered handlers are invoked in priority order starting
 * with the those with the lowest numerical priority value. Observers with the same priority
 * are called in order they were registered, which can be arbitrary for globally declared
 * observer objects.
 * 
 * # Cautions
 * 
 * NB: To preserve the simplicity of the code, this implementation is intentionally *not*
 * thread-safe.
 */
template<typename... EventArgs>
struct Event : public internal::ObserverListBase
{
#if SIMPLE_EVENT_OBSERVER_FUNCTION_POINTER_ONLY
    using HandlerFunct = void (*)(EventArgs... eventArgs);
#else
    using HandlerFunct = std::function<void (EventArgs...)>;
#endif

    struct Observer : public internal::ObserverBase
    {
        friend struct Event;

    public:
        Observer(Event & event, HandlerFunct handlerFunct, int priority = 0)
        : internal::ObserverBase(&event, priority), mHandlerFunct(handlerFunct)
        {
        }

    private:
        HandlerFunct mHandlerFunct;
    };

    void RaiseEvent(EventArgs... eventArgs)
    {
        for (auto obs = mObservers; obs; obs = obs->mNext)
        {
            ((const Observer *)obs)->mHandlerFunct(eventArgs...);
        }
    }

    inline bool HasObservers(void)
    {
        return mObservers != nullptr;
    }
};

} // namespace SimpleEventObserver

/** Convenience macro for declaring event observers
 */
#define SIMPLE_EVENT_OBSERVER(NAME, EVENT, PRIORITY, HANDLER) \
decltype(EVENT)::Observer NAME = decltype(EVENT)::Observer(EVENT, HANDLER, PRIORITY)

#endif // SIMPLEEVENTOBSERVER_H
