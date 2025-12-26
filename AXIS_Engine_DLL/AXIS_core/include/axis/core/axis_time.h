/**
 * @file axis_time.h
 * @brief AXIS Core Time System - Public C API
 *
 * This module provides the fundamental time definitions for all AXIS systems.
 *
 * TIME IS NOT A SERVICE - IT IS A LAW.
 *
 * This is NOT:
 * - A timer/alarm system
 * - A scheduler
 * - A game loop
 * - A frame limiter
 * - A concurrency manager
 *
 * This IS:
 * - The single source of truth for "what time is it now?"
 * - The foundation for deterministic simulation
 * - The shared time contract all systems depend on
 *
 * THREE AXES VALIDATION:
 * - TIME: Frame-to-frame progression is explicit (manual Update call)
 * - SPACE: Time units are explicit (microseconds), overflow controlled (uint64_t)
 * - DATA: Time source is transparent (platform injection), logic vs platform separated
 *
 * THREAD-SAFETY CONTRACT:
 * - Core Time assumes SINGLE-THREADED update
 * - Axis_InitializeTime: Call ONCE from main thread
 * - Axis_UpdateTime: Call from SINGLE THREAD (game loop)
 * - Axis_Time_Get*: Undefined behavior if called during Update
 * - NO mutex, NO atomic, NO thread-safety guarantees
 * - Core defines the LAW, not the concurrency model
 */

#ifndef AXIS_CORE_TIME_H
#define AXIS_CORE_TIME_H

#include "axis_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Time Types
// ============================================================================

/**
 * @brief Time value in microseconds
 *
 * - Unit: microseconds (1/1,000,000 second)
 * - Range: 0 to 2^64-1 (~584,542 years)
 * - Precision: Integer, no floating-point error
 */
typedef uint64_t AxisTimeMicroseconds;

/**
 * @brief Platform time source (injected by platform or user)
 *
 * This allows:
 * - Platform abstraction (Windows QPC, Linux clock_gettime, etc.)
 * - Test mocking (deterministic replay)
 * - Custom time sources (network time, recorded time, etc.)
 */
typedef struct AxisTimeSource {
    /**
     * @brief Get current platform ticks
     * @param user_data User-provided context
     * @return Current tick count
     */
    uint64_t (*GetCurrentTicks)(void* user_data);

    /**
     * @brief Platform ticks per second
     *
     * Used to convert ticks to microseconds.
     * Must be > 0.
     */
    uint64_t ticks_per_second;

    /**
     * @brief User data passed to GetCurrentTicks
     */
    void* user_data;
} AxisTimeSource;

/**
 * @brief Time system initialization configuration
 */
typedef struct AxisTimeConfig {
    /**
     * @brief Custom time source (optional)
     *
     * If NULL, uses default platform high-resolution timer:
     * - Windows: QueryPerformanceCounter
     * - Linux: clock_gettime(CLOCK_MONOTONIC)
     * - macOS: mach_absolute_time
     */
    AxisTimeSource* time_source;

    /**
     * @brief Fixed delta time in microseconds (optional)
     *
     * If 0, delta time is variable (actual elapsed time).
     * If > 0, delta time is fixed (useful for physics simulation).
     *
     * Example: 16666 microseconds = ~60 FPS fixed step
     */
    AxisTimeMicroseconds fixed_delta_us;
} AxisTimeConfig;

// ============================================================================
// Core API
// ============================================================================

/**
 * @brief Initialize the time system
 *
 * Must be called before any other time functions.
 * Call ONCE from main thread.
 *
 * @param config Configuration (NULL for default)
 * @return AXIS_OK on success
 *         AXIS_ERROR_ALREADY_INITIALIZED if already initialized
 *         AXIS_ERROR_INVALID_PARAMETER if time_source is invalid
 */
AXIS_API AxisResult Axis_InitializeTime(const AxisTimeConfig* config);

/**
 * @brief Shutdown the time system
 *
 * Call when done with time system.
 * Call ONCE from main thread.
 *
 * @return AXIS_OK on success
 *         AXIS_ERROR_NOT_INITIALIZED if not initialized
 */
AXIS_API AxisResult Axis_ShutdownTime(void);

/**
 * @brief Update time system (advance to next frame)
 *
 * MUST be called once per frame from SINGLE THREAD (game loop).
 * This calculates delta time and advances the logical clock.
 *
 * Time does NOT advance automatically - it only advances when you call this.
 * This ensures explicit, deterministic time progression.
 *
 * @return AXIS_OK on success
 *         AXIS_ERROR_NOT_INITIALIZED if not initialized
 */
AXIS_API AxisResult Axis_UpdateTime(void);

// ============================================================================
// Query API (Individual Getters - Core Philosophy)
// ============================================================================

/**
 * @brief Get total elapsed time since initialization
 *
 * This is the "absolute time" - how long the system has been running.
 *
 * @return Total elapsed microseconds, or 0 if not initialized
 */
AXIS_API AxisTimeMicroseconds Axis_Time_GetTotalElapsed(void);

/**
 * @brief Get delta time of last frame
 *
 * This is the "dt" used for animation, movement, etc.
 * If fixed_delta_us is set, this equals fixed_delta_us.
 * Otherwise, this is the actual elapsed time since last Update.
 *
 * @return Frame delta microseconds, or 0 if not initialized or before first Update
 */
AXIS_API AxisTimeMicroseconds Axis_Time_GetFrameDelta(void);

/**
 * @brief Get fixed delta time setting
 *
 * @return Fixed delta microseconds (0 if variable delta), or 0 if not initialized
 */
AXIS_API AxisTimeMicroseconds Axis_Time_GetFixedDelta(void);

/**
 * @brief Get total number of frames (Update calls)
 *
 * @return Frame count since initialization, or 0 if not initialized
 */
AXIS_API uint64_t Axis_Time_GetFrameCount(void);

#ifdef __cplusplus
}
#endif

#endif // AXIS_CORE_TIME_H
