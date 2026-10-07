#pragma once

#include "util/log/Logger.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace util {

/** @brief Opt-in, non-payload publication diagnostics for isolated qualification. */
class PublicationTiming {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::optional<Clock::time_point>;

    /** @brief Capture a start time only when CLIO_PUBLICATION_TIMINGS=1. */
    static TimePoint
    startPoint()
    {
        static bool const enabled = [] {
            auto const* value = std::getenv("CLIO_PUBLICATION_TIMINGS");
            return value != nullptr && std::string_view{value} == "1";
        }();
        return enabled ? TimePoint{Clock::now()} : std::nullopt;
    }

    /** @brief Create a timer; an absent start disables both clocks and logging. */
    PublicationTiming(
        util::Logger& log,
        std::uint32_t ledger,
        std::string_view operation,
        TimePoint start = startPoint()
    )
        : log_(log)
        , ledger_(ledger)
        , operation_(operation)
        , start_(start)
        , previous_(start.value_or(Clock::time_point{}))
    {
    }

    /** @brief Whether this timer records diagnostics. */
    [[nodiscard]] bool
    enabled() const
    {
        return start_.has_value();
    }

    /** @brief Record a static stage name and durations; never record provider data. */
    void
    mark(std::string_view stage)
    {
        if (!start_)
            return;
        auto const now = Clock::now();
        auto const elapsed =
            std::chrono::duration_cast<std::chrono::microseconds>(now - previous_).count();
        auto const total =
            std::chrono::duration_cast<std::chrono::microseconds>(now - *start_).count();
        previous_ = now;
        LOG(log_.info()) << "PublicationTiming ledger=" << ledger_ << " operation=" << operation_
                         << " stage=" << stage << " elapsed_us=" << elapsed
                         << " total_us=" << total;
    }

private:
    util::Logger& log_;
    std::uint32_t ledger_;
    std::string_view operation_;
    TimePoint start_;
    Clock::time_point previous_;
};

}  // namespace util
