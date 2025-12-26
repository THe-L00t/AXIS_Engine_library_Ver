/**
 * @file pool_allocator.h
 * @brief Pool allocator for fixed-size objects with frequent allocation/deallocation
 */

#ifndef AXIS_POOL_ALLOCATOR_H
#define AXIS_POOL_ALLOCATOR_H

#include "axis/core/axis_memory.h"
#include "memory_internal.h"
#include <mutex>

namespace axis::core::memory {

/**
 * @brief Pool allocator implementation
 *
 * This allocator manages fixed-size objects using a free-list.
 * It provides O(1) allocation and deallocation.
 *
 * Memory layout:
 * - Allocates a large block of memory upfront
 * - Divides it into fixed-size chunks
 * - Uses intrusive free-list (stores next pointer in free chunks)
 *
 * Thread-safety: Thread-safe for concurrent operations
 */
class PoolAllocator {
public:
    /**
     * @brief Construct a pool allocator
     * @param name Debug name
     * @param object_size Size of each object in bytes
     * @param object_count Number of objects to allocate
     * @param tag Memory tag for tracking
     */
    PoolAllocator(const char* name, size_t object_size, size_t object_count, AxisMemoryTag tag);

    /**
     * @brief Destructor
     *
     * Frees the entire pool. All allocated objects become invalid.
     */
    ~PoolAllocator();

    // Disable copy/move
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    /**
     * @brief Allocate an object from the pool
     * @return Pointer to object, or nullptr if pool is exhausted
     */
    void* Allocate();

    /**
     * @brief Free an object back to the pool
     * @param ptr Pointer to object (must be from this pool)
     */
    void Free(void* ptr);

    /**
     * @brief Get the number of free objects
     * @return Number of available objects
     */
    size_t GetFreeCount() const;

    /**
     * @brief Get the object size
     */
    size_t GetObjectSize() const { return object_size_; }

    /**
     * @brief Get the name of this allocator
     */
    const char* GetName() const { return name_; }

private:
    // Free-list node (stored in free chunks)
    struct FreeNode {
        FreeNode* next;
    };

    const char* name_;
    size_t object_size_;
    size_t object_count_;
    size_t chunk_size_;  // Actual size of each chunk (>= object_size for alignment)
    AxisMemoryTag tag_;
    void* pool_memory_;
    FreeNode* free_list_;
    mutable std::mutex mutex_;
    size_t free_count_;
};

} // namespace axis::core::memory

// =============================================================================
// C API Implementation Structure
// =============================================================================

struct AxisPoolAllocator {
    axis::core::memory::PoolAllocator* impl;
};

#endif // AXIS_POOL_ALLOCATOR_H
