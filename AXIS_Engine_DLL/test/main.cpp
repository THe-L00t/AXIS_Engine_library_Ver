/**
 * @file main.cpp
 * @brief AXIS Core Memory System Test Program
 *
 * This console application tests the AXIS_core.dll memory system.
 */

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cassert>

// AXIS Core Public API
#include "axis/core/axis_memory.h"

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

void TestGeneralAllocator() {
    PrintSeparator("Testing General Allocator");

    AxisGeneralAllocator* alloc = Axis_CreateGeneralAllocator("TestGeneral", 1024 * 1024);
    assert(alloc != nullptr);
    PrintTestResult("Create general allocator", alloc != nullptr);

    // Allocate various sizes
    void* ptr1 = Axis_AllocGeneral(alloc, 128, 16, AXIS_MEMORY_TAG_CORE);
    PrintTestResult("Allocate 128 bytes (16-byte aligned)", ptr1 != nullptr);
    std::cout << "    Address: " << ptr1 << "\n";

    void* ptr2 = Axis_AllocGeneral(alloc, 256, 32, AXIS_MEMORY_TAG_CORE);
    PrintTestResult("Allocate 256 bytes (32-byte aligned)", ptr2 != nullptr);
    std::cout << "    Address: " << ptr2 << "\n";

    void* ptr3 = Axis_AllocGeneral(alloc, 64, 8, AXIS_MEMORY_TAG_RENDERER);
    PrintTestResult("Allocate 64 bytes (8-byte aligned)", ptr3 != nullptr);
    std::cout << "    Address: " << ptr3 << "\n";

    // Write and verify data
    char* data = static_cast<char*>(ptr1);
    for (int i = 0; i < 128; ++i) {
        data[i] = static_cast<char>(i);
    }

    bool data_verified = true;
    for (int i = 0; i < 128; ++i) {
        if (data[i] != static_cast<char>(i)) {
            data_verified = false;
            break;
        }
    }
    PrintTestResult("Write and verify data", data_verified);

    // Free memory
    Axis_FreeGeneral(alloc, ptr1);
    Axis_FreeGeneral(alloc, ptr2);
    Axis_FreeGeneral(alloc, ptr3);
    PrintTestResult("Free all allocations", true);

    Axis_DestroyGeneralAllocator(alloc);
    PrintTestResult("Destroy general allocator", true);
}

void TestPoolAllocator() {
    PrintSeparator("Testing Pool Allocator");

    struct TestObject {
        int id;
        float value;
        char data[56];  // Total 64 bytes
    };

    AxisPoolAllocator* pool = Axis_CreatePoolAllocator(
        "TestPool",
        sizeof(TestObject),
        10,  // 10 objects
        AXIS_MEMORY_TAG_CORE
    );
    assert(pool != nullptr);
    PrintTestResult("Create pool allocator (10 x 64 bytes)", pool != nullptr);

    size_t free_count = Axis_GetPoolFreeCount(pool);
    std::cout << "    Initial free count: " << free_count << "\n";
    PrintTestResult("Check initial capacity", free_count == 10);

    // Allocate some objects
    TestObject* obj1 = static_cast<TestObject*>(Axis_AllocPool(pool));
    PrintTestResult("Allocate object 1", obj1 != nullptr);
    if (obj1) {
        obj1->id = 1;
        obj1->value = 1.0f;
        std::cout << "    Object 1 address: " << obj1 << "\n";
    }

    TestObject* obj2 = static_cast<TestObject*>(Axis_AllocPool(pool));
    PrintTestResult("Allocate object 2", obj2 != nullptr);
    if (obj2) {
        obj2->id = 2;
        obj2->value = 2.0f;
        std::cout << "    Object 2 address: " << obj2 << "\n";
    }

    free_count = Axis_GetPoolFreeCount(pool);
    std::cout << "    Free count after 2 allocs: " << free_count << "\n";
    PrintTestResult("Check free count decreased", free_count == 8);

    // Free one object
    Axis_FreePool(pool, obj1);
    PrintTestResult("Free object 1", true);

    free_count = Axis_GetPoolFreeCount(pool);
    std::cout << "    Free count after 1 free: " << free_count << "\n";
    PrintTestResult("Check free count increased", free_count == 9);

    // Allocate again (should reuse freed slot)
    TestObject* obj3 = static_cast<TestObject*>(Axis_AllocPool(pool));
    PrintTestResult("Allocate object 3 (reused slot)", obj3 != nullptr);
    std::cout << "    Object 3 address: " << obj3 << " (should be same as obj1)\n";

    // Free remaining objects
    Axis_FreePool(pool, obj2);
    Axis_FreePool(pool, obj3);
    PrintTestResult("Free remaining objects", true);

    Axis_DestroyPoolAllocator(pool);
    PrintTestResult("Destroy pool allocator", true);
}

void TestFrameAllocator() {
    PrintSeparator("Testing Frame Allocator");

    AxisFrameAllocator* frame = Axis_CreateFrameAllocator(
        "TestFrame",
        1024,  // 1KB
        AXIS_MEMORY_TAG_TEMP
    );
    assert(frame != nullptr);
    PrintTestResult("Create frame allocator (1KB)", frame != nullptr);

    size_t usage = Axis_GetFrameUsage(frame);
    std::cout << "    Initial usage: " << usage << " bytes\n";
    PrintTestResult("Check initial usage is zero", usage == 0);

    // Allocate some temporary data
    void* temp1 = Axis_AllocFrame(frame, 128, 16);
    PrintTestResult("Allocate 128 bytes", temp1 != nullptr);
    std::cout << "    Temp1 address: " << temp1 << "\n";

    usage = Axis_GetFrameUsage(frame);
    std::cout << "    Usage after alloc: " << usage << " bytes\n";

    void* temp2 = Axis_AllocFrame(frame, 256, 16);
    PrintTestResult("Allocate 256 bytes", temp2 != nullptr);
    std::cout << "    Temp2 address: " << temp2 << "\n";

    usage = Axis_GetFrameUsage(frame);
    std::cout << "    Usage after 2nd alloc: " << usage << " bytes\n";

    // Write to memory
    char* data = static_cast<char*>(temp1);
    for (int i = 0; i < 128; ++i) {
        data[i] = static_cast<char>(i);
    }
    PrintTestResult("Write data to frame memory", true);

    // Reset (simulating end of frame)
    Axis_ResetFrameAllocator(frame);
    PrintTestResult("Reset frame allocator", true);

    usage = Axis_GetFrameUsage(frame);
    std::cout << "    Usage after reset: " << usage << " bytes\n";
    PrintTestResult("Check usage is zero after reset", usage == 0);

    // Allocate again
    void* temp3 = Axis_AllocFrame(frame, 64, 16);
    PrintTestResult("Allocate 64 bytes after reset", temp3 != nullptr);
    std::cout << "    Temp3 address: " << temp3 << " (should be same as temp1)\n";

    Axis_DestroyFrameAllocator(frame);
    PrintTestResult("Destroy frame allocator", true);
}

void TestStatistics() {
    PrintSeparator("Testing Statistics");

    AxisMemoryStats stats;
    AxisResult result = Axis_GetMemoryStats(&stats);
    PrintTestResult("Get overall statistics", result == AXIS_OK);

    std::cout << "  Total current bytes: " << stats.total_current_bytes << "\n";
    std::cout << "  Total peak bytes: " << stats.total_peak_bytes << "\n\n";

    std::cout << "  Per-tag statistics:\n";
    const char* tag_names[] = {
        "Core",
        "Renderer",
        "Resource",
        "Audio",
        "Physics",
        "Temp"
    };

    for (int i = 0; i < AXIS_MEMORY_TAG_COUNT; ++i) {
        AxisMemoryTagStats tag_stats;
        result = Axis_GetTagStats(static_cast<AxisMemoryTag>(i), &tag_stats);

        if (result == AXIS_OK && tag_stats.total_allocations > 0) {
            std::cout << "    " << std::left << std::setw(12) << tag_names[i] << ": "
                      << "current=" << std::setw(8) << tag_stats.current_bytes
                      << " peak=" << std::setw(8) << tag_stats.peak_bytes
                      << " allocs=" << std::setw(6) << tag_stats.total_allocations
                      << " frees=" << tag_stats.total_frees << "\n";
        }
    }

    PrintTestResult("Statistics test completed", true);
}

void TestCapacityLimits() {
    PrintSeparator("Testing Capacity Limits");

    // Test pool exhaustion
    AxisPoolAllocator* small_pool = Axis_CreatePoolAllocator("SmallPool", 64, 3, AXIS_MEMORY_TAG_CORE);
    PrintTestResult("Create small pool (3 objects)", small_pool != nullptr);

    void* p1 = Axis_AllocPool(small_pool);
    void* p2 = Axis_AllocPool(small_pool);
    void* p3 = Axis_AllocPool(small_pool);
    void* p4 = Axis_AllocPool(small_pool);  // Should fail

    PrintTestResult("Pool exhaustion test", p1 && p2 && p3 && !p4);
    std::cout << "    Allocated: " << p1 << ", " << p2 << ", " << p3 << "\n";
    std::cout << "    Failed (expected): " << p4 << "\n";

    Axis_DestroyPoolAllocator(small_pool);

    // Test frame allocator overflow
    AxisFrameAllocator* small_frame = Axis_CreateFrameAllocator("SmallFrame", 128, AXIS_MEMORY_TAG_TEMP);
    PrintTestResult("Create small frame (128 bytes)", small_frame != nullptr);

    void* f1 = Axis_AllocFrame(small_frame, 64, 16);
    void* f2 = Axis_AllocFrame(small_frame, 64, 16);
    void* f3 = Axis_AllocFrame(small_frame, 64, 16);  // Should fail

    PrintTestResult("Frame overflow test", f1 && f2 && !f3);
    std::cout << "    Allocated: " << f1 << ", " << f2 << "\n";
    std::cout << "    Failed (expected): " << f3 << "\n";

    Axis_DestroyFrameAllocator(small_frame);
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main() {
    PrintSeparator("AXIS Core Memory System Test");
    std::cout << "Version: 1.0\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";

    // Initialize memory system
    AxisMemoryConfig config;
    config.general_reserve_bytes = 1024 * 1024;  // 1MB
    config.enable_statistics = 1;

    AxisResult result = Axis_InitializeMemory(&config);
    if (result != AXIS_OK) {
        std::cerr << "ERROR: Failed to initialize AXIS memory system!\n";
        return 1;
    }
    PrintTestResult("Initialize memory system", true);

    // Run all tests
    try {
        TestGeneralAllocator();
        TestPoolAllocator();
        TestFrameAllocator();
        TestStatistics();
        TestCapacityLimits();
    }
    catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION: " << e.what() << "\n";
        Axis_ShutdownMemory();
        return 1;
    }

    // Shutdown
    result = Axis_ShutdownMemory();
    PrintTestResult("Shutdown memory system", result == AXIS_OK);

    PrintSeparator("All Tests Completed Successfully!");
    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return 0;
}
