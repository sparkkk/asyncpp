#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

#include <asyncpp/common.hpp>

namespace asyncpp
{
    template<typename VT=uint32_t>
    class semaphore
    {
    public:
        semaphore() = default;
        semaphore(const semaphore &) = delete;
        semaphore<VT> & operator = (const semaphore<VT> &) = delete;
    public:
        result_code enable(VT initialValue) {
            if (initialValue < 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            if (mEnabled) {
                return result_code::INCORRECT_STATE;
            }
            mValue = initialValue;
            mEnabled = true;
            return result_code::SUCCEED;
        }
        void re_enable() {
            std::unique_lock<std::mutex> lock(mMutex);
            mEnabled = true;
        }
        void disable() {
            std::unique_lock<std::mutex> lock(mMutex);
            if (!mEnabled) {
                return;
            }
            mEnabled = false;
            mExclusiveAccessingThreadID = std::thread::id();
            mCond.notify_all();
        }
        result_code acquire(VT count) {
            if (count < 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            result_code res = result_code::SUCCEED;
            if ((res = _reserve(lock, count)) == result_code::SUCCEED) {
                mValue -= count;
            }
            return res;
        }
        result_code reserve(VT count) {
            if (count < 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            return _reserve(lock, count);
        }
        result_code nonblock_acquire(VT count) {
            if (count < 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            result_code res = result_code::SUCCEED;
            if ((res = _nonblock_reserve(lock, count)) == result_code::SUCCEED) {
                mValue -= count;
            }
            return res;
        }
        result_code nonblock_reserve(VT count) {
            if (count < 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            return _nonblock_reserve(lock, count);
        }
        template <typename Rep, typename Period>
        result_code timed_acquire(VT count, std::chrono::duration<Rep, Period> timeoutDuration) {
            if (count < 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            auto timeoutTime = std::chrono::steady_clock::now() + timeoutDuration;
            result_code res = result_code::SUCCEED;
            if ((res = _timed_reserve(lock, count, timeoutTime)) == result_code::SUCCEED) {
                mValue -= count;
            }
            return res;
        }
        template <typename Rep, typename Period>
        result_code timed_reserve(VT count, std::chrono::duration<Rep, Period> timeoutDuration) {
            if (count < 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            auto timeoutTime = std::chrono::steady_clock::now() + timeoutDuration;
            return _timed_reserve(lock, count, timeoutTime);
        }
        result_code release(VT count){
            if (count < 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            mValue += count;
            mCond.notify_all();
            return result_code::SUCCEED;
        }
        VT get() {
            std::unique_lock<std::mutex> lock(mMutex);
            return mValue;
        }
        result_code enter_exclusive_scope() {
            std::unique_lock<std::mutex> lock(mMutex);
            while (mEnabled && !_pass_exclusive_check()) {
                mCond.wait(lock);
            }
            if (!mEnabled) {
                return result_code::INCORRECT_STATE;
            }
            mExclusiveAccessingThreadID = std::this_thread::get_id();
            mCond.notify_all();
            return result_code::SUCCEED;
        }
        result_code exit_exclusive_scope() {
            std::unique_lock<std::mutex> lock(mMutex);
            if (!mEnabled || !_pass_exclusive_check()) {
                return result_code::INCORRECT_STATE;
            }
            mExclusiveAccessingThreadID = std::thread::id();
            mCond.notify_all();
            return result_code::SUCCEED;
        }
        bool exclusive_accessing() {
            std::unique_lock<std::mutex> lock(mMutex);
            return mEnabled && mExclusiveAccessingThreadID == std::this_thread::get_id();
        }
    private:
        bool _pass_exclusive_check() {
            return mExclusiveAccessingThreadID == std::thread::id() ||
                mExclusiveAccessingThreadID == std::this_thread::get_id();
        }
        result_code _reserve(std::unique_lock<std::mutex> & lock, VT count) {
            do {
                while (mEnabled && !_pass_exclusive_check()) {
                    mCond.wait(lock);
                }
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
                while (mEnabled && _pass_exclusive_check() && mValue < count) {
                    mCond.wait(lock);
                }
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
            } while (!_pass_exclusive_check());
            return result_code::SUCCEED;
        }
        result_code _nonblock_reserve(std::unique_lock<std::mutex> & lock, VT count) {
            if (!mEnabled) {
                return result_code::INCORRECT_STATE;
            }
            if (!_pass_exclusive_check()) {
                return result_code::INCORRECT_STATE;
            }
            if (mValue < count) {
                return result_code::TRY_FAILED;
            }
            return result_code::SUCCEED;
        }
        result_code _timed_reserve(
            std::unique_lock<std::mutex> & lock, 
            VT count, 
            std::chrono::steady_clock::time_point timeoutTime) {
            do {
                while (mEnabled && !_pass_exclusive_check()) {
                    if (mCond.wait_until(lock, timeoutTime) == std::cv_status::timeout) {
                        return result_code::TIME_OUT;
                    }
                }
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
                while (mEnabled && _pass_exclusive_check() && mValue < count) {
                    if (mCond.wait_until(lock, timeoutTime) == std::cv_status::timeout) {
                        return result_code::TIME_OUT;
                    }
                }
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
            } while (!_pass_exclusive_check());
            return result_code::SUCCEED;
        }
    private:
        std::mutex mMutex;
        std::condition_variable mCond;
        bool mEnabled = false;
        std::thread::id mExclusiveAccessingThreadID;
        VT mValue = 0;
    };
}