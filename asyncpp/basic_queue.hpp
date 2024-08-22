#pragma once
#include <list>
#include <mutex>
#include <functional>
#include <atomic>

#include <asyncpp/common.hpp>
#include <asyncpp/timeout.hpp>
#include <asyncpp/basic_semaphore.hpp>

namespace asyncpp
{

    template<typename _Item, bool _InterProcess = false, typename _Queue=std::list<_Item>>
    class basic_queue
    {
    public:
        basic_queue() = default;
        basic_queue(const basic_queue &) = delete;
        basic_queue & operator = (const basic_queue &) = delete;
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

        //data functions
        result_code push(const _Item & item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            auto on_acquired = [&]() {
                mQueue.emplace_back(item);
            };
            if ((res = mSemC.acquire(to, on_acquired)) != result_code::SUCCEED) {
                return res;
            }
            mSemP.release();
            return res;
        }
        result_code push(_Item && item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            auto on_acquired = [&]() {
                mQueue.emplace_back(std::move(item));
            };
            if ((res = mSemC.acquire(to, on_acquired)) != result_code::SUCCEED) {
                return res;
            }
            mSemP.release();
            return res;
        }

        result_code try_push(const _Item & item) {
            result_code res = result_code::SUCCEED;
            auto on_acquired = [&]() {
                mQueue.emplace_back(item);
            };
            if ((res = mSemC.try_acquire(on_acquired)) != result_code::SUCCEED) {
                return res;
            }
            mSemP.release();
            return res;
        }
        result_code try_push(_Item && item) {
            result_code res = result_code::SUCCEED;
            auto on_acquired = [&]() {
                mQueue.emplace_back(std::move(item));
            };
            if ((res = mSemC.try_acquire(on_acquired)) != result_code::SUCCEED) {
                return res;
            }
            mSemP.release();
            return res;
        }

        result_code pop(_Item & item, const timeout & to = timeout()) {
            result_code res = result_code::SUCCEED;
            if ((res = mSemP.acquire(to)) != result_code::SUCCEED) {
                return res;
            }
            mSemC.release([&]() {
                item = std::move(mQueue.front());
                mQueue.pop_front();
            });
            return res;
        }
        result_code try_pop(_Item & item) {
            result_code res = result_code::SUCCEED;
            if ((res = mSemP.try_acquire()) != result_code::SUCCEED) {
                return res;
            }
            mSemC.release([&]() {
                item = std::move(mQueue.front());
                mQueue.pop_front();
            });
            return res;
        }
    private:
        mutex_t mMutex;
        std::atomic<uint32_t> mCapacity = 0;
        basic_semaphore<_InterProcess> mSemP;
        basic_semaphore<_InterProcess> mSemC;
        _Queue mQueue;
    };
}