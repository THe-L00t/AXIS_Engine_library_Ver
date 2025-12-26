/**
 * @file frame_allocator.cpp
 * @brief Implementation of the frame allocator
 */

#include "pch.h"
#include "frame_allocator.h"
#include <cstdlib>
#include <cassert>

namespace axis::core::memory {

FrameAllocator::FrameAllocator(const char* name, size_t capacity_bytes, AxisMemoryTag tag)
    : name_(name)
    , capacity_(capacity_bytes)
    , tag_(tag)
    , buffer_(nullptr)
    , current_offset_(0)
    , peak_offset_(0) {

    // Allocate the buffer
    buffer_ = std::malloc(capacity_);

    if (!buffer_) {
        capacity_ = 0;
        return;
    }

    // Record allocation in statistics
    auto& state = GetMemoryState();
    if (state.statistics_enabled) {
        state.stats_tracker.RecordAllocation(tag, capacity_);
    }
}

FrameAllocator::~FrameAllocator() {
    if (buffer_) {
        // Record deallocation in statistics
        auto& state = GetMemoryState();
        if (state.statistics_enabled) {
            state.stats_tracker.RecordDeallocation(tag_, capacity_);
        }

        std::free(buffer_);
        buffer_ = nullptr;
    }
}

void* FrameAllocator::Allocate(size_t size, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }

    if (alignment == 0) {
        alignment = DEFAULT_ALIGNMENT;
    }

    if (!IsPowerOfTwo(alignment)) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (!buffer_) {
        return nullptr;
    }

    // Calculate aligned offset
    size_t aligned_offset = AlignUp(current_offset_, alignment);

    // Check if we have enough space
    if (aligned_offset + size > capacity_) {
        // Capacity exceeded
        return nullptr;
    }

    // Calculate pointer
    uintptr_t buffer_addr = reinterpret_cast<uintptr_t>(buffer_);
    void* ptr = reinterpret_cast<void*>(buffer_addr + aligned_offset);

    // Update offset
    current_offset_ = aligned_offset + size;

    // Update peak
    if (current_offset_ > peak_offset_) {
        peak_offset_ = current_offset_;
    }

    return ptr;
}

void FrameAllocator::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_offset_ = 0;
    // Note: We keep peak_offset_ to track high-water mark
}

size_t FrameAllocator::GetUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_offset_;
}

size_t FrameAllocator::GetPeakUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peak_offset_;
}

} // namespace axis::core::memory
