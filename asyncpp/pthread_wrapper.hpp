#pragma once

#include <pthread.h>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <type_traits>

using namespace std::chrono;

namespace asyncpp
{
    template<bool _InterProcess = false>
    class mutex : public std::mutex {
    public:
        mutex() noexcept {
            if constexpr (_InterProcess) {
                pthread_mutexattr_t attr;
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_setpshared(
                    &attr, 
                    PTHREAD_PROCESS_SHARED
                );
                pthread_mutex_init(native_handle(), &attr);
                pthread_mutexattr_destroy(&attr);
            }
        }
        mutex(const mutex &) = delete;
        mutex & operator=(const mutex &) = delete;
        /*operator std::mutex & () const {
            return static_cast<std::mutex &>(*this);
        }*/
    };

    template<bool _InterProcess = false>
    class condition_variable : public std::condition_variable {
    public:
        condition_variable() noexcept {
            if constexpr (_InterProcess) {
                pthread_condattr_t attr;
                pthread_condattr_init(&attr);
                pthread_condattr_setpshared(
                    &attr, 
                    PTHREAD_PROCESS_SHARED
                );
                pthread_cond_init(native_handle(), &attr);
                pthread_condattr_destroy(&attr);
            }
        }
        condition_variable(const condition_variable &) = delete;
        condition_variable & operator=(const condition_variable &) = delete;
    };

    class thread
    {
    private:
        template<typename _Callable, typename ..._Args>
        struct CallWrapper
        {
            template<size_t ..._Indices>
            struct Caller
            {
                static void call(const _Callable & callable, const std::tuple<_Args ...> & args) {
                    callable(std::get<_Indices>(args)...);
                }
            };

            const _Callable callable;
            const std::tuple<_Args...> args;
            CallWrapper(_Callable && _callable, _Args && ..._args) :
                callable(_callable), args(std::make_tuple<_Args...>(_args...))
            {
                
            }

            void call() {
                Range<Caller, 0, sizeof...(_Args)>::call(callable, args);
            }

        };
        
    public:
        thread() = default;
        template<typename _Callable, typename ..._Args>
        thread(_Callable && callable, _Args && ...args)
        {
            auto * cw = new CallWrapper<_Callable, _Args...>(callable, args...);
            pthread_create(&pthread, NULL, proc, cw);
        }
    private:
        template<typename _Callable, typename ..._Args>
        static void * proc(void * wrapper) {
            auto * cw = reinterpret_cast<CallWrapper<_Callable, _Args...>*>(wrapper);
            cw->call();
            delete cw;
            return NULL;
        }
    private:
        pthread_t pthread;
    };

    struct this_thread
    {
        static bool make_fifo(int prio) {
            sched_param param = { prio };
            int r = pthread_setschedparam(
                pthread_self(), 
                SCHED_FIFO, 
                &param
            );
            if (r != 0) {
                printf("pthread_setschedparam failed with %d\n", r);
            }
            return r == 0;
        }
        static bool set_prio(int prio) {
            return pthread_setschedprio(pthread_self(), prio) == 0;
        }
    };
}