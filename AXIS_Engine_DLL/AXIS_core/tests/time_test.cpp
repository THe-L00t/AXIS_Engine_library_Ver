/**
 * @file time_test.cpp
 * @brief Test code for AXIS time system
 *
 * This file demonstrates usage of the time system.
 */

#include "pch.h"
#include "axis/core/axis_time.h"
#include <cstdio>
#include <cassert>
#include <thread>
#include <chrono>

void TestBasicTimeFlow() {
    printf("=== Testing Basic Time Flow ===\n");

    AxisTimeConfig config = {};
    config.time_source = nullptr;  // Use default platform timer
    config.fixed_delta_us = 0;      // Variable dt

    AxisResult result = Axis_InitializeTime(&config);
    assert(result == AXIS_OK);
    printf("  Initialized time system\n");

    // Initial state - should be zero
    AxisTimeState state;
    Axis_GetTimeState(&state);
    assert(state.total_elapsed_us == 0);
    assert(state.frame_delta_us == 0);
    assert(state.frame_count == 0);
    printf("  Initial state verified (all zeros)\n");

    // Simulate 3 frames with delays
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~16ms

        result = Axis_UpdateTime();
        assert(result == AXIS_OK);

        Axis_GetTimeState(&state);
        double dt_sec = Axis_MicrosecondsToSeconds(state.frame_delta_us);
        double total_sec = Axis_MicrosecondsToSeconds(state.total_elapsed_us);

        printf("  Frame %llu: dt=%.6f sec, total=%.6f sec\n",
               state.frame_count, dt_sec, total_sec);

        assert(state.frame_count == i + 1);
        assert(state.frame_delta_us > 0);  // Should have elapsed time
    }

    result = Axis_ShutdownTime();
    assert(result == AXIS_OK);
    printf("  Time system shutdown\n");
    printf("  Basic time flow test PASSED\n\n");
}

void TestFixedDelta() {
    printf("=== Testing Fixed Delta ===\n");

    // 60 FPS fixed step = 16666 microseconds
    AxisTimeConfig config = {};
    config.time_source = nullptr;
    config.fixed_delta_us = 16666;  // ~60 FPS

    AxisResult result = Axis_InitializeTime(&config);
    assert(result == AXIS_OK);
    printf("  Initialized with fixed_delta = 16666 us (~60 FPS)\n");

    // Run several frames
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));  // Vary actual delay

        result = Axis_UpdateTime();
        assert(result == AXIS_OK);

        AxisTimeState state;
        Axis_GetTimeState(&state);

        // Delta should ALWAYS be fixed_delta_us regardless of actual elapsed time
        assert(state.frame_delta_us == 16666);
        assert(state.fixed_delta_us == 16666);

        printf("  Frame %llu: dt=%llu us (fixed)\n",
               state.frame_count, state.frame_delta_us);
    }

    result = Axis_ShutdownTime();
    assert(result == AXIS_OK);
    printf("  Fixed delta test PASSED\n\n");
}

void TestUnitConversion() {
    printf("=== Testing Unit Conversion ===\n");

    // Microseconds to seconds
    AxisTimeMicroseconds us = 1000000;  // 1 second
    double seconds = Axis_MicrosecondsToSeconds(us);
    assert(seconds == 1.0);
    printf("  1,000,000 us = %.1f sec\n", seconds);

    us = 16666;  // ~60 FPS frame time
    seconds = Axis_MicrosecondsToSeconds(us);
    printf("  16,666 us = %.6f sec (~60 FPS)\n", seconds);

    // Seconds to microseconds
    AxisTimeMicroseconds converted = Axis_SecondsToMicroseconds(1.0);
    assert(converted == 1000000);
    printf("  1.0 sec = %llu us\n", converted);

    converted = Axis_SecondsToMicroseconds(0.016666);
    printf("  0.016666 sec = %llu us\n", converted);

    printf("  Unit conversion test PASSED\n\n");
}

void TestErrorConditions() {
    printf("=== Testing Error Conditions ===\n");

    // Double initialization
    AxisTimeConfig config = {};
    AxisResult result = Axis_InitializeTime(&config);
    assert(result == AXIS_OK);
    printf("  First initialization: OK\n");

    result = Axis_InitializeTime(&config);
    assert(result == AXIS_ERROR_ALREADY_INITIALIZED);
    printf("  Second initialization: ALREADY_INITIALIZED (expected)\n");

    Axis_ShutdownTime();

    // Update before initialization
    result = Axis_UpdateTime();
    assert(result == AXIS_ERROR_NOT_INITIALIZED);
    printf("  Update before init: NOT_INITIALIZED (expected)\n");

    // GetTimeState before initialization
    AxisTimeState state;
    result = Axis_GetTimeState(&state);
    assert(result == AXIS_ERROR_NOT_INITIALIZED);
    printf("  GetTimeState before init: NOT_INITIALIZED (expected)\n");

    // GetTimeState with NULL parameter
    Axis_InitializeTime(&config);
    result = Axis_GetTimeState(nullptr);
    assert(result == AXIS_ERROR_INVALID_PARAMETER);
    printf("  GetTimeState(NULL): INVALID_PARAMETER (expected)\n");

    Axis_ShutdownTime();

    printf("  Error conditions test PASSED\n\n");
}

// Custom time source for deterministic testing
namespace {
    uint64_t g_custom_ticks = 0;
    const uint64_t CUSTOM_TICKS_PER_SECOND = 1000000;  // 1 tick = 1 microsecond

    uint64_t CustomGetTicks(void* user_data) {
        (void)user_data;
        return g_custom_ticks;
    }
}

void TestCustomTimeSource() {
    printf("=== Testing Custom Time Source ===\n");

    g_custom_ticks = 0;

    AxisTimeSource custom_source;
    custom_source.GetCurrentTicks = CustomGetTicks;
    custom_source.ticks_per_second = CUSTOM_TICKS_PER_SECOND;
    custom_source.user_data = nullptr;

    AxisTimeConfig config = {};
    config.time_source = &custom_source;
    config.fixed_delta_us = 0;

    AxisResult result = Axis_InitializeTime(&config);
    assert(result == AXIS_OK);
    printf("  Initialized with custom time source\n");

    // Manually advance custom ticks
    g_custom_ticks += 16666;  // +16.666 ms
    result = Axis_UpdateTime();
    assert(result == AXIS_OK);

    AxisTimeState state;
    Axis_GetTimeState(&state);
    assert(state.frame_delta_us == 16666);
    printf("  Advanced by 16666 ticks: dt=%llu us\n", state.frame_delta_us);

    g_custom_ticks += 33333;  // +33.333 ms
    result = Axis_UpdateTime();
    assert(result == AXIS_OK);

    Axis_GetTimeState(&state);
    assert(state.frame_delta_us == 33333);
    assert(state.total_elapsed_us == 16666 + 33333);
    printf("  Advanced by 33333 ticks: dt=%llu us, total=%llu us\n",
           state.frame_delta_us, state.total_elapsed_us);

    Axis_ShutdownTime();
    printf("  Custom time source test PASSED\n\n");
}

void TestFrameCount() {
    printf("=== Testing Frame Count ===\n");

    AxisTimeConfig config = {};
    Axis_InitializeTime(&config);

    for (uint64_t expected_frame = 1; expected_frame <= 10; ++expected_frame) {
        Axis_UpdateTime();

        AxisTimeState state;
        Axis_GetTimeState(&state);

        assert(state.frame_count == expected_frame);
        printf("  Frame %llu verified\n", state.frame_count);
    }

    Axis_ShutdownTime();
    printf("  Frame count test PASSED\n\n");
}

void RunTimeTests() {
    printf("\n========================================\n");
    printf("AXIS Time System Test\n");
    printf("========================================\n\n");

    TestBasicTimeFlow();
    TestFixedDelta();
    TestUnitConversion();
    TestCustomTimeSource();
    TestFrameCount();
    TestErrorConditions();

    printf("========================================\n");
    printf("All Time Tests PASSED!\n");
    printf("========================================\n\n");
}
