/**
 * @file time_internal.h
 * @brief Internal time system utilities and state
 */

#ifndef AXIS_TIME_INTERNAL_H
#define AXIS_TIME_INTERNAL_H

#include "axis/core/axis_time.h"
#include <mutex>
#include <atomic>

namespace axis::core::time {

/**
 * @brief Internal time system state
 *
 * Thread-safety strategy:
 * - Initialization/Shutdown: mutex-protected
 * - Update: single-threaded (called from main loop)
 * - Read (GetTimeState): atomic reads of individual fields
 */
struct TimeSystemState {
    // Initialization state
    bool initialized = false;
    bool has_custom_source = false;
    std::mutex mutex;  // For init/shutdown only

    // Time source
    AxisTimeSource time_source = {};

    // Platform ticks (raw values from time source)
    uint64_t start_ticks = 0;
    uint64_t last_frame_ticks = 0;
    uint64_t current_ticks = 0;

    // Logical time (microseconds)
    std::atomic<AxisTimeMicroseconds> total_elapsed_us{0};
    std::atomic<AxisTimeMicroseconds> frame_delta_us{0};
    AxisTimeMicroseconds fixed_delta_us = 0;  // Constant after init

    // Frame count
    std::atomic<uint64_t> frame_count{0};
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
