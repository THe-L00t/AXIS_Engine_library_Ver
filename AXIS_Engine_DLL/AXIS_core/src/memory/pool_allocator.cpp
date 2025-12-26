/**
 * @file pool_allocator.cpp
 * @brief Implementation of the pool allocator
 */

#include "pch.h"
#include "pool_allocator.h"
#include <cstdlib>
#include <cassert>
#include <algorithm>

namespace axis::core::memory {

PoolAllocator::PoolAllocator(const char* name, size_t object_size, size_t object_count, AxisMemoryTag tag)
    : name_(name)
    , object_size_(object_size)
    , object_count_(object_count)
    , tag_(tag)
    , pool_memory_(nullptr)
    , free_list_(nullptr)
    , free_count_(0) {

    // Ensure chunk size is at least sizeof(FreeNode) and properly aligned
    chunk_size_ = std::max(object_size, sizeof(FreeNode));
    chunk_size_ = AlignUp(chunk_size_, DEFAULT_ALIGNMENT);

    // Allocate the entire pool
    size_t total_size = chunk_size_ * object_count;
    pool_memory_ = std::malloc(total_size);

    if (!pool_memory_) {
        return;
    }

    // Initialize free list - link all chunks together
    free_list_ = nullptr;
    uint8_t* chunk = static_cast<uint8_t*>(pool_memory_);

    for (size_t i = 0; i < object_count; ++i) {
        FreeNode* node = reinterpret_cast<FreeNode*>(chunk);
        node->next = free_list_;
        free_list_ = node;
        chunk += chunk_size_;
    }

    free_count_ = object_count;

    // Record allocation in statistics
    auto& state = GetMemoryState();
    if (state.statistics_enabled) {
        state.stats_tracker.RecordAllocation(tag, total_size);
    }
}

PoolAllocator::~PoolAllocator() {
    if (pool_memory_) {
        // Record deallocation in statistics
        auto& state = GetMemoryState();
        if (state.statistics_enabled) {
            size_t total_size = chunk_size_ * object_count_;
            state.stats_tracker.RecordDeallocation(tag_, total_size);
        }

        std::free(pool_memory_);
        pool_memory_ = nullptr;
    }
}

void* PoolAllocator::Allocate() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!free_list_) {
        // Pool exhausted
        return nullptr;
    }

    // Pop from free list
    FreeNode* node = free_list_;
    free_list_ = node->next;
    --free_count_;

    return node;
}

void PoolAllocator::Free(void* ptr) {
    if (!ptr) {
        return;
    }

    // Validate that ptr is within the pool bounds
    uintptr_t pool_start = reinterpret_cast<uintptr_t>(pool_memory_);
    uintptr_t pool_end = pool_start + (chunk_size_ * object_count_);
    uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);

    if (ptr_addr < pool_start || ptr_addr >= pool_end) {
        assert(false && "Attempt to free pointer not from this pool");
        return;
    }

    // Validate alignment
    if ((ptr_addr - pool_start) % chunk_size_ != 0) {
        assert(false && "Attempt to free misaligned pointer");
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Push to free list
    FreeNode* node = static_cast<FreeNode*>(ptr);
    node->next = free_list_;
    free_list_ = node;
    ++free_count_;
}

size_t PoolAllocator::GetFreeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return free_count_;
}

} // namespace axis::core::memory
