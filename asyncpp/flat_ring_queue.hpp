#pragma once

#include <cstddef>
#include <array>
#include <exception>

namespace asyncpp
{
    template<typename _Item, std::size_t _Cap>
    class flat_ring_queue
    {
    public:
        std::size_t size() const {
            return mBack >= mFront ? (mBack - mFront) : (mBack + _Cap - mFront);
        }
        void push_back(const _Item & item) {
            if (size() == _Cap - 1) {
                throw std::out_of_range("exceed capacity");
            }
            mArray[mBack] = item;
            mBack = (mBack + 1) % _Cap;
        }
        template<typename ...Args>
        void emplace_back(Args ...args) {
            if (size() == _Cap - 1) {
                throw std::out_of_range("exceed capacity");
            }
            mArray[mBack] = _Item(args...);
            mBack = (mBack + 1) % _Cap;
        }
        void push_front(const _Item & item) {
            if (size() == _Cap - 1) {
                throw std::out_of_range("exceed capacity");
            }
            mFront = (mFront + _Cap - 1) % _Cap;
            mArray[mFront] = item;
        }
        template<typename ...Args>
        void emplace_front(Args ...args) {
            if (size() == _Cap - 1) {
                throw std::out_of_range("exceed capacity");
            }
            mFront = (mFront + _Cap - 1) % _Cap;
            mArray[mFront] = _Item(args...);
        }
        void pop_back() {
            if (size() == 0) {
                throw std::out_of_range("exceed capacity");
            }
            mBack = (mBack + _Cap - 1) % _Cap;
        }
        void pop_front() {
            if (size() == 0) {
                throw std::out_of_range("exceed capacity");
            }
            mFront = (mFront + 1) % _Cap;
        }
        void clear() {
            for (int32_t i = mFront; i != mBack; i = (i + 1) % _Cap) {
                mArray[i].~_Item();
            }
            mFront = mBack = 0;
        }
        _Item & front() {
            return mArray[mFront];
        }
        _Item & back() {
            return mArray[(mBack + _Cap - 1) % _Cap];
        }
    private:
        int32_t mFront = 0;
        int32_t mBack = 0;
        std::array<_Item, _Cap> mArray;
    };
}