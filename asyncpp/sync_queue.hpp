#pragma once

#include <asyncpp/common.hpp>
#include <mutex>
#include <condition_variable>

namespace asyncpp
{
    template<typename IT>
    class sync_queue
    {
    public:
        sync_queue() = default;
        sync_queue(const sync_queue<IT> &) = delete;
        sync_queue<IT> & operator = (const sync_queue<IT> &) = delete;
    public:
        result_code enable() {
            std::unique_lock<std::mutex> lock(mMutex);
            if (mEnabled) {
                return result_code::INCORRECT_STATE;
            }
            mEnabled = true;
            mPushPtr = nullptr;
            return result_code::SUCCEED;
        }
        void disable() {
            std::unique_lock<std::mutex> lock(mMutex);
            mEnabled = false;
            mCond.notify_all();
        }

        result_code push(const IT & item) {
            std::unique_lock<std::mutex> pushLock(mPushMutex);
            {
                std::unique_lock<std::mutex> lock(mMutex);
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
                if (mPushPtr == nullptr) {
                    mPushPtr = &item;
                    mCond.wait(lock);
                    mPushPtr = nullptr;
                    if (!mEnabled) {
                        return result_code::INCORRECT_STATE;
                    }
                } else {
                    *mPopPtr = item;
                    mCond.notify_one();
                }
                return result_code::SUCCEED;
            }
        }

        result_code pop(IT & item) {
            std::unique_lock<std::mutex> popLock(mPopMutex);
            {
                std::unique_lock<std::mutex> lock(mMutex);
                if (!mEnabled) {
                    return result_code::INCORRECT_STATE;
                }
                if (mPopPtr == nullptr) {
                    mPopPtr = &item;
                    mCond.wait(lock);
                    mPopPtr = nullptr;
                    if (!mEnabled) {
                        return result_code::INCORRECT_STATE;
                    }
                } else {
                    item = *mPushPtr;
                    mCond.notify_one();
                }
                return result_code::SUCCEED;
            }
        }
    private:
        bool mEnabled = false;
        std::mutex mPushMutex;
        std::mutex mPopMutex;
        std::mutex mMutex;
        std::condition_variable mCond;
        union {
            IT * mPopPtr;
            const IT * mPushPtr = nullptr;
        };
    };
}