#pragma once

#include <chrono>
#include <optional>

namespace asyncpp
{
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    template<typename Rep, typename Period>
    using duration = std::chrono::duration<Rep, Period>;

    class timeout
    {
    public:
        timeout() = default;
        timeout(const timeout &) = default;
        timeout(timeout &&) = default;
        ~timeout() = default;
        timeout & operator=(const timeout &) = default;
        timeout & operator=(timeout &&) = default;

        timeout(const time_point & until) {
            mTimePointOpt.emplace(until);
        }

        template<typename Rep, typename Period>
        timeout(const duration<Rep, Period> & duration) {
            auto until = clock::now() + duration;
            mTimePointOpt.emplace(until);
        }

        constexpr bool has_value() const {
            return mTimePointOpt.has_value();
        }

        constexpr const clock::time_point & value() const {
            return mTimePointOpt.value();
        }
    private:
        std::optional<clock::time_point> mTimePointOpt;
    };
}