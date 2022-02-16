#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

#include "common.hpp"

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
        bool enable(VT initialValue) {
            std::unique_lock<std::mutex> lock(mMutex);
            if (mEnabled) {
                return false;
            }
            mValue = initialValue;
            mEnabled = true;
            return true;
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
            std::unique_lock<std::mutex> lock(mMutex);
            do {
                while (mEnabled && !passExclusiveCheck()) {
                    mCond.wait(lock);
                }
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
                while (mEnabled && passExclusiveCheck() && mValue < count) {
                    mCond.wait(lock);
                }
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
            } while (!passExclusiveCheck());
            mValue -= count;
            return result_code::SUCCEED;
        }
        result_code try_acquire(VT count) {
            std::unique_lock<std::mutex> lock(mMutex);
            if (!mEnabled) {
                return result_code::INCORRECT_STATE;
            }
            if (!passExclusiveCheck()) {
                return result_code::INCORRECT_STATE;
            }
            if (mValue < count) {
                return result_code::TRY_FAILED;
            }
            mValue -= count;
            return result_code::SUCCEED;
        }
        result_code reserve(VT count) {
            std::unique_lock<std::mutex> lock(mMutex);
            do {
                while (mEnabled && !passExclusiveCheck()) {
                    mCond.wait(lock);
                }
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
                while (mEnabled && passExclusiveCheck() && mValue < count) {
                    mCond.wait(lock);
                }
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
            } while (!passExclusiveCheck());
            return result_code::SUCCEED;
        }
        result_code try_reserve(VT count) {
            std::unique_lock<std::mutex> lock(mMutex);
            if (!mEnabled) {
                return result_code::INCORRECT_STATE;
            }
            if (!passExclusiveCheck()) {
                return result_code::INCORRECT_STATE;
            }
            if (mValue < count) {
                return result_code::TRY_FAILED;
            }
            return result_code::SUCCEED;
        }
        void release(VT count){
            std::unique_lock<std::mutex> lock(mMutex);
            mValue += count;
            mCond.notify_all();
        }
        VT get() {
            return mValue;
        }
        result_code enter_exclusive_scope() {
            std::unique_lock<std::mutex> lock(mMutex);
            while (mEnabled && !passExclusiveCheck()) {
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
            if (!mEnabled || !passExclusiveCheck()) {
                return result_code::INCORRECT_STATE;
            }
            mExclusiveAccessingThreadID = std::thread::id();
            mCond.notify_all();
            return result_code::SUCCEED;
        }
        bool exclusive_accessing() {
            std::unique_lock<std::mutex> lock(mMutex);
            return mExclusiveAccessingThreadID == std::this_thread::get_id();
        }
    private:
        bool passExclusiveCheck() {
            return mExclusiveAccessingThreadID == std::thread::id() ||
                mExclusiveAccessingThreadID == std::this_thread::get_id();
        }
    private:
        std::mutex mMutex;
        std::condition_variable mCond;
        bool mEnabled = false;
        std::thread::id mExclusiveAccessingThreadID;
        VT mValue = 0;
    };
}