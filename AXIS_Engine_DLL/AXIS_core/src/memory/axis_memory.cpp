/**
 * @file axis_memory.cpp
 * @brief Implementation of the AXIS memory system public C API
 */

#include "pch.h"
#include "axis/core/axis_memory.h"
#include "memory_internal.h"
#include "general_allocator.h"
#include "pool_allocator.h"
#include "frame_allocator.h"

using namespace axis::core::memory;

// =============================================================================
// Memory System Initialization
// =============================================================================

AxisResult Axis_InitializeMemory(const AxisMemoryConfig* config) {
    auto& state = GetMemoryState();

    std::lock_guard<std::mutex> lock(state.mutex);

    if (state.initialized) {
        return AXIS_ERROR_ALREADY_INITIALIZED;
    }

    // Apply configuration
    if (config) {
        state.statistics_enabled = config->enable_statistics != 0;
    } else {
        // Default configuration
        state.statistics_enabled = true;
    }

    // Reset statistics
    state.stats_tracker.Reset();

    state.initialized = true;

    return AXIS_OK;
}

AxisResult Axis_ShutdownMemory(void) {
    auto& state = GetMemoryState();

    std::lock_guard<std::mutex> lock(state.mutex);

    if (!state.initialized) {
        return AXIS_ERROR_NOT_INITIALIZED;
    }

    // In a real implementation, we would check for leaks and report them
    // For now, we just mark as not initialized

    state.initialized = false;

    return AXIS_OK;
}

// =============================================================================
// General Allocator
// =============================================================================

AxisGeneralAllocator* Axis_CreateGeneralAllocator(const char* name, size_t reserve_bytes) {
    if (!name) {
        return nullptr;
    }

    auto& state = GetMemoryState();
    if (!state.initialized) {
        return nullptr;
    }

    auto* allocator = new AxisGeneralAllocator();
    allocator->impl = new GeneralAllocator(name, reserve_bytes);

    return allocator;
}

void Axis_DestroyGeneralAllocator(AxisGeneralAllocator* allocator) {
    if (!allocator) {
        return;
    }

    delete allocator->impl;
    delete allocator;
}

void* Axis_AllocGeneral(
    AxisGeneralAllocator* allocator,
    size_t size,
    size_t alignment,
    AxisMemoryTag tag) {

    if (!allocator || !allocator->impl) {
        return nullptr;
    }

    return allocator->impl->Allocate(size, alignment, tag);
}

void Axis_FreeGeneral(AxisGeneralAllocator* allocator, void* ptr) {
    if (!allocator || !allocator->impl) {
        return;
    }

    allocator->impl->Free(ptr);
}

// =============================================================================
// Pool Allocator
// =============================================================================

AxisPoolAllocator* Axis_CreatePoolAllocator(
    const char* name,
    size_t object_size,
    size_t object_count,
    AxisMemoryTag tag) {

    if (!name || object_size == 0 || object_count == 0) {
        return nullptr;
    }

    auto& state = GetMemoryState();
    if (!state.initialized) {
        return nullptr;
    }

    auto* allocator = new AxisPoolAllocator();
    allocator->impl = new PoolAllocator(name, object_size, object_count, tag);

    return allocator;
}

void Axis_DestroyPoolAllocator(AxisPoolAllocator* allocator) {
    if (!allocator) {
        return;
    }

    delete allocator->impl;
    delete allocator;
}

void* Axis_AllocPool(AxisPoolAllocator* allocator) {
    if (!allocator || !allocator->impl) {
        return nullptr;
    }

    return allocator->impl->Allocate();
}

void Axis_FreePool(AxisPoolAllocator* allocator, void* ptr) {
    if (!allocator || !allocator->impl) {
        return;
    }

    allocator->impl->Free(ptr);
}

size_t Axis_GetPoolFreeCount(const AxisPoolAllocator* allocator) {
    if (!allocator || !allocator->impl) {
        return 0;
    }

    return allocator->impl->GetFreeCount();
}

// =============================================================================
// Frame Allocator
// =============================================================================

AxisFrameAllocator* Axis_CreateFrameAllocator(
    const char* name,
    size_t capacity_bytes,
    AxisMemoryTag tag) {

    if (!name || capacity_bytes == 0) {
        return nullptr;
    }

    auto& state = GetMemoryState();
    if (!state.initialized) {
        return nullptr;
    }

    auto* allocator = new AxisFrameAllocator();
    allocator->impl = new FrameAllocator(name, capacity_bytes, tag);

    return allocator;
}

void Axis_DestroyFrameAllocator(AxisFrameAllocator* allocator) {
    if (!allocator) {
        return;
    }

    delete allocator->impl;
    delete allocator;
}

void* Axis_AllocFrame(
    AxisFrameAllocator* allocator,
    size_t size,
    size_t alignment) {

    if (!allocator || !allocator->impl) {
        return nullptr;
    }

    return allocator->impl->Allocate(size, alignment);
}

void Axis_ResetFrameAllocator(AxisFrameAllocator* allocator) {
    if (!allocator || !allocator->impl) {
        return;
    }

    allocator->impl->Reset();
}

size_t Axis_GetFrameUsage(const AxisFrameAllocator* allocator) {
    if (!allocator || !allocator->impl) {
        return 0;
    }

    return allocator->impl->GetUsage();
}

// =============================================================================
// Statistics
// =============================================================================

AxisResult Axis_GetMemoryStats(AxisMemoryStats* out_stats) {
    if (!out_stats) {
        return AXIS_ERROR_INVALID_PARAMETER;
    }

    auto& state = GetMemoryState();

    if (!state.initialized) {
        return AXIS_ERROR_NOT_INITIALIZED;
    }

    state.stats_tracker.GetOverallStats(out_stats);

    return AXIS_OK;
}

AxisResult Axis_GetTagStats(AxisMemoryTag tag, AxisMemoryTagStats* out_stats) {
    if (!out_stats || tag >= AXIS_MEMORY_TAG_COUNT) {
        return AXIS_ERROR_INVALID_PARAMETER;
    }

    auto& state = GetMemoryState();

    if (!state.initialized) {
        return AXIS_ERROR_NOT_INITIALIZED;
    }

    state.stats_tracker.GetTagStats(tag, out_stats);

    return AXIS_OK;
}
