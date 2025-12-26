/**
 * @file general_allocator.h
 * @brief General purpose allocator for variable-size, long-lifetime allocations
 */

#ifndef AXIS_GENERAL_ALLOCATOR_H
#define AXIS_GENERAL_ALLOCATOR_H

#include "axis/core/axis_memory.h"
#include "memory_internal.h"
#include <unordered_map>
#include <mutex>

namespace axis::core::memory {

/**
 * @brief General allocator implementation
 *
 * This allocator wraps the system allocator (malloc/free) and provides:
 * - Memory tracking per tag
 * - Statistics collection
 * - Debug information
 *
 * Thread-safety: Thread-safe for concurrent allocations
 */
class GeneralAllocator {
public:
    /**
     * @brief Construct a general allocator
     * @param name Debug name for this allocator
     * @param reserve_bytes Hint for initial capacity (currently unused)
     */
    GeneralAllocator(const char* name, size_t reserve_bytes);

    /**
     * @brief Destructor
     *
     * Frees all tracked allocations and reports leaks in debug builds.
     */
    ~GeneralAllocator();

    // Disable copy/move
    GeneralAllocator(const GeneralAllocator&) = delete;
    GeneralAllocator& operator=(const GeneralAllocator&) = delete;

    /**
     * @brief Allocate memory
     * @param size Size in bytes
     * @param alignment Alignment requirement (power of 2)
     * @param tag Memory tag for tracking
     * @return Allocated pointer, or nullptr on failure
     */
    void* Allocate(size_t size, size_t alignment, AxisMemoryTag tag);

    /**
     * @brief Free memory
     * @param ptr Pointer to free
     */
    void Free(void* ptr);

    /**
     * @brief Get the name of this allocator
     */
    const char* GetName() const { return name_; }

private:
    struct AllocationHeader {
        size_t size;
        size_t alignment;
        AxisMemoryTag tag;
    };

    const char* name_;
    size_t reserve_bytes_;
    std::mutex mutex_;
    std::unordered_map<void*, AllocationHeader> allocations_;
};

} // namespace axis::core::memory

// =============================================================================
// C API Implementation Structure
// =============================================================================

struct AxisGeneralAllocator {
    axis::core::memory::GeneralAllocator* impl;
};

#endif // AXIS_GENERAL_ALLOCATOR_H
