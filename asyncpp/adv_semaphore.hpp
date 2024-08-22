#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>
#include <type_traits>

#include "asyncpp/common.hpp"
#include "asyncpp/timeout.hpp"
#include "asyncpp/pthread_wrapper.hpp"

namespace asyncpp
{
    template<
        bool _InterProcess = false,
        typename _Counter = uint32_t, 
        typename = typename std::enable_if<std::is_unsigned<_Counter>::value>::type>
    class adv_semaphore
    {
    public:
        adv_semaphore() = default;
        adv_semaphore(const adv_semaphore &) = delete;
        adv_semaphore & operator = (const adv_semaphore &) = delete;
    public:
        using mutex_t = asyncpp::mutex<_InterProcess>;
        using cond_t = asyncpp::condition_variable<_InterProcess>;
        using lock_t = std::unique_lock<std::mutex>;
        using proc_t = std::function<void()>;
        enum opflag {
            NONE = 0x00,
            PREV_BLOCK = 0x01,
            POST_UNBLOCK = 0x02,
            RESERVE = 0x04,
            ACQUIRE = 0x08,
            RELEASE = 0x10,
        };
    public:
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
            mBlockerID = std::thread::id();
            mCondBlock.notify_all();
            mCond.notify_all();
            return result_code::SUCCEED;
        }

        result_code do_operations(
                opflag flags, 
                _Counter count,
                const proc_t & proc = nullptr, 
                const timeout & to = timeout()) {
            return _do_operations(flags, count, proc, to);
        }

        result_code try_oprations(
                opflag flags, 
                _Counter count,
                const proc_t & proc = nullptr) {
            return _try_operations(flags, count, proc);
        }

        result_code acquire(const proc_t & proc = nullptr, const timeout & to = timeout()) {
            return _do_operations(opflag::ACQUIRE, 1, proc, to);
        }
        result_code reserve(const proc_t & proc = nullptr, const timeout & to = timeout()) {
            return _do_operations(opflag::RESERVE, 1, proc, to);
        }
        result_code try_acquire(const proc_t & proc = nullptr) {
            return _try_operations(opflag::ACQUIRE, 1, proc);
        }
        result_code try_reserve(const proc_t & proc = nullptr) {
            return _try_operations(opflag::RESERVE, 1, proc);
        }

        result_code block(const proc_t & proc = nullptr, const timeout & to = timeout()) {
            return _do_operations(opflag::PREV_BLOCK, 0, proc, to);
        }
        result_code try_block(const proc_t & proc = nullptr) {
            return _try_operations(opflag::PREV_BLOCK, 0, proc);
        }

        result_code unblock(const proc_t & proc = nullptr) {
            return _do_operations(opflag::POST_UNBLOCK, 0, proc, timeout());
        }

        result_code release(const proc_t & proc = nullptr) {
            return _do_operations(opflag::RELEASE, 1, proc, timeout());
        }

        result_code release(_Counter count, const proc_t & proc = nullptr) {
            return _do_operations(opflag::RELEASE, count, proc, timeout());
        }

        result_code block_and_acquire(
                _Counter count, 
                const proc_t & proc = nullptr,
                const timeout & to = timeout()) {
            return _do_operations((opflag)(opflag::ACQUIRE | opflag::PREV_BLOCK), count, proc, to);
        }
        result_code block_and_reserve(
                _Counter count, 
                const proc_t & proc = nullptr,
                const timeout & to = timeout()) {
            return _do_operations((opflag)(opflag::RESERVE | opflag::PREV_BLOCK), count, proc, to);
        }
        result_code reserve_and_unblock(
                _Counter count, 
                const proc_t & proc = nullptr,
                const timeout & to = timeout()) {
            return _do_operations((opflag)(opflag::RESERVE | opflag::POST_UNBLOCK), count, proc, to);
        }

    private:
        inline bool _has_block_flag(opflag f) {
            return (f & opflag::PREV_BLOCK) != 0;
        }
        inline bool _has_unblock_flag(opflag f) {
            return (f & opflag::POST_UNBLOCK) != 0;
        }
        inline bool _has_acquire_flag(opflag f) {
            return (f & opflag::ACQUIRE) != 0;
        }
        inline bool _has_reserve_flag(opflag f) {
            return (f & opflag::RESERVE) != 0;
        }
        inline bool _has_acquire_or_reserve_flag(opflag f) {
            return (f & (opflag::ACQUIRE | opflag::RESERVE)) != 0;
        }
        inline bool _has_release_flag(opflag f) {
            return (f & opflag::RELEASE) != 0;
        }

        inline bool _blocked() {
            return mBlockerID != std::thread::id();
        }

        inline bool _blocked_by_this() {
            return mBlockerID == std::this_thread::get_id();
        }

        inline bool _blocked_by_others() {
            return _blocked() && !_blocked_by_this();
        }

        result_code _wait(lock_t & lock, cond_t & cond, const timeout & to) {
            if (to.has_value()) {
                if (cond.wait_until(lock, to.value()) == std::cv_status::timeout) {
                    return result_code::UNAVAILABLE_OR_TIMEOUT;
                } 
            } else {
                cond.wait(lock);
            }
            return result_code::SUCCEED;
        }

        result_code _wait_block(lock_t & lock, const timeout & to) {
            result_code res = result_code::SUCCEED;
            while (_blocked_by_others()) {
                res = _wait(lock, mCondBlock, to);
                if (res != result_code::SUCCEED) {
                    return res;
                }
                if (!mEnabled) {
                    return result_code::DISABLED;
                }
            }
            return result_code::SUCCEED;
        }

        result_code _wait_value(lock_t & lock, _Counter value, const timeout & to) {
            if (_blocked_by_others()) {
                return result_code::BLOCKED;
            }
            result_code res = result_code::SUCCEED;
            while (mValue < value) {
                res = _wait(lock, mCond, to);
                if (res != result_code::SUCCEED) {
                    return res;
                }
                if (!mEnabled) {
                    return result_code::DISABLED;
                }
                if (_blocked_by_others()) {
                    return result_code::BLOCKED;
                }
            }
            return result_code::SUCCEED;
        }
        
        result_code _do_operations(
                opflag flag,
                _Counter count, 
                const proc_t & proc, 
                const timeout & to) {
            lock_t lock(mMutex);
            result_code res = result_code::SUCCEED;
            if (!mEnabled) {
                return result_code::DISABLED;
            }
            if (_has_acquire_or_reserve_flag(flag)) {
                if (count == 0) {
                    return result_code::INVALID_ARGUMENTS;
                }
                if (count > 1 && !_has_block_flag(flag) && !_blocked_by_this()) {
                    return result_code::INVALID_ARGUMENTS;
                }
            }
            if (_has_release_flag(flag)) {
                if (count == 0) {
                    return result_code::INVALID_ARGUMENTS;
                }
            }
            if (_has_block_flag(flag) || _has_acquire_or_reserve_flag(flag)) {
                res = _wait_block(lock, to);
                if (res != result_code::SUCCEED) {
                    return res;
                }
            }
            if (_has_block_flag(flag)) {
                if (!_blocked_by_this()) {
                    mBlockerID = std::this_thread::get_id();
                    mCond.notify_all();
                }
            }
            if (_has_acquire_or_reserve_flag(flag)) {

                while ((res = _wait_value(lock, count, to)) == result_code::BLOCKED) {
                    res = _wait_block(lock, to);
                    if (res != result_code::SUCCEED) {
                        return res;
                    }
                }
                if (res != result_code::SUCCEED) {
                    return res;
                }
                if (_has_acquire_flag(flag)) {
                    mValue -= count;
                }
            }
            if (proc != nullptr) {
                proc();
            }
            if (_has_release_flag(flag)) {
                mValue += count;
                mCond.notify_all();
            }
            if (_has_unblock_flag(flag)) {
                if (_blocked()) {
                    mBlockerID = std::thread::id();
                    mCondBlock.notify_all();
                }
            }
            return result_code::SUCCEED;
        }
        result_code _try_operations(
                opflag flag, 
                _Counter count, 
                const proc_t & proc) {
            lock_t lock(mMutex);
            result_code res = result_code::SUCCEED;
            if (!mEnabled) {
                return result_code::DISABLED;
            }
            if (_has_acquire_or_reserve_flag(flag)) {
                if (count == 0) {
                    return result_code::INVALID_ARGUMENTS;
                }
            }
            if (_has_release_flag(flag)) {
                if (count == 0) {
                    return result_code::INVALID_ARGUMENTS;
                }
            }
            if (_has_block_flag(flag) || _has_acquire_or_reserve_flag(flag)) {
                if (_blocked_by_others()) {
                    return result_code::BLOCKED;
                }
            }
            if (_has_block_flag(flag)) {
                if (!_blocked_by_this()) {
                    mBlockerID = std::this_thread::get_id();
                    mCond.notify_all();
                }
            }
            if (_has_acquire_or_reserve_flag(flag)) {
                if (mValue < count) {
                    return result_code::UNAVAILABLE_OR_TIMEOUT;
                }
            }
            if (_has_acquire_flag(flag)) {
                mValue -= count;
            }
            if (proc != nullptr) {
                proc();
            }
            if (_has_release_flag(flag)) {
                mValue += count;
                mCond.notify_all();
            }
            if (_has_unblock_flag(flag)) {
                if (_blocked()) {
                    mBlockerID = std::thread::id();
                    mCondBlock.notify_all();
                }
            }
            return result_code::SUCCEED;
        }
    private:
        mutable mutex_t mMutex;
        mutable cond_t mCond;
        mutable cond_t mCondBlock;
        bool mEnabled = false;
        std::thread::id mBlockerID;
        _Counter mValue = 0;
    };
}