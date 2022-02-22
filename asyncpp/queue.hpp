#pragma once

#include <list>
#include <mutex>
#include <functional>

#include <asyncpp/common.hpp>
#include <asyncpp/semaphore.hpp>

namespace asyncpp
{
    template<typename IT, typename CT=std::list<IT>>
    class queue
    {
    public:
        queue() = default;
        queue(const queue<IT> &) = delete;
        queue<IT> & operator = (const queue<IT> &) = delete;
    public:
        //manipulating functions:
        result_code enable(uint32_t capacity) {
            if (capacity == 0) {
                return result_code::INVALID_ARGUMENTS;
            }
            std::unique_lock<std::mutex> lock(mMutex);
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                mQueue.clear();
            }
            mCapacity = capacity;
            mSemC.enable(capacity);
            mSemP.enable(0);
            return result_code::SUCCEED;
        }

        void disable() {
            std::unique_lock<std::mutex> lock(mMutex);
            mSemC.disable();
            mSemP.disable();
            mQueue.clear();
        }

        uint32_t get_capacity() {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCapacity;
        }

        result_code block_pushing() {
            std::unique_lock<std::mutex> lock(mMutex);
            result_code res = result_code::SUCCEED;
            if (!_producing_blocked()) {
                res = _block_producing();
            }
            return res;
        }

        result_code block_popping() {
            std::unique_lock<std::mutex> lock(mMutex);
            result_code res = result_code::SUCCEED;
            if (!_consuming_blocked()) {
                res = _block_consuming();
            }
            return res;
        }

        result_code continue_pushing() {
            std::unique_lock<std::mutex> lock(mMutex);
            result_code res = result_code::SUCCEED;
            if (_producing_blocked()) {
                res = _continue_producing();
            }
            return res;
        }

        result_code continue_popping() {
            std::unique_lock<std::mutex> lock(mMutex);
            result_code res = result_code::SUCCEED;
            if (_consuming_blocked()) {
                res = _continue_consuming();
            }
            return res;
        }

        result_code fill() {
            std::unique_lock<std::mutex> lock(mMutex);
            result_code res = result_code::SUCCEED;
            if (!_consuming_blocked()) {
                if ((res = _block_consuming()) != result_code::SUCCEED) {
                    return res;
                }
            }
            if (_producing_blocked()) {
                if ((res = _continue_producing()) != result_code::SUCCEED) {
                    return res;
                }
            }
            if ((res = _reserve_consuming(mCapacity)) != result_code::SUCCEED) {
                return res;
            }
            return result_code::SUCCEED;
        }

        result_code drain() {
            std::unique_lock<std::mutex> lock(mMutex);
            result_code res = result_code::SUCCEED;
            if ((res = _block_producing()) != result_code::SUCCEED) {
                return res;
            }
            if (_consuming_blocked()) {
                if ((res = _continue_consuming()) != result_code::SUCCEED) {
                    return res;
                }
            }
            if ((res = _reserve_producing(mCapacity)) != result_code::SUCCEED) {
                return res;
            }
            return result_code::SUCCEED;
        }

        result_code change_capacity(uint32_t capacity) {
            std::unique_lock<std::mutex> lock(mMutex);
            if (capacity == mCapacity) {
                return result_code::SUCCEED;
            }
            result_code res = result_code::SUCCEED;
            if (capacity < mCapacity) {
                if (!_producing_blocked()) {
                    if ((res = _block_producing()) != result_code::SUCCEED) {
                        return res;
                    }
                }
                if ((res = _acquire_producing(mCapacity - capacity)) != result_code::SUCCEED) {
                    return res;
                }
                mCapacity = capacity;
                if ((res = _continue_producing()) != result_code::SUCCEED) {
                    return res;
                }
            } else {
                _release_producing(capacity - mCapacity);
                mCapacity = capacity;
            }
            return result_code::SUCCEED;
        }

        //data functions
        result_code push(const IT & item) {
            result_code res = result_code::SUCCEED;
            if ((res = _acquire_producing(1)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                mQueue.push_front(item);
            }
            _release_consuming(1);
            return res;
        }

        result_code nonblock_push(const IT & item) {
            result_code res = result_code::SUCCEED;
            if ((res = _nonblock_acquire_producing(1)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                mQueue.push_front(item);
            }
            _release_consuming(1);
            return res;
        }

        template<typename Rep, typename Period>
        result_code timed_push(
            const IT & item, 
            std::chrono::duration<Rep, Period> timeoutDuration) {
            result_code res = result_code::SUCCEED;
            if ((res = _timeout_acquire_producing(1, timeoutDuration)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                mQueue.push_front(item);
            }
            _release_consuming(1);
            return res;
        }

        result_code pop(IT & item) {
            result_code res = result_code::SUCCEED;
            if ((res = _acquire_consuming(1)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                item = std::move(mQueue.back());
                mQueue.pop_back();
            }
            _release_producing(1);
            return res;
        }
        result_code nonblock_pop(IT & item) {
            result_code res = result_code::SUCCEED;
            if ((res = _nonblock_acquire_consuming(1)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                item = std::move(mQueue.back());
                mQueue.pop_back();
            }
            _release_producing(1);
            return res;
        }
        template<typename Rep, typename Period>
        result_code timed_pop(
            IT & item, 
            std::chrono::duration<Rep, Period> timeoutDuration) {
            result_code res = result_code::SUCCEED;
            if ((res = _timed_acquire_consuming(1, timeoutDuration)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                item = std::move(mQueue.back());
                mQueue.pop_back();
            }
            _release_producing(1);
            return res;
        }

        result_code peek(IT & item) {
            result_code res = result_code::SUCCEED;
            if ((res = _acquire_consuming(1)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                item = mQueue.back();
            }
            _release_consuming(1);
            return res;
        }

        result_code nonblock_peek(IT & item) {
            result_code res = result_code::SUCCEED;
            if ((res = _nonblock_acquire_consuming(1)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                item = mQueue.back();
            }
            _release_consuming(1);
            return res;
        }

        template<typename Rep, typename Period>
        result_code timed_peek(
            IT & item, 
            std::chrono::duration<Rep, Period> timeoutDuration) {
            result_code res = result_code::SUCCEED;
            if ((res = _timed_acquire_consuming(1, timeoutDuration)) != result_code::SUCCEED) {
                return res;
            }
            {
                std::unique_lock<std::mutex> lock(mQueueMutex);
                item = mQueue.back();
            }
            _release_consuming(1);
            return res;
        }


        uint32_t get_size() {
            std::unique_lock<std::mutex> lock(mQueueMutex);
            return mQueue.size();
        }

    private:

        inline result_code _block_producing() {
            return mSemC.enter_exclusive_scope();
        }

        inline result_code _block_consuming() {
            return mSemP.enter_exclusive_scope();
        }

        inline result_code _continue_producing() {
            return mSemC.exit_exclusive_scope();
        }

        inline result_code _continue_consuming() {
            return mSemP.exit_exclusive_scope();
        }

        inline result_code _reserve_producing(uint32_t count) {
            return mSemC.reserve(count);
        }

        inline result_code _reserve_consuming(uint32_t count) {
            return mSemP.reserve(count);
        }

        inline result_code _acquire_producing(uint32_t count) {
            return mSemC.acquire(count);
        }
        inline result_code _nonblock_acquire_producing(uint32_t count) {
            return mSemC.nonblock_acquire(count);
        }
        template<typename Rep, typename Period>
        inline result_code _timeout_acquire_producing(
            uint32_t count, 
            std::chrono::duration<Rep, Period> timeoutDuration) {
            return mSemC.timed_acquire(count, timeoutDuration);
        }

        inline result_code _acquire_consuming(uint32_t count) {
            return mSemP.acquire(count);
        }
        inline result_code _nonblock_acquire_consuming(uint32_t count) {
            return mSemP.nonblock_acquire(count);
        }
        template<typename Rep, typename Period>
        inline result_code _timed_acquire_consuming(
            uint32_t count, 
            std::chrono::duration<Rep, Period> timeoutDuration) {
            return mSemP.timed_acquire(count, timeoutDuration);
        }

        inline void _release_producing(uint32_t count) {
            mSemC.release(count);
        }

        inline void _release_consuming(uint32_t count) {
            mSemP.release(count);
        }

        inline bool _producing_blocked() {
            return mSemC.exclusive_accessing();
        }

        inline bool _consuming_blocked() {
            return mSemP.exclusive_accessing();
        }
    private:
        std::mutex mMutex;
        uint32_t mCapacity = 0;
        semaphore<> mSemP;
        semaphore<> mSemC;
        std::mutex mQueueMutex;
        CT mQueue;
    };
}
