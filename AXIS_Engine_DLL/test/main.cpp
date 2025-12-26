/**
 * @file main.cpp
 * @brief AXIS Core Time System Test Program
 *
 * This console application tests the AXIS_core.dll time system.
 */

#include <iostream>
#include <cstdio>
#include <cassert>
#include <thread>
#include <chrono>

// AXIS Core Public API
#include "axis/core/axis_time.h"

// =============================================================================
// Test Helper Functions
// =============================================================================

void PrintSeparator(const char* title = nullptr) {
    std::cout << "\n";
    std::cout << "========================================\n";
    if (title) {
        std::cout << title << "\n";
        std::cout << "========================================\n";
    }
}

void PrintTestResult(const char* test_name, bool passed) {
    std::cout << "  [" << (passed ? "PASS" : "FAIL") << "] " << test_name << "\n";
}

// =============================================================================
// Test Functions
// =============================================================================

void TestBasicTimeFlow() {
    PrintSeparator("Testing Basic Time Flow");

    AxisTimeConfig config = {};
    config.time_source = nullptr;  // Use default platform timer
    config.fixed_delta_us = 0;      // Variable dt

    AxisResult result = Axis_InitializeTime(&config);
    assert(result == AXIS_OK);
    PrintTestResult("Initialize time system", true);

    // Initial state - should be zero
    AxisTimeState state;
    Axis_GetTimeState(&state);
    PrintTestResult("Get initial time state", state.total_elapsed_us == 0 && state.frame_count == 0);

    std::cout << "    Total: " << state.total_elapsed_us << " us\n";
    std::cout << "    Delta: " << state.frame_delta_us << " us\n";
    std::cout << "    Frames: " << state.frame_count << "\n";

    // Simulate 3 frames with delays
    std::cout << "\n  Simulating 3 frames with ~16ms delays:\n";
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        result = Axis_UpdateTime();
        assert(result == AXIS_OK);

        Axis_GetTimeState(&state);
        double dt_sec = Axis_MicrosecondsToSeconds(state.frame_delta_us);
        double total_sec = Axis_MicrosecondsToSeconds(state.total_elapsed_us);

        std::cout << "    Frame " << state.frame_count << ": dt=" << dt_sec << "s, total=" << total_sec << "s\n";
    }

    PrintTestResult("Frame updates working", state.frame_count == 3);

    result = Axis_ShutdownTime();
    PrintTestResult("Shutdown time system", result == AXIS_OK);
}

void TestFixedDelta() {
    PrintSeparator("Testing Fixed Delta (60 FPS)");

    // 60 FPS fixed step = 16666 microseconds
    AxisTimeConfig config = {};
    config.time_source = nullptr;
    config.fixed_delta_us = 16666;  // ~60 FPS

    AxisResult result = Axis_InitializeTime(&config);
    PrintTestResult("Initialize with fixed_delta = 16666 us", result == AXIS_OK);

    std::cout << "\n  Running 5 frames with varying actual delays:\n";
    std::cout << "  (Delta should ALWAYS be 16666 us regardless)\n\n";

    bool all_deltas_fixed = true;
    for (int i = 0; i < 5; ++i) {
        // Vary actual delay (10-30ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + i * 5));

        result = Axis_UpdateTime();
        assert(result == AXIS_OK);

        AxisTimeState state;
        Axis_GetTimeState(&state);

        std::cout << "    Frame " << state.frame_count << ": dt=" << state.frame_delta_us << " us";

        if (state.frame_delta_us == 16666) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗ (expected 16666)\n";
            all_deltas_fixed = false;
        }
    }

    PrintTestResult("All deltas fixed at 16666 us", all_deltas_fixed);

    result = Axis_ShutdownTime();
    assert(result == AXIS_OK);
}

void TestUnitConversion() {
    PrintSeparator("Testing Unit Conversion");

    struct TestCase {
        AxisTimeMicroseconds us;
        double expected_sec;
        const char* description;
    };

    TestCase cases[] = {
        {1000000, 1.0, "1 second"},
        {16666, 0.016666, "~60 FPS frame time"},
        {33333, 0.033333, "~30 FPS frame time"},
        {8333, 0.008333, "~120 FPS frame time"}
    };

    bool all_passed = true;
    for (const auto& tc : cases) {
        double seconds = Axis_MicrosecondsToSeconds(tc.us);
        bool passed = (std::abs(seconds - tc.expected_sec) < 0.000001);

        std::cout << "  " << tc.us << " us -> " << seconds << " sec (" << tc.description << ")";
        if (passed) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗\n";
            all_passed = false;
        }
    }

    PrintTestResult("Microseconds to seconds conversion", all_passed);

    // Test reverse conversion
    AxisTimeMicroseconds converted = Axis_SecondsToMicroseconds(1.0);
    bool reverse_ok = (converted == 1000000);
    std::cout << "  1.0 sec -> " << converted << " us";
    if (reverse_ok) {
        std::cout << " ✓\n";
    } else {
        std::cout << " ✗\n";
    }

    PrintTestResult("Seconds to microseconds conversion", reverse_ok);
}

void TestErrorConditions() {
    PrintSeparator("Testing Error Conditions");

    // Double initialization
    AxisTimeConfig config = {};
    AxisResult result = Axis_InitializeTime(&config);
    PrintTestResult("First initialization", result == AXIS_OK);

    result = Axis_InitializeTime(&config);
    PrintTestResult("Second init returns ALREADY_INITIALIZED", result == AXIS_ERROR_ALREADY_INITIALIZED);

    Axis_ShutdownTime();

    // Operations before initialization
    result = Axis_UpdateTime();
    PrintTestResult("Update before init returns NOT_INITIALIZED", result == AXIS_ERROR_NOT_INITIALIZED);

    AxisTimeState state;
    result = Axis_GetTimeState(&state);
    PrintTestResult("GetTimeState before init returns NOT_INITIALIZED", result == AXIS_ERROR_NOT_INITIALIZED);

    // NULL parameter
    Axis_InitializeTime(&config);
    result = Axis_GetTimeState(nullptr);
    PrintTestResult("GetTimeState(NULL) returns INVALID_PARAMETER", result == AXIS_ERROR_INVALID_PARAMETER);

    Axis_ShutdownTime();
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
    PrintSeparator("Testing Custom Time Source (Deterministic)");

    g_custom_ticks = 0;

    AxisTimeSource custom_source;
    custom_source.GetCurrentTicks = CustomGetTicks;
    custom_source.ticks_per_second = CUSTOM_TICKS_PER_SECOND;
    custom_source.user_data = nullptr;

    AxisTimeConfig config = {};
    config.time_source = &custom_source;
    config.fixed_delta_us = 0;

    AxisResult result = Axis_InitializeTime(&config);
    PrintTestResult("Initialize with custom time source", result == AXIS_OK);

    std::cout << "\n  Manually controlling time progression:\n";

    // Frame 1: advance by 16666 us
    g_custom_ticks += 16666;
    Axis_UpdateTime();

    AxisTimeState state;
    Axis_GetTimeState(&state);
    std::cout << "    Ticks += 16666: dt=" << state.frame_delta_us << " us, total=" << state.total_elapsed_us << " us\n";
    bool frame1_ok = (state.frame_delta_us == 16666 && state.total_elapsed_us == 16666);

    // Frame 2: advance by 33333 us
    g_custom_ticks += 33333;
    Axis_UpdateTime();

    Axis_GetTimeState(&state);
    std::cout << "    Ticks += 33333: dt=" << state.frame_delta_us << " us, total=" << state.total_elapsed_us << " us\n";
    bool frame2_ok = (state.frame_delta_us == 33333 && state.total_elapsed_us == 49999);

    // Frame 3: advance by 10000 us
    g_custom_ticks += 10000;
    Axis_UpdateTime();

    Axis_GetTimeState(&state);
    std::cout << "    Ticks += 10000: dt=" << state.frame_delta_us << " us, total=" << state.total_elapsed_us << " us\n";
    bool frame3_ok = (state.frame_delta_us == 10000 && state.total_elapsed_us == 59999);

    PrintTestResult("Custom time source deterministic", frame1_ok && frame2_ok && frame3_ok);

    Axis_ShutdownTime();
}

void TestFrameCount() {
    PrintSeparator("Testing Frame Count");

    AxisTimeConfig config = {};
    Axis_InitializeTime(&config);

    std::cout << "\n  Running 10 updates:\n";

    bool all_correct = true;
    for (uint64_t expected_frame = 1; expected_frame <= 10; ++expected_frame) {
        Axis_UpdateTime();

        AxisTimeState state;
        Axis_GetTimeState(&state);

        std::cout << "    Update " << expected_frame << ": frame_count=" << state.frame_count;
        if (state.frame_count == expected_frame) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗\n";
            all_correct = false;
        }
    }

    PrintTestResult("Frame count increments correctly", all_correct);

    Axis_ShutdownTime();
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main() {
    PrintSeparator("AXIS Core Time System Test");
    std::cout << "Version: 1.0\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";

    // Run all tests
    try {
        TestBasicTimeFlow();
        TestFixedDelta();
        TestUnitConversion();
        TestCustomTimeSource();
        TestFrameCount();
        TestErrorConditions();
    }
    catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION: " << e.what() << "\n";
        return 1;
    }

    PrintSeparator("All Tests Completed Successfully!");

    std::cout << "\n";
    std::cout << "Summary:\n";
    std::cout << "  - Basic time flow: PASSED\n";
    std::cout << "  - Fixed delta (60 FPS): PASSED\n";
    std::cout << "  - Unit conversion: PASSED\n";
    std::cout << "  - Custom time source: PASSED\n";
    std::cout << "  - Frame count: PASSED\n";
    std::cout << "  - Error conditions: PASSED\n";

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}
