#pragma once

#include <chrono>
#include <type_traits>
#include <functional>

#include "asyncpp/common.hpp"
#include "asyncpp/timeout.hpp"
#include "asyncpp/pthread_wrapper.hpp"

namespace asyncpp
{
    template<
        bool _InterProcess = false,
        typename _Counter = uint32_t, 
        typename = typename std::enable_if<std::is_unsigned<_Counter>::value>::type>
    class basic_semaphore
    {
    public:
        basic_semaphore() = default;
        basic_semaphore(const basic_semaphore &) = delete;
        basic_semaphore & operator = (const basic_semaphore &) = delete;
    public:
        using mutex_t = asyncpp::mutex<_InterProcess>;
        using cond_t = asyncpp::condition_variable<_InterProcess>;
        using lock_t = std::unique_lock<std::mutex>;
        using proc_t = std::function<void()>;
        result_code set_value(_Counter value) {
            lock_t lock(mMutex);
            if (mEnabled) {
                return result_code::INCORRECT_STATE;
            }
            mValue = value;
            return result_code::SUCCEED;
        }
        _Counter get_value() const {
            lock_t lock(mMutex);
            return mValue;
        }
        result_code enable() {
            lock_t lock(mMutex);
            mEnabled = true;
            return result_code::SUCCEED;
        }

        result_code disable() {
            lock_t lock(mMutex);
            if (!mEnabled) {
                return result_code::SUCCEED;
            }
            mEnabled = false;
            mCond.notify_all();
            return result_code::SUCCEED;
        }

        result_code acquire(
                const timeout & to = timeout(), 
                const proc_t & on_acquired = nullptr) {
            lock_t lock(mMutex);
            if (!mEnabled) {
                return result_code::DISABLED;
            }
            while (mValue == 0) {
                if (to.has_value()) {
                    if (mCond.wait_until(lock, to.value()) == std::cv_status::timeout) {
                        return result_code::UNAVAILABLE_OR_TIMEOUT;
                    }
                } else {
                    mCond.wait(lock);
                }
                if (!mEnabled) {
                    return result_code::DISABLED;
                }
            }
            --mValue;
            if (on_acquired != nullptr) {
                on_acquired();
            }
            return result_code::SUCCEED;
        }

        result_code try_acquire(const proc_t & on_acquired = nullptr) {
            lock_t lock(mMutex);
            if (!mEnabled) {
                return result_code::DISABLED;
            }
            if (mValue == 0) {
                return result_code::UNAVAILABLE_OR_TIMEOUT;
            }
            --mValue;
            if (on_acquired != nullptr) {
                on_acquired();
            }
            return result_code::SUCCEED;
        }

        result_code release(const proc_t & on_releasing = nullptr) {
            lock_t lock(mMutex);
            if (!mEnabled) {
                return result_code::DISABLED;
            }
            if (on_releasing != nullptr) {
                on_releasing();
            }
            ++mValue;
            //notify_all is always faster than notify_one
            mCond.notify_all();
            return result_code::SUCCEED;
        }
    private:
        mutable mutex_t mMutex;
        mutable cond_t mCond;
        bool mEnabled = false;
        _Counter mValue = 0;
    };
}