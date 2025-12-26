/**
 * @file axis_assert.h
 * @brief AXIS Core Assert and Fatal Error System - Public C API
 *
 * This module provides debug-time assertions and fatal error handling for AXIS Core.
 *
 * Key concepts:
 * - AXIS_ASSERT: Debug-only check, removed in release builds
 * - AXIS_VERIFY: Always-checked, triggers FATAL on failure
 * - AXIS_FATAL: Unrecoverable error, does not return
 *
 * THREE AXES VALIDATION:
 * - TIME: Clear separation between debug (Assert) and runtime (Verify/Fatal)
 * - SPACE: Minimal overhead in release builds (asserts compiled out)
 * - DATA: Transparent failure reporting via customizable handlers
 */

#ifndef AXIS_CORE_ASSERT_H
#define AXIS_CORE_ASSERT_H

#include "axis_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handler for assertion failures
 *
 * @param file Source file where assertion failed
 * @param line Line number where assertion failed
 * @param condition The condition that failed (as string)
 * @param message Optional user message (can be NULL)
 * @param user_data User-provided data passed during handler registration
 * @return Non-zero to break into debugger, zero to continue
 */
typedef int (*AxisAssertHandler)(
    const char* file,
    int line,
    const char* condition,
    const char* message,
    void* user_data
);

/**
 * @brief Handler for fatal errors
 *
 * This handler is called for unrecoverable errors. The program will
 * terminate after this handler returns.
 *
 * @param file Source file where fatal error occurred
 * @param line Line number where fatal error occurred
 * @param message Error message
 * @param user_data User-provided data passed during handler registration
 */
typedef void (*AxisFatalErrorHandler)(
    const char* file,
    int line,
    const char* message,
    void* user_data
);

/**
 * @brief Set custom handler for assertion failures
 *
 * Only one handler can be active at a time. Passing NULL restores default handler.
 * Thread-safe.
 *
 * @param handler The handler function (NULL for default)
 * @param user_data User data to pass to handler (can be NULL)
 */
AXIS_API void Axis_SetAssertHandler(AxisAssertHandler handler, void* user_data);

/**
 * @brief Set custom handler for fatal errors
 *
 * Only one handler can be active at a time. Passing NULL restores default handler.
 * Thread-safe.
 *
 * @param handler The handler function (NULL for default)
 * @param user_data User data to pass to handler (can be NULL)
 */
AXIS_API void Axis_SetFatalErrorHandler(AxisFatalErrorHandler handler, void* user_data);

/**
 * @brief Internal function called when assertion fails
 *
 * Do not call directly - use AXIS_ASSERT macros instead.
 *
 * @param file Source file
 * @param line Line number
 * @param condition Condition that failed
 * @param message Optional message (can be NULL)
 * @return Non-zero if debugger break requested
 */
AXIS_API int Axis_AssertFailed(
    const char* file,
    int line,
    const char* condition,
    const char* message
);

/**
 * @brief Trigger fatal error
 *
 * This function does NOT return. The program will terminate after the handler runs.
 *
 * @param file Source file
 * @param line Line number
 * @param message Error message
 */
AXIS_API void Axis_FatalError(
    const char* file,
    int line,
    const char* message
);

// ============================================================================
// Macros
// ============================================================================

#ifdef _DEBUG

/**
 * @brief Debug-only assertion
 *
 * Checks condition in debug builds. Removed completely in release builds.
 * Use for internal consistency checks that should never fail in correct code.
 */
#define AXIS_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            if (Axis_AssertFailed(__FILE__, __LINE__, #cond, NULL)) { \
                __debugbreak(); \
            } \
        } \
    } while (0)

/**
 * @brief Debug-only assertion with message
 *
 * Same as AXIS_ASSERT but with custom message.
 */
#define AXIS_ASSERT_MSG(cond, msg) \
    do { \
        if (!(cond)) { \
            if (Axis_AssertFailed(__FILE__, __LINE__, #cond, msg)) { \
                __debugbreak(); \
            } \
        } \
    } while (0)

#else

// In release builds, asserts compile to nothing
#define AXIS_ASSERT(cond) ((void)0)
#define AXIS_ASSERT_MSG(cond, msg) ((void)0)

#endif // _DEBUG

/**
 * @brief Always-checked verification
 *
 * Checks condition in both debug and release builds.
 * Triggers FATAL error if condition fails.
 * Use for critical checks that must always be validated (e.g., API contract violations).
 */
#define AXIS_VERIFY(cond) \
    do { \
        if (!(cond)) { \
            Axis_FatalError(__FILE__, __LINE__, "Verification failed: " #cond); \
        } \
    } while (0)

/**
 * @brief Trigger fatal error with message
 *
 * Use for unrecoverable error conditions.
 * This does NOT return - program will terminate.
 */
#define AXIS_FATAL(msg) \
    Axis_FatalError(__FILE__, __LINE__, msg)

#ifdef __cplusplus
}
#endif

#endif // AXIS_CORE_ASSERT_H
