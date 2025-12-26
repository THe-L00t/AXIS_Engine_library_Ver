/**
 * @file axis_memory.h
 * @brief AXIS Core Memory System Public API
 *
 * This header provides the public C API for the AXIS memory system.
 * All allocations in AXIS should go through these APIs for tracking and debugging.
 *
 * Design Philosophy:
 * - Memory is managed by lifetime, not by data structure
 * - All allocations are tracked and tagged
 * - Three allocator types: General, Pool, Frame
 * - Explicit initialization and shutdown
 */

#ifndef AXIS_MEMORY_H
#define AXIS_MEMORY_H

#include "axis_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Opaque Handles
// =============================================================================

/**
 * @brief Opaque handle to a general allocator
 *
 * General allocators handle variable-size, long-lifetime allocations.
 */
typedef struct AxisGeneralAllocator AxisGeneralAllocator;

/**
 * @brief Opaque handle to a pool allocator
 *
 * Pool allocators handle fixed-size objects with frequent allocation/deallocation.
 */
typedef struct AxisPoolAllocator AxisPoolAllocator;

/**
 * @brief Opaque handle to a frame allocator
 *
 * Frame allocators handle temporary allocations that live for one frame.
 * No individual free() - only reset() at frame end.
 */
typedef struct AxisFrameAllocator AxisFrameAllocator;

// =============================================================================
// Memory System Initialization
// =============================================================================

/**
 * @brief Configuration for memory system initialization
 */
typedef struct AxisMemoryConfig {
    size_t general_reserve_bytes;  /** Initial reserve for general allocator */
    int enable_statistics;         /** Enable statistics collection (0 or 1) */
} AxisMemoryConfig;

/**
 * @brief Initialize the AXIS memory system
 *
 * This must be called before any memory allocation.
 *
 * @param config Configuration settings. If NULL, uses defaults.
 * @return AXIS_OK on success, error code otherwise
 */
AXIS_API AxisResult Axis_InitializeMemory(const AxisMemoryConfig* config);

/**
 * @brief Shutdown the AXIS memory system
 *
 * Frees all allocators and reports leaks.
 *
 * @return AXIS_OK on success
 */
AXIS_API AxisResult Axis_ShutdownMemory(void);

// =============================================================================
// General Allocator
// =============================================================================

/**
 * @brief Create a general allocator
 *
 * @param name Debug name for this allocator (not copied, must remain valid)
 * @param reserve_bytes Initial capacity to reserve
 * @return Allocator handle, or NULL on failure
 */
AXIS_API AxisGeneralAllocator* Axis_CreateGeneralAllocator(
    const char* name,
    size_t reserve_bytes
);

/**
 * @brief Destroy a general allocator
 *
 * Frees all memory. Any pointers allocated from this allocator become invalid.
 *
 * @param allocator Allocator to destroy. Must not be NULL.
 */
AXIS_API void Axis_DestroyGeneralAllocator(AxisGeneralAllocator* allocator);

/**
 * @brief Allocate memory from a general allocator
 *
 * @param allocator Allocator to use. Must not be NULL.
 * @param size Number of bytes to allocate. Must be > 0.
 * @param alignment Alignment requirement (must be power of 2). If 0, uses default (16).
 * @param tag Memory tag for tracking
 * @return Pointer to allocated memory, or NULL on failure
 * @note Caller must free with Axis_FreeGeneral()
 */
AXIS_API void* Axis_AllocGeneral(
    AxisGeneralAllocator* allocator,
    size_t size,
    size_t alignment,
    AxisMemoryTag tag
);

/**
 * @brief Free memory allocated by a general allocator
 *
 * @param allocator Allocator that allocated the memory. Must not be NULL.
 * @param ptr Pointer to free. Must not be NULL.
 */
AXIS_API void Axis_FreeGeneral(
    AxisGeneralAllocator* allocator,
    void* ptr
);

// =============================================================================
// Pool Allocator
// =============================================================================

/**
 * @brief Create a pool allocator
 *
 * @param name Debug name (not copied, must remain valid)
 * @param object_size Size of each object in bytes
 * @param object_count Initial number of objects to allocate
 * @param tag Memory tag for tracking
 * @return Allocator handle, or NULL on failure
 */
AXIS_API AxisPoolAllocator* Axis_CreatePoolAllocator(
    const char* name,
    size_t object_size,
    size_t object_count,
    AxisMemoryTag tag
);

/**
 * @brief Destroy a pool allocator
 *
 * Frees all memory. Any pointers allocated from this pool become invalid.
 *
 * @param allocator Allocator to destroy. Must not be NULL.
 */
AXIS_API void Axis_DestroyPoolAllocator(AxisPoolAllocator* allocator);

/**
 * @brief Allocate an object from a pool
 *
 * @param allocator Pool allocator. Must not be NULL.
 * @return Pointer to allocated object, or NULL if pool is exhausted
 * @note Caller must free with Axis_FreePool()
 */
AXIS_API void* Axis_AllocPool(AxisPoolAllocator* allocator);

/**
 * @brief Free an object back to a pool
 *
 * @param allocator Pool allocator. Must not be NULL.
 * @param ptr Pointer to object. Must not be NULL and must be from this pool.
 */
AXIS_API void Axis_FreePool(AxisPoolAllocator* allocator, void* ptr);

/**
 * @brief Get the number of free objects in a pool
 *
 * @param allocator Pool allocator. Must not be NULL.
 * @return Number of available objects
 */
AXIS_API size_t Axis_GetPoolFreeCount(const AxisPoolAllocator* allocator);

// =============================================================================
// Frame Allocator
// =============================================================================

/**
 * @brief Create a frame allocator
 *
 * @param name Debug name (not copied, must remain valid)
 * @param capacity_bytes Total capacity in bytes
 * @param tag Memory tag for tracking
 * @return Allocator handle, or NULL on failure
 */
AXIS_API AxisFrameAllocator* Axis_CreateFrameAllocator(
    const char* name,
    size_t capacity_bytes,
    AxisMemoryTag tag
);

/**
 * @brief Destroy a frame allocator
 *
 * @param allocator Allocator to destroy. Must not be NULL.
 */
AXIS_API void Axis_DestroyFrameAllocator(AxisFrameAllocator* allocator);

/**
 * @brief Allocate temporary memory from a frame allocator
 *
 * @param allocator Frame allocator. Must not be NULL.
 * @param size Number of bytes to allocate. Must be > 0.
 * @param alignment Alignment requirement (power of 2). If 0, uses default (16).
 * @return Pointer to allocated memory, or NULL if capacity exceeded
 * @note No individual free. Call Axis_ResetFrameAllocator() at frame end.
 */
AXIS_API void* Axis_AllocFrame(
    AxisFrameAllocator* allocator,
    size_t size,
    size_t alignment
);

/**
 * @brief Reset a frame allocator
 *
 * Marks all memory as free. Pointers allocated from this frame become invalid.
 *
 * @param allocator Frame allocator. Must not be NULL.
 */
AXIS_API void Axis_ResetFrameAllocator(AxisFrameAllocator* allocator);

/**
 * @brief Get current usage of a frame allocator
 *
 * @param allocator Frame allocator. Must not be NULL.
 * @return Number of bytes currently allocated
 */
AXIS_API size_t Axis_GetFrameUsage(const AxisFrameAllocator* allocator);

// =============================================================================
// Statistics
// =============================================================================

/**
 * @brief Get overall memory statistics
 *
 * @param out_stats Output statistics structure. Must not be NULL.
 * @return AXIS_OK on success, error code otherwise
 */
AXIS_API AxisResult Axis_GetMemoryStats(AxisMemoryStats* out_stats);

/**
 * @brief Get statistics for a specific tag
 *
 * @param tag Memory tag to query
 * @param out_stats Output statistics structure. Must not be NULL.
 * @return AXIS_OK on success, error code otherwise
 */
AXIS_API AxisResult Axis_GetTagStats(
    AxisMemoryTag tag,
    AxisMemoryTagStats* out_stats
);

#ifdef __cplusplus
}
#endif

#endif // AXIS_MEMORY_H
