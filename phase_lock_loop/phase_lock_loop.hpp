#pragma once

#ifndef INCLUDED_PHASE_LOCK_LOOP_HPP
#define INCLUDED_PHASE_LOCK_LOOP_HPP

#include <chrono>

class phase_lock_loop {
public:
    explicit phase_lock_loop(double frequency_hz) noexcept
        : nominal_frequency_(frequency_hz)
        , period_ns_(static_cast<long long>(1e9 / frequency_hz))
        , next_tick_(std::chrono::system_clock::now())
    {}

    /**
     * @brief Align internal timeline with the master clock.
     */
    void synchronize(double master_timestamp_s) {
        auto const master_time = std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::duration<double>(master_timestamp_s)
            )
        );
        auto const raw_error_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(master_time - next_tick_).count();

        // Shortest-path phase wrapping: $Error \in [-\frac{P}{2}, \frac{P}{2}]$
        long long const wrapped_error_ns = ((raw_error_ns + period_ns_ / 2) % period_ns_ + period_ns_) % period_ns_ - period_ns_ / 2;

        // Proportional steering
        adjustment_ns_ = wrapped_error_ns / 10;
    }

    void set_lead_time(std::chrono::nanoseconds duration) noexcept {
        lead_time_ = std::chrono::nanoseconds(static_cast<long long>(duration.count()));
    }

    void advance() noexcept {
        next_tick_ += std::chrono::nanoseconds(period_ns_ + adjustment_ns_);
        adjustment_ns_ = 0;
    }

    auto frequency() const noexcept { return nominal_frequency_; }

    std::chrono::system_clock::time_point at_tick() const noexcept { return next_tick_; }
    std::chrono::system_clock::time_point pre_tick() const noexcept { return next_tick_ - lead_time_; }

private:
    double const nominal_frequency_;
    long long const period_ns_;
    std::chrono::system_clock::time_point next_tick_;
    std::chrono::nanoseconds lead_time_ { std::chrono::milliseconds(100) };
    long long adjustment_ns_ = 0;
};

#endif // INCLUDED_PHASE_LOCK_LOOP_HPP