#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

#include "common.hpp"

namespace asyncpp
{
    template<typename T=int>
    class barrier
    {
    public:
        barrier() = default;
        barrier(const barrier &) = delete;
        barrier & operator = (const barrier &) = delete;
    public:
        result_code enable(uint32_t parties, std::function<bool(void)> callback = nullptr) {
            if (parties == 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            mEnabled = true;
            mTotalParties = parties;
            mAwaitingParties = 0;
            mCallback = callback;
            return result_code::SUCCEED;
        }
        void disable() {
            std::unique_lock<std::mutex> lock(mMutex);
            mEnabled = false;
            mCond.notify_all();
        }
        result_code await() {
            std::unique_lock<std::mutex> lock(mMutex);
            if (mAwaitingParties >= mTotalParties) {
                return result_code::INCORRECT_STATE;
            }
            if (!mEnabled) {
                return result_code::INCORRECT_STATE;
            }
            ++mAwaitingParties;
            if (mAwaitingParties == mTotalParties) {
                mCond.notify_all();
                if (mCallback != nullptr && mCallback()) {
                    mAwaitingParties = 0;
                }
            } else {
                mCond.wait(lock);
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
            }
            return result_code::SUCCEED;
        }
    private:
        std::mutex mMutex;
        bool mEnabled = false;
        uint32_t mTotalParties = 0;
        uint32_t mAwaitingParties = 0;
        std::condition_variable mCond;
        std::function<bool(void)> mCallback;
    };
}