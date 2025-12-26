/**
 * @file axis_time.cpp
 * @brief Implementation of AXIS Time System
 */

#include "pch.h"
#include "time_internal.h"
#include "axis/core/axis_assert.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace axis::core::time {

// ============================================================================
// Global State
// ============================================================================

TimeSystemState& GetTimeState() {
    static TimeSystemState state;
    return state;
}

// ============================================================================
// Platform Time Source (Windows)
// ============================================================================

#ifdef _WIN32

namespace {
    uint64_t WindowsGetCurrentTicks(void* user_data) {
        (void)user_data;
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return static_cast<uint64_t>(counter.QuadPart);
    }

    uint64_t WindowsGetTicksPerSecond() {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return static_cast<uint64_t>(frequency.QuadPart);
    }
}

AxisTimeSource GetDefaultTimeSource() {
    AxisTimeSource source;
    source.GetCurrentTicks = WindowsGetCurrentTicks;
    source.ticks_per_second = WindowsGetTicksPerSecond();
    source.user_data = nullptr;
    return source;
}

#else
#error "Platform time source not implemented for this platform"
#endif

} // namespace axis::core::time

using namespace axis::core::time;

// ============================================================================
// Public API Implementation
// ============================================================================

extern "C" {

AxisResult Axis_InitializeTime(const AxisTimeConfig* config) {
    auto& state = GetTimeState();

    // Single-threaded init assumption (no mutex needed)
    if (state.initialized) {
        return AXIS_ERROR_ALREADY_INITIALIZED;
    }

    // Configure time source
    if (config && config->time_source) {
        // Custom time source
        AXIS_VERIFY(config->time_source->GetCurrentTicks != nullptr);
        AXIS_VERIFY(config->time_source->ticks_per_second > 0);

        state.time_source = *config->time_source;
        state.has_custom_source = true;
    } else {
        // Default platform time source
        state.time_source = GetDefaultTimeSource();
        state.has_custom_source = false;
    }

    // Configure fixed delta
    state.fixed_delta_us = (config ? config->fixed_delta_us : 0);

    // Initialize ticks
    state.start_ticks = state.time_source.GetCurrentTicks(state.time_source.user_data);
    state.last_frame_ticks = state.start_ticks;
    state.current_ticks = state.start_ticks;

    // Reset logical time (NO atomic, just direct assignment)
    state.total_elapsed_us = 0;
    state.frame_delta_us = 0;
    state.frame_count = 0;

    state.initialized = true;

    return AXIS_OK;
}

AxisResult Axis_ShutdownTime(void) {
    auto& state = GetTimeState();

    // Single-threaded shutdown assumption (no mutex needed)
    if (!state.initialized) {
        return AXIS_ERROR_NOT_INITIALIZED;
    }

    state.initialized = false;

    return AXIS_OK;
}

AxisResult Axis_UpdateTime(void) {
    auto& state = GetTimeState();

    // NO lock - single-threaded assumption
    if (!state.initialized) {
        return AXIS_ERROR_NOT_INITIALIZED;
    }

    // Get current ticks from platform
    state.current_ticks = state.time_source.GetCurrentTicks(state.time_source.user_data);

    // Calculate delta ticks
    uint64_t delta_ticks = state.current_ticks - state.last_frame_ticks;

    // Convert to microseconds
    AxisTimeMicroseconds delta_us = TicksToMicroseconds(delta_ticks, state.time_source.ticks_per_second);

    // If fixed delta is set, use it instead of actual delta
    if (state.fixed_delta_us > 0) {
        delta_us = state.fixed_delta_us;
    }

    // Update logical time (NO atomic, just direct assignment)
    state.total_elapsed_us += delta_us;
    state.frame_delta_us = delta_us;
    state.frame_count++;

    // Update last frame ticks for next update
    state.last_frame_ticks = state.current_ticks;

    return AXIS_OK;
}

// ============================================================================
// Individual Getter Functions (Core Philosophy)
// ============================================================================

AxisTimeMicroseconds Axis_Time_GetTotalElapsed(void) {
    auto& state = GetTimeState();

    if (!state.initialized) {
        return 0;
    }

    return state.total_elapsed_us;
}

AxisTimeMicroseconds Axis_Time_GetFrameDelta(void) {
    auto& state = GetTimeState();

    if (!state.initialized) {
        return 0;
    }

    return state.frame_delta_us;
}

AxisTimeMicroseconds Axis_Time_GetFixedDelta(void) {
    auto& state = GetTimeState();

    if (!state.initialized) {
        return 0;
    }

    return state.fixed_delta_us;
}

uint64_t Axis_Time_GetFrameCount(void) {
    auto& state = GetTimeState();

    if (!state.initialized) {
        return 0;
    }

    return state.frame_count;
}

} // extern "C"
