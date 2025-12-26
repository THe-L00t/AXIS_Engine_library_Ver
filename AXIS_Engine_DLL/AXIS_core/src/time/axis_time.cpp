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
    std::lock_guard<std::mutex> lock(state.mutex);

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

    // Reset logical time
    state.total_elapsed_us.store(0, std::memory_order_release);
    state.frame_delta_us.store(0, std::memory_order_release);
    state.frame_count.store(0, std::memory_order_release);

    state.initialized = true;

    return AXIS_OK;
}

AxisResult Axis_ShutdownTime(void) {
    auto& state = GetTimeState();
    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.initialized) {
        return AXIS_ERROR_NOT_INITIALIZED;
    }

    state.initialized = false;

    return AXIS_OK;
}

AxisResult Axis_UpdateTime(void) {
    auto& state = GetTimeState();

    // Note: Update is NOT mutex-protected (single-threaded assumption)
    // Only init/shutdown use mutex
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

    // Update logical time (atomic for thread-safe reads)
    AxisTimeMicroseconds new_total = state.total_elapsed_us.load(std::memory_order_acquire) + delta_us;
    state.total_elapsed_us.store(new_total, std::memory_order_release);
    state.frame_delta_us.store(delta_us, std::memory_order_release);

    // Increment frame count
    uint64_t new_frame_count = state.frame_count.load(std::memory_order_acquire) + 1;
    state.frame_count.store(new_frame_count, std::memory_order_release);

    // Update last frame ticks for next update
    state.last_frame_ticks = state.current_ticks;

    return AXIS_OK;
}

AxisResult Axis_GetTimeState(AxisTimeState* out_state) {
    if (!out_state) {
        return AXIS_ERROR_INVALID_PARAMETER;
    }

    auto& state = GetTimeState();

    if (!state.initialized) {
        return AXIS_ERROR_NOT_INITIALIZED;
    }

    // Atomic snapshot (no mutex needed for reads)
    out_state->total_elapsed_us = state.total_elapsed_us.load(std::memory_order_acquire);
    out_state->frame_delta_us = state.frame_delta_us.load(std::memory_order_acquire);
    out_state->fixed_delta_us = state.fixed_delta_us;  // Constant after init
    out_state->frame_count = state.frame_count.load(std::memory_order_acquire);

    return AXIS_OK;
}

double Axis_MicrosecondsToSeconds(AxisTimeMicroseconds us) {
    return static_cast<double>(us) / 1000000.0;
}

AxisTimeMicroseconds Axis_SecondsToMicroseconds(double seconds) {
    return static_cast<AxisTimeMicroseconds>(seconds * 1000000.0);
}

} // extern "C"
