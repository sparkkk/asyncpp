#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

#include "common.hpp"

namespace asyncpp
{
    template<typename _Counter = uint32_t, bool _InterProcess = false>
    class barrier
    {
    public:
        barrier() = default;
        barrier(const barrier &) = delete;
        barrier & operator = (const barrier &) = delete;
    private:
        using mutex_t = asyncpp::mutex<_InterProcess>;
        using cond_t = asyncpp::condition_variable<_InterProcess>;
        using lock_t = std::unique_lock<std::mutex>;
    public:
        result_code enable(_Counter total) {
            if (total == 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            lock_t lock(mMutex);
            mTotal = total;
            mValue = 0;
            mEnabled = true;
            return result_code::SUCCEED;
        }
        void disable() {
            lock_t lock(mMutex);
            mEnabled = false;
            mCond.notify_all();
        }
        result_code await(const timeout & to = timeout()) {
            lock_t lock(mMutex);
            if (!mEnabled) {
                return result_code::DISABLED;
            }
            result_code res = result_code::SUCCEED;
            ++mValue;
            if (mValue == mTotal) {
                mCond.notify_all();
                mValue = 0;
            } else {
                if (to.has_value()) {
                    if (mCond.wait_until(lock, to.value()) == std::cv_status::timeout) {
                        res = result_code::UNAVAILABLE_OR_TIMEOUT;
                    }
                } else {
                    mCond.wait(lock);
                }
                if (!mEnabled) {
                    res = result_code::DISABLED;
                }
            }
            return res;
        }
    private:
        mutex_t mMutex;
        cond_t mCond;
        bool mEnabled = false;
        _Counter mTotal = 0;
        _Counter mValue = 0;
    };
}