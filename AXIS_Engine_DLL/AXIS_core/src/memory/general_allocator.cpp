/**
 * @file general_allocator.cpp
 * @brief Implementation of the general allocator
 */

#include "pch.h"
#include "general_allocator.h"
#include "axis/core/axis_assert.h"
#include <cstdlib>

namespace axis::core::memory {

GeneralAllocator::GeneralAllocator(const char* name, size_t reserve_bytes)
    : name_(name)
    , reserve_bytes_(reserve_bytes) {
}

GeneralAllocator::~GeneralAllocator() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Report leaks in debug builds
    if (!allocations_.empty()) {
        // In a real implementation, this would use the logging system
        // For now, we just free the memory
        for (auto& [ptr, header] : allocations_) {
            auto& state = GetMemoryState();
            if (state.statistics_enabled) {
                state.stats_tracker.RecordDeallocation(header.tag, header.size);
            }
            std::free(ptr);
        }
        allocations_.clear();
    }
}

void* GeneralAllocator::Allocate(size_t size, size_t alignment, AxisMemoryTag tag) {
    if (size == 0) {
        return nullptr;
    }

    if (alignment == 0) {
        alignment = DEFAULT_ALIGNMENT;
    }

    if (!IsPowerOfTwo(alignment)) {
        return nullptr;
    }

    // Allocate with alignment
    // Layout: [raw_ptr_space][padding][raw_ptr_address][aligned_data]
    // We need space for: data + alignment + sizeof(void*) to store raw pointer
    size_t total_size = size + alignment + sizeof(void*);
    void* raw_ptr = std::malloc(total_size);

    if (!raw_ptr) {
        return nullptr;
    }

    // Calculate aligned address after storing the raw pointer
    uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw_ptr);
    uintptr_t aligned_addr = AlignUp(raw_addr + sizeof(void*), alignment);
    void* aligned_ptr = reinterpret_cast<void*>(aligned_addr);

    // Store raw pointer just before the aligned pointer
    void** ptr_before_aligned = reinterpret_cast<void**>(aligned_addr - sizeof(void*));
    *ptr_before_aligned = raw_ptr;

    // Store header information
    AllocationHeader header;
    header.size = size;
    header.alignment = alignment;
    header.tag = tag;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_[aligned_ptr] = header;
    }

    // Update statistics
    auto& state = GetMemoryState();
    if (state.statistics_enabled) {
        state.stats_tracker.RecordAllocation(tag, size);
    }

    return aligned_ptr;
}

void GeneralAllocator::Free(void* ptr) {
    if (!ptr) {
        return;
    }

    AllocationHeader header;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = allocations_.find(ptr);
        if (it == allocations_.end()) {
            // Invalid pointer - not allocated by this allocator
            AXIS_ASSERT_MSG(false, "Attempt to free invalid pointer");
            return;
        }

        header = it->second;
        allocations_.erase(it);
    }

    // Retrieve the raw pointer stored before the aligned pointer
    uintptr_t aligned_addr = reinterpret_cast<uintptr_t>(ptr);
    void** ptr_before_aligned = reinterpret_cast<void**>(aligned_addr - sizeof(void*));
    void* raw_ptr = *ptr_before_aligned;

    // Update statistics
    auto& state = GetMemoryState();
    if (state.statistics_enabled) {
        state.stats_tracker.RecordDeallocation(header.tag, header.size);
    }

    std::free(raw_ptr);
}

} // namespace axis::core::memory
