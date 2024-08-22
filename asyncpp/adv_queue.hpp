#pragma once

#include <list>
#include <mutex>
#include <functional>
#include <atomic>

#include <asyncpp/common.hpp>
#include <asyncpp/timeout.hpp>
#include <asyncpp/adv_semaphore.hpp>

namespace asyncpp
{
    template<typename _Item, bool _InterProcess = false, typename _Queue=std::list<_Item>>
    class adv_queue
    {
    public:
        adv_queue() = default;
        adv_queue(const adv_queue &) = delete;
        adv_queue & operator = (const adv_queue &) = delete;
    public:
        using mutex_t = asyncpp::mutex<_InterProcess>;
        using lock_t = std::unique_lock<std::mutex>;
    public:
        //manipulating functions:
        result_code enable(uint32_t capacity) {
            if (capacity == 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            lock_t lock(mMutex);
            mQueue.clear();
            mCapacity = capacity;
            mSemC.set_value(mCapacity);
            mSemP.set_value(0);
            mSemC.enable();
            mSemP.enable();
            return result_code::SUCCEED;
        }

        void disable() {
            lock_t lock(mMutex);
            mSemC.disable();
            mSemP.disable();
        }

        //don't clear when queue still in use
        //call this at your own risk
        void clear() {
            lock_t lock(mMutex);
            mQueue.clear();
        }

        uint32_t get_capacity() {
            return mCapacity;
        }

        uint32_t get_size() {
            lock_t lock(mMutex);
            return mQueue.size();
        }

        result_code block_pushing(const timeout & to = timeout()) {
            lock_t lock(mMutex);
            return mSemC.block(nullptr, to);
        }

        result_code try_block_pushing() {
            lock_t lock(mMutex);
            return mSemC.try_block();
        }

        result_code block_popping(const timeout & to = timeout()) {
            lock_t lock(mMutex);
            return mSemP.block(nullptr, to);
        }

        result_code try_block_popping() {
            lock_t lock(mMutex);
            return mSemP.try_block();
        }

        result_code unblock_pushing() {
            lock_t lock(mMutex);
            return mSemC.unblock();
        }

        result_code unblock_popping() {
            lock_t lock(mMutex);
            return mSemP.unblock();
        }

        result_code fill(const timeout & to = timeout()) {
            lock_t lock(mMutex);
            result_code res = result_code::SUCCEED;
            if ((res = mSemC.unblock()) != result_code::SUCCEED) {
                return res;
            }
            if ((res = mSemP.block_and_reserve(mCapacity, nullptr, to)) != result_code::SUCCEED) {
                return res;
            }
            return result_code::SUCCEED;
        }

        result_code drain(const timeout & to = timeout()) {
            lock_t lock(mMutex);
            result_code res = result_code::SUCCEED;
            if ((res = mSemP.unblock()) != result_code::SUCCEED) {
                return res;
            }
            if ((res = mSemC.block_and_reserve(mCapacity, nullptr, to)) != result_code::SUCCEED) {
                return res;
            }
            return result_code::SUCCEED;
        }

        result_code change_capacity(uint32_t capacity, const timeout & to = timeout()) {
            lock_t lock(mMutex);
            if (capacity == mCapacity) {
                return result_code::SUCCEED;
            }
            result_code res = result_code::SUCCEED;
            if (capacity < mCapacity) {
                if ((res = mSemC.block_and_acquire(mCapacity - capacity, nullptr, to)) != result_code::SUCCEED) {
                    return res;
                }
                mCapacity = capacity;
                if ((res = mSemC.unblock()) != result_code::SUCCEED) {
                    return res;
                }
            } else {
                mSemC.release(capacity - mCapacity);
                mCapacity = capacity;
            }
            return result_code::SUCCEED;
        }

        //data functions
        result_code push(const _Item & item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            if ((res = mSemC.acquire([&]{ mQueue.emplace_back(item); }, to)) != result_code::SUCCEED) {
                return res;
            }
            mSemP.release();
            return res;
        }
        result_code push(_Item && item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            if ((res = mSemC.acquire([&]{ mQueue.emplace_back(std::move(item)); }, to)) != result_code::SUCCEED) {
                return res;
            }
            mSemP.release();
            return res;
        }

        result_code try_push(const _Item & item) {
            result_code res = result_code::SUCCEED;
            if ((res = mSemC.try_acquire([&]{ mQueue.emplace_back(item); })) != result_code::SUCCEED) {
                return res;
            }
            mSemP.release();
            return res;
        }
        result_code try_push(_Item && item) {
            result_code res = result_code::SUCCEED;
            if ((res = mSemC.try_acquire([&]{ mQueue.emplace_back(std::move(item)); })) != result_code::SUCCEED) {
                return res;
            }
            mSemP.release();
            return res;
        }

        result_code pop(_Item & item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            if ((res = mSemP.acquire(nullptr, to)) != result_code::SUCCEED) {
                return res;
            }
            auto p = [&] {
                item = std::move(mQueue.front());
                mQueue.pop_front();
            };
            mSemC.release(p);
            return res;
        }
        result_code try_pop(_Item & item) {
            result_code res = result_code::SUCCEED;
            if ((res = mSemP.try_acquire()) != result_code::SUCCEED) {
                return res;
            }
            auto p = [&] {
                item = std::move(mQueue.front());
                mQueue.pop_front();
            };
            mSemC.release(p);
            return res;
        }

    private:
    private:
        mutex_t mMutex;
        std::atomic<uint32_t> mCapacity = 0;
        adv_semaphore<_InterProcess> mSemP;
        adv_semaphore<_InterProcess> mSemC;
        _Queue mQueue;
    };
}
