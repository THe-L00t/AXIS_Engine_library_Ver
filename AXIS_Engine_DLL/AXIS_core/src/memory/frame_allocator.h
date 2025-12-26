/**
 * @file frame_allocator.h
 * @brief Frame allocator for temporary, single-frame allocations
 */

#ifndef AXIS_FRAME_ALLOCATOR_H
#define AXIS_FRAME_ALLOCATOR_H

#include "axis/core/axis_memory.h"
#include "memory_internal.h"
#include <mutex>

namespace axis::core::memory {

/**
 * @brief Frame allocator implementation
 *
 * This allocator uses a bump-pointer strategy for ultra-fast allocations.
 * All memory is freed at once with Reset() - no individual free().
 *
 * Memory layout:
 * - Single contiguous buffer
 * - Bump pointer advances on each allocation
 * - Reset moves pointer back to start
 *
 * Use case:
 * - Temporary data that lives for one frame
 * - Render commands
 * - Temporary calculations
 *
 * Thread-safety: Thread-safe for concurrent operations
 */
class FrameAllocator {
public:
    /**
     * @brief Construct a frame allocator
     * @param name Debug name
     * @param capacity_bytes Total capacity in bytes
     * @param tag Memory tag for tracking
     */
    FrameAllocator(const char* name, size_t capacity_bytes, AxisMemoryTag tag);

    /**
     * @brief Destructor
     */
    ~FrameAllocator();

    // Disable copy/move
    FrameAllocator(const FrameAllocator&) = delete;
    FrameAllocator& operator=(const FrameAllocator&) = delete;

    /**
     * @brief Allocate temporary memory
     * @param size Number of bytes to allocate
     * @param alignment Alignment requirement (power of 2)
     * @return Pointer to memory, or nullptr if capacity exceeded
     */
    void* Allocate(size_t size, size_t alignment);

    /**
     * @brief Reset the allocator
     *
     * All allocated pointers become invalid.
     * Call this at the end of each frame.
     */
    void Reset();

    /**
     * @brief Get current usage
     * @return Number of bytes currently allocated
     */
    size_t GetUsage() const;

    /**
     * @brief Get capacity
     * @return Total capacity in bytes
     */
    size_t GetCapacity() const { return capacity_; }

    /**
     * @brief Get peak usage since last reset
     * @return Peak bytes used
     */
    size_t GetPeakUsage() const;

    /**
     * @brief Get the name of this allocator
     */
    const char* GetName() const { return name_; }

private:
    const char* name_;
    size_t capacity_;
    AxisMemoryTag tag_;
    void* buffer_;
    size_t current_offset_;
    size_t peak_offset_;
    mutable std::mutex mutex_;
};

} // namespace axis::core::memory

// =============================================================================
// C API Implementation Structure
// =============================================================================

struct AxisFrameAllocator {
    axis::core::memory::FrameAllocator* impl;
};

#endif // AXIS_FRAME_ALLOCATOR_H
