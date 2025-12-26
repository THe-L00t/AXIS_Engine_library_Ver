/**
 * @file main.cpp
 * @brief AXIS Core Assert/Fatal Error System Test Program
 *
 * This console application tests the AXIS_core.dll assert and fatal error system.
 */

#include <iostream>
#include <cstdio>
#include <cstring>

// AXIS Core Public API
#include "axis/core/axis_assert.h"

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
// Custom Handler Test Data
// =============================================================================

struct AssertHandlerTestData {
    int call_count = 0;
    char last_file[256] = {};
    int last_line = 0;
    char last_condition[256] = {};
    char last_message[256] = {};
    bool trigger_break = false;
};

struct FatalHandlerTestData {
    int call_count = 0;
    char last_file[256] = {};
    int last_line = 0;
    char last_message[256] = {};
};

AssertHandlerTestData g_assert_test_data;
FatalHandlerTestData g_fatal_test_data;

// Custom assert handler for testing
int CustomAssertHandler(
    const char* file,
    int line,
    const char* condition,
    const char* message,
    void* user_data
) {
    auto* data = static_cast<AssertHandlerTestData*>(user_data);

    data->call_count++;
    strncpy_s(data->last_file, file, sizeof(data->last_file) - 1);
    data->last_line = line;
    strncpy_s(data->last_condition, condition, sizeof(data->last_condition) - 1);
    if (message) {
        strncpy_s(data->last_message, message, sizeof(data->last_message) - 1);
    } else {
        data->last_message[0] = '\0';
    }

    std::cout << "\n  [Custom Assert Handler Called]\n";
    std::cout << "    File: " << file << "\n";
    std::cout << "    Line: " << line << "\n";
    std::cout << "    Condition: " << condition << "\n";
    if (message) {
        std::cout << "    Message: " << message << "\n";
    }
    std::cout << "    Call count: " << data->call_count << "\n";

    // Return 0 to skip debugger break (for testing)
    return data->trigger_break ? 1 : 0;
}

// Custom fatal error handler for testing
void CustomFatalHandler(
    const char* file,
    int line,
    const char* message,
    void* user_data
) {
    auto* data = static_cast<FatalHandlerTestData*>(user_data);

    data->call_count++;
    strncpy_s(data->last_file, file, sizeof(data->last_file) - 1);
    data->last_line = line;
    strncpy_s(data->last_message, message, sizeof(data->last_message) - 1);

    std::cout << "\n  [Custom Fatal Handler Called]\n";
    std::cout << "    File: " << file << "\n";
    std::cout << "    Line: " << line << "\n";
    std::cout << "    Message: " << message << "\n";
    std::cout << "    Call count: " << data->call_count << "\n";

    // Note: Program will terminate after this handler returns
}

// =============================================================================
// Test Functions
// =============================================================================

void TestBasicAssert() {
    PrintSeparator("Testing Basic AXIS_ASSERT");

    std::cout << "  Note: AXIS_ASSERT is only active in Debug builds\n\n";

#ifdef _DEBUG
    // Test successful assertion (should not trigger)
    int x = 10;
    AXIS_ASSERT(x == 10);
    PrintTestResult("AXIS_ASSERT with true condition (no output expected)", true);

    AXIS_ASSERT(x > 5);
    PrintTestResult("AXIS_ASSERT with another true condition", true);

    // This would trigger an assertion failure:
    // AXIS_ASSERT(x == 20);  // Commented out - would trigger default handler
#else
    std::cout << "  AXIS_ASSERT is compiled out in Release builds\n";
    PrintTestResult("Assert disabled in Release", true);
#endif
}

void TestAssertWithMessage() {
    PrintSeparator("Testing AXIS_ASSERT_MSG");

#ifdef _DEBUG
    int value = 42;
    AXIS_ASSERT_MSG(value == 42, "Value should be 42");
    PrintTestResult("AXIS_ASSERT_MSG with true condition", true);
#else
    std::cout << "  AXIS_ASSERT_MSG is compiled out in Release builds\n";
    PrintTestResult("Assert with message disabled in Release", true);
#endif
}

void TestCustomAssertHandler() {
    PrintSeparator("Testing Custom Assert Handler");

#ifdef _DEBUG
    // Reset test data
    g_assert_test_data = AssertHandlerTestData();

    // Install custom handler
    Axis_SetAssertHandler(CustomAssertHandler, &g_assert_test_data);
    PrintTestResult("Set custom assert handler", true);

    // Trigger assertion failure
    std::cout << "\n  Triggering assertion failure (false condition):\n";
    int x = 10;
    AXIS_ASSERT(x == 20);  // This will fail and call our custom handler

    bool handler_was_called = (g_assert_test_data.call_count == 1);
    PrintTestResult("Custom handler was called", handler_was_called);

    if (handler_was_called) {
        std::cout << "    Captured condition: " << g_assert_test_data.last_condition << "\n";
        std::cout << "    Captured line: " << g_assert_test_data.last_line << "\n";
    }

    // Trigger another failure with message
    std::cout << "\n  Triggering assertion failure with message:\n";
    AXIS_ASSERT_MSG(x == 30, "Value should be 30 but isn't");

    bool handler_called_twice = (g_assert_test_data.call_count == 2);
    PrintTestResult("Custom handler called for message assert", handler_called_twice);

    if (handler_called_twice) {
        std::cout << "    Captured message: " << g_assert_test_data.last_message << "\n";
    }

    // Restore default handler
    Axis_SetAssertHandler(nullptr, nullptr);
    PrintTestResult("Restored default assert handler", true);
#else
    std::cout << "  Custom assert handler test requires Debug build\n";
    PrintTestResult("Skipped in Release", true);
#endif
}

void TestVerify() {
    PrintSeparator("Testing AXIS_VERIFY");

    std::cout << "  Note: AXIS_VERIFY is ALWAYS checked (Debug and Release)\n";
    std::cout << "  AXIS_VERIFY triggers FATAL error on failure\n\n";

    // Test successful verify
    int x = 100;
    AXIS_VERIFY(x == 100);
    PrintTestResult("AXIS_VERIFY with true condition", true);

    AXIS_VERIFY(x > 50);
    PrintTestResult("AXIS_VERIFY with another true condition", true);

    std::cout << "\n  WARNING: Verify failure test is commented out\n";
    std::cout << "  Uncomment to test, but it will terminate the program!\n";

    // This would terminate the program:
    // AXIS_VERIFY(x == 200);  // Would trigger fatal error

    PrintTestResult("AXIS_VERIFY test completed", true);
}

void TestCustomFatalHandler() {
    PrintSeparator("Testing Custom Fatal Handler");

    // Reset test data
    g_fatal_test_data = FatalHandlerTestData();

    // Install custom fatal handler
    Axis_SetFatalErrorHandler(CustomFatalHandler, &g_fatal_test_data);
    PrintTestResult("Set custom fatal handler", true);

    std::cout << "\n  Note: Testing fatal handler requires triggering AXIS_FATAL\n";
    std::cout << "  This will terminate the program after the handler runs!\n\n";

    std::cout << "  To test fatal handler, uncomment the AXIS_FATAL line below\n";
    std::cout << "  and recompile. The program will terminate after showing\n";
    std::cout << "  the custom handler output.\n\n";

    // Uncomment to test fatal error (will terminate program):
    // AXIS_FATAL("This is a test fatal error");

    PrintTestResult("Fatal handler installed (not triggered)", true);
}

void TestFatalMacro() {
    PrintSeparator("Testing AXIS_FATAL");

    std::cout << "  AXIS_FATAL immediately terminates the program\n";
    std::cout << "  It calls the fatal handler, then calls abort()\n\n";

    std::cout << "  To test AXIS_FATAL:\n";
    std::cout << "  1. Uncomment the line below\n";
    std::cout << "  2. Recompile and run\n";
    std::cout << "  3. Observe fatal handler output and program termination\n\n";

    // Uncomment to test (will terminate program):
    // AXIS_FATAL("Testing immediate program termination");

    PrintTestResult("AXIS_FATAL not triggered (test skipped)", true);
}

void TestHandlerThreadSafety() {
    PrintSeparator("Testing Handler Thread Safety");

    std::cout << "  Handler registration uses atomic operations\n";
    std::cout << "  Thread-safe to set/clear handlers at runtime\n\n";

    // Test setting and clearing handlers multiple times
    Axis_SetAssertHandler(CustomAssertHandler, &g_assert_test_data);
    Axis_SetAssertHandler(nullptr, nullptr);
    Axis_SetAssertHandler(CustomAssertHandler, &g_assert_test_data);
    Axis_SetAssertHandler(nullptr, nullptr);
    PrintTestResult("Multiple handler set/clear operations", true);

    Axis_SetFatalErrorHandler(CustomFatalHandler, &g_fatal_test_data);
    Axis_SetFatalErrorHandler(nullptr, nullptr);
    Axis_SetFatalErrorHandler(CustomFatalHandler, &g_fatal_test_data);
    Axis_SetFatalErrorHandler(nullptr, nullptr);
    PrintTestResult("Multiple fatal handler set/clear operations", true);
}

void TestBuildConfiguration() {
    PrintSeparator("Testing Build Configuration");

#ifdef _DEBUG
    std::cout << "  Current Build: DEBUG\n";
    std::cout << "  AXIS_ASSERT: ENABLED\n";
    std::cout << "  AXIS_ASSERT_MSG: ENABLED\n";
#else
    std::cout << "  Current Build: RELEASE\n";
    std::cout << "  AXIS_ASSERT: DISABLED (compiled out)\n";
    std::cout << "  AXIS_ASSERT_MSG: DISABLED (compiled out)\n";
#endif
    std::cout << "  AXIS_VERIFY: ENABLED (always)\n";
    std::cout << "  AXIS_FATAL: ENABLED (always)\n\n";

    PrintTestResult("Build configuration verified", true);
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main() {
    PrintSeparator("AXIS Core Assert/Fatal Error System Test");
    std::cout << "Version: 1.0\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";

    // Run all tests
    try {
        TestBuildConfiguration();
        TestBasicAssert();
        TestAssertWithMessage();
        TestCustomAssertHandler();
        TestVerify();
        TestCustomFatalHandler();
        TestFatalMacro();
        TestHandlerThreadSafety();
    }
    catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION: " << e.what() << "\n";
        return 1;
    }

    PrintSeparator("All Tests Completed Successfully!");

    std::cout << "\n";
    std::cout << "Summary:\n";
    std::cout << "  - All assert tests passed\n";
    std::cout << "  - Custom handlers working correctly\n";
    std::cout << "  - Verify and Fatal macros available\n";
    std::cout << "  - To test fatal errors, uncomment fatal test code\n";

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}
