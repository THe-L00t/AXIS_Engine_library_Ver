/**
 * @file time_internal.h
 * @brief Internal time system utilities and state
 */

#ifndef AXIS_TIME_INTERNAL_H
#define AXIS_TIME_INTERNAL_H

#include "axis/core/axis_time.h"

namespace axis::core::time {

/**
 * @brief Internal time system state
 *
 * Thread-safety contract (Core philosophy):
 * - Axis_InitializeTime: Call ONCE from main thread before any other calls
 * - Axis_ShutdownTime: Call ONCE from main thread after all other calls
 * - Axis_UpdateTime: Call from SINGLE THREAD (game loop thread)
 * - Axis_Time_Get*: Reading during Update has undefined behavior
 *
 * NO mutex, NO atomic, NO thread-safety guarantees.
 * Core defines the law of time, not the concurrency model.
 */
struct TimeSystemState {
    // Initialization state
    bool initialized = false;
    bool has_custom_source = false;

    // Time source
    AxisTimeSource time_source = {};

    // Platform ticks (raw values from time source)
    uint64_t start_ticks = 0;
    uint64_t last_frame_ticks = 0;
    uint64_t current_ticks = 0;

    // Logical time (microseconds)
    AxisTimeMicroseconds total_elapsed_us = 0;
    AxisTimeMicroseconds frame_delta_us = 0;
    AxisTimeMicroseconds fixed_delta_us = 0;  // Constant after init

    // Frame count
    uint64_t frame_count = 0;
};

/**
 * @brief Get global time system state
 */
TimeSystemState& GetTimeState();

/**
 * @brief Get default platform time source
 *
 * Platform-specific implementation:
 * - Windows: QueryPerformanceCounter
 * - Linux: clock_gettime(CLOCK_MONOTONIC)
 * - macOS: mach_absolute_time
 */
AxisTimeSource GetDefaultTimeSource();

/**
 * @brief Convert ticks to microseconds
 *
 * @param ticks Tick count
 * @param ticks_per_second Ticks per second from time source
 * @return Microseconds
 */
inline AxisTimeMicroseconds TicksToMicroseconds(uint64_t ticks, uint64_t ticks_per_second) {
    // Avoid overflow: (ticks * 1000000) / ticks_per_second
    // Use double intermediate for safety (minor precision loss acceptable for time)
    double us_double = (static_cast<double>(ticks) * 1000000.0) / static_cast<double>(ticks_per_second);
    return static_cast<AxisTimeMicroseconds>(us_double);
}

} // namespace axis::core::time

#endif // AXIS_TIME_INTERNAL_H
