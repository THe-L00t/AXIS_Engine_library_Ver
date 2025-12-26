/**
 * @file memory_test.cpp
 * @brief Test code for AXIS memory system
 *
 * This file demonstrates usage of the three allocator types.
 */

#include "pch.h"
#include "axis/core/axis_memory.h"
#include <cstdio>
#include <cassert>

void TestGeneralAllocator() {
    printf("=== Testing General Allocator ===\n");

    AxisGeneralAllocator* alloc = Axis_CreateGeneralAllocator("TestGeneral", 1024 * 1024);
    assert(alloc != nullptr);

    // Allocate various sizes
    void* ptr1 = Axis_AllocGeneral(alloc, 128, 16, AXIS_MEMORY_TAG_CORE);
    assert(ptr1 != nullptr);
    printf("  Allocated 128 bytes at %p\n", ptr1);

    void* ptr2 = Axis_AllocGeneral(alloc, 256, 32, AXIS_MEMORY_TAG_CORE);
    assert(ptr2 != nullptr);
    printf("  Allocated 256 bytes at %p\n", ptr2);

    void* ptr3 = Axis_AllocGeneral(alloc, 64, 8, AXIS_MEMORY_TAG_RENDERER);
    assert(ptr3 != nullptr);
    printf("  Allocated 64 bytes at %p\n", ptr3);

    // Write to memory to verify it works
    char* data = static_cast<char*>(ptr1);
    for (int i = 0; i < 128; ++i) {
        data[i] = static_cast<char>(i);
    }

    // Free memory
    Axis_FreeGeneral(alloc, ptr1);
    printf("  Freed ptr1\n");

    Axis_FreeGeneral(alloc, ptr2);
    printf("  Freed ptr2\n");

    Axis_FreeGeneral(alloc, ptr3);
    printf("  Freed ptr3\n");

    Axis_DestroyGeneralAllocator(alloc);
    printf("  General allocator test PASSED\n\n");
}

void TestPoolAllocator() {
    printf("=== Testing Pool Allocator ===\n");

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

    size_t free_count = Axis_GetPoolFreeCount(pool);
    assert(free_count == 10);
    printf("  Initial free count: %zu\n", free_count);

    // Allocate some objects
    TestObject* obj1 = static_cast<TestObject*>(Axis_AllocPool(pool));
    assert(obj1 != nullptr);
    obj1->id = 1;
    obj1->value = 1.0f;
    printf("  Allocated object 1 at %p\n", obj1);

    TestObject* obj2 = static_cast<TestObject*>(Axis_AllocPool(pool));
    assert(obj2 != nullptr);
    obj2->id = 2;
    obj2->value = 2.0f;
    printf("  Allocated object 2 at %p\n", obj2);

    free_count = Axis_GetPoolFreeCount(pool);
    assert(free_count == 8);
    printf("  Free count after 2 allocs: %zu\n", free_count);

    // Free one object
    Axis_FreePool(pool, obj1);
    printf("  Freed object 1\n");

    free_count = Axis_GetPoolFreeCount(pool);
    assert(free_count == 9);
    printf("  Free count after 1 free: %zu\n", free_count);

    // Allocate again (should reuse freed slot)
    TestObject* obj3 = static_cast<TestObject*>(Axis_AllocPool(pool));
    assert(obj3 != nullptr);
    printf("  Allocated object 3 at %p (reused)\n", obj3);

    // Free remaining objects
    Axis_FreePool(pool, obj2);
    Axis_FreePool(pool, obj3);

    Axis_DestroyPoolAllocator(pool);
    printf("  Pool allocator test PASSED\n\n");
}

void TestFrameAllocator() {
    printf("=== Testing Frame Allocator ===\n");

    AxisFrameAllocator* frame = Axis_CreateFrameAllocator(
        "TestFrame",
        1024,  // 1KB
        AXIS_MEMORY_TAG_TEMP
    );
    assert(frame != nullptr);

    size_t usage = Axis_GetFrameUsage(frame);
    assert(usage == 0);
    printf("  Initial usage: %zu bytes\n", usage);

    // Allocate some temporary data
    void* temp1 = Axis_AllocFrame(frame, 128, 16);
    assert(temp1 != nullptr);
    printf("  Allocated 128 bytes at %p\n", temp1);

    usage = Axis_GetFrameUsage(frame);
    printf("  Usage after alloc: %zu bytes\n", usage);

    void* temp2 = Axis_AllocFrame(frame, 256, 16);
    assert(temp2 != nullptr);
    printf("  Allocated 256 bytes at %p\n", temp2);

    usage = Axis_GetFrameUsage(frame);
    printf("  Usage after 2nd alloc: %zu bytes\n", usage);

    // Write to memory
    char* data = static_cast<char*>(temp1);
    for (int i = 0; i < 128; ++i) {
        data[i] = static_cast<char>(i);
    }

    // Reset (simulating end of frame)
    Axis_ResetFrameAllocator(frame);
    printf("  Reset frame allocator\n");

    usage = Axis_GetFrameUsage(frame);
    assert(usage == 0);
    printf("  Usage after reset: %zu bytes\n", usage);

    // Allocate again
    void* temp3 = Axis_AllocFrame(frame, 64, 16);
    assert(temp3 != nullptr);
    printf("  Allocated 64 bytes at %p (after reset)\n", temp3);

    Axis_DestroyFrameAllocator(frame);
    printf("  Frame allocator test PASSED\n\n");
}

void TestStatistics() {
    printf("=== Testing Statistics ===\n");

    AxisMemoryStats stats;
    AxisResult result = Axis_GetMemoryStats(&stats);
    assert(result == AXIS_OK);

    printf("  Total current bytes: %zu\n", stats.total_current_bytes);
    printf("  Total peak bytes: %zu\n", stats.total_peak_bytes);

    for (int i = 0; i < AXIS_MEMORY_TAG_COUNT; ++i) {
        AxisMemoryTagStats tag_stats;
        result = Axis_GetTagStats(static_cast<AxisMemoryTag>(i), &tag_stats);
        assert(result == AXIS_OK);

        if (tag_stats.total_allocations > 0) {
            printf("  Tag %d: current=%zu, peak=%zu, allocs=%zu, frees=%zu\n",
                i,
                tag_stats.current_bytes,
                tag_stats.peak_bytes,
                tag_stats.total_allocations,
                tag_stats.total_frees
            );
        }
    }

    printf("  Statistics test PASSED\n\n");
}

void RunMemoryTests() {
    printf("\n========================================\n");
    printf("AXIS Memory System Test\n");
    printf("========================================\n\n");

    // Initialize memory system
    AxisMemoryConfig config;
    config.general_reserve_bytes = 1024 * 1024;  // 1MB
    config.enable_statistics = 1;

    AxisResult result = Axis_InitializeMemory(&config);
    assert(result == AXIS_OK);
    printf("Memory system initialized\n\n");

    // Run tests
    TestGeneralAllocator();
    TestPoolAllocator();
    TestFrameAllocator();
    TestStatistics();

    // Shutdown
    result = Axis_ShutdownMemory();
    assert(result == AXIS_OK);
    printf("Memory system shutdown\n");

    printf("\n========================================\n");
    printf("All tests PASSED!\n");
    printf("========================================\n\n");
}
