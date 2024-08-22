#pragma once

#include <asyncpp/common.hpp>
#include <asyncpp/pthread_wrapper.hpp>
#include <asyncpp/adv_semaphore.hpp>
#include <asyncpp/timeout.hpp>

namespace asyncpp
{
    template<typename _Item, bool _InterProcess = false>
    class sync_queue
    {
    public:
        sync_queue() = default;
        sync_queue(const sync_queue &) = delete;
        sync_queue & operator = (const sync_queue &) = delete;
    public:
        using mutex_t = asyncpp::mutex<_InterProcess>;
        using lock_t = std::unique_lock<std::mutex>;
    public:
        result_code enable() {
            lock_t lock(mMutex);
            mEnabled = true;
            mPushSem.set_value(1);
            mPopSem.set_value(0);
            mPushSem.enable();
            mPopSem.enable();
            return result_code::SUCCEED;
        }
        void disable() {
            lock_t lock(mMutex);
            mEnabled = false;
            mPushSem.disable();
            mPopSem.disable();
        }

        result_code push(const _Item & item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            if ((res = mPushSem.block_and_acquire(1, nullptr, to)) != result_code::SUCCEED) {
                return res;
            }
            {
                mBuf = item;
            }
            if ((res = mPopSem.release(1)) != result_code::SUCCEED) {
                return res;
            }
            if ((res = mPushSem.reserve_and_unblock(1, nullptr, to)) != result_code::SUCCEED) {
                return res;
            }
            return result_code::SUCCEED;
        }
        result_code push(_Item && item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            if ((res = mPushSem.block_and_acquire(1, to)) != result_code::SUCCEED) {
                return res;
            }
            {
                mBuf = std::move(item);
            }
            if ((res = mPopSem.release()) != result_code::SUCCEED) {
                return res;
            }
            if ((res = mPushSem.reserve_and_unblock(1, to)) != result_code::SUCCEED) {
                return res;
            }
            return result_code::SUCCEED;
        }

        result_code pop(_Item & item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            if ((res = mPopSem.acquire(nullptr, to)) != result_code::SUCCEED) {
                return res;
            }
            {
                item = std::move(mBuf);
            }
            if ((res = mPushSem.release()) != result_code::SUCCEED) {
                return res;
            }
            return result_code::SUCCEED;
        }
    private:
        mutex_t mMutex;
        bool mEnabled = false;
        _Item mBuf;
        adv_semaphore<_InterProcess> mPushSem;
        adv_semaphore<_InterProcess> mPopSem;
    };
}