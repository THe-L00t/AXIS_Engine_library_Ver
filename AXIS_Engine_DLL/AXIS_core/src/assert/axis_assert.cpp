/**
 * @file axis_assert.cpp
 * @brief Implementation of AXIS Assert and Fatal Error System
 */

#include "pch.h"
#include "axis/core/axis_assert.h"
#include <cstdio>
#include <cstdlib>
#include <atomic>

namespace {

// ============================================================================
// Handler Storage (Thread-safe)
// ============================================================================

struct AssertHandlerData {
    std::atomic<AxisAssertHandler> handler;
    std::atomic<void*> user_data;
};

struct FatalHandlerData {
    std::atomic<AxisFatalErrorHandler> handler;
    std::atomic<void*> user_data;
};

AssertHandlerData g_assert_handler;
FatalHandlerData g_fatal_handler;

// ============================================================================
// Default Handlers
// ============================================================================

int DefaultAssertHandler(
    const char* file,
    int line,
    const char* condition,
    const char* message,
    void* user_data
) {
    (void)user_data;

    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "ASSERTION FAILED\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "File:      %s\n", file);
    fprintf(stderr, "Line:      %d\n", line);
    fprintf(stderr, "Condition: %s\n", condition);
    if (message) {
        fprintf(stderr, "Message:   %s\n", message);
    }
    fprintf(stderr, "========================================\n");
    fflush(stderr);

    // Return non-zero to trigger debugger break
    return 1;
}

void DefaultFatalErrorHandler(
    const char* file,
    int line,
    const char* message,
    void* user_data
) {
    (void)user_data;

    fprintf(stderr, "\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "FATAL ERROR\n");
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "File:    %s\n", file);
    fprintf(stderr, "Line:    %d\n", line);
    fprintf(stderr, "Message: %s\n", message);
    fprintf(stderr, "========================================\n");
    fprintf(stderr, "\nProgram will now terminate.\n");
    fflush(stderr);
}

} // anonymous namespace

// ============================================================================
// Public API Implementation
// ============================================================================

extern "C" {

void Axis_SetAssertHandler(AxisAssertHandler handler, void* user_data) {
    g_assert_handler.handler.store(handler, std::memory_order_release);
    g_assert_handler.user_data.store(user_data, std::memory_order_release);
}

void Axis_SetFatalErrorHandler(AxisFatalErrorHandler handler, void* user_data) {
    g_fatal_handler.handler.store(handler, std::memory_order_release);
    g_fatal_handler.user_data.store(user_data, std::memory_order_release);
}

int Axis_AssertFailed(
    const char* file,
    int line,
    const char* condition,
    const char* message
) {
    // Load handler atomically
    AxisAssertHandler handler = g_assert_handler.handler.load(std::memory_order_acquire);
    void* user_data = g_assert_handler.user_data.load(std::memory_order_acquire);

    // Use default if no custom handler set
    if (!handler) {
        handler = DefaultAssertHandler;
        user_data = nullptr;
    }

    // Invoke handler
    return handler(file, line, condition, message, user_data);
}

void Axis_FatalError(
    const char* file,
    int line,
    const char* message
) {
    // Load handler atomically
    AxisFatalErrorHandler handler = g_fatal_handler.handler.load(std::memory_order_acquire);
    void* user_data = g_fatal_handler.user_data.load(std::memory_order_acquire);

    // Use default if no custom handler set
    if (!handler) {
        handler = DefaultFatalErrorHandler;
        user_data = nullptr;
    }

    // Invoke handler
    handler(file, line, message, user_data);

    // Terminate program
    // Use abort() for immediate termination with core dump (helpful for debugging)
    std::abort();
}

} // extern "C"
