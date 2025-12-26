/**
 * @file memory_internal.h
 * @brief Internal memory system utilities and statistics tracking
 *
 * This header is for internal use only. It provides utilities for
 * memory tracking, statistics, and common helpers.
 */

#ifndef AXIS_MEMORY_INTERNAL_H
#define AXIS_MEMORY_INTERNAL_H

#include "axis/core/axis_core_types.h"
#include "axis/core/axis_memory.h"
#include <stdint.h>
#include <atomic>
#include <mutex>

namespace axis::core::memory {

// =============================================================================
// Memory Alignment Utilities
// =============================================================================

/**
 * @brief Align a value up to the given alignment
 * @param value Value to align
 * @param alignment Alignment (must be power of 2)
 * @return Aligned value
 */
inline size_t AlignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Check if a value is aligned
 * @param value Value to check
 * @param alignment Alignment (must be power of 2)
 * @return true if aligned
 */
inline bool IsAligned(size_t value, size_t alignment) {
    return (value & (alignment - 1)) == 0;
}

/**
 * @brief Check if a value is a power of 2
 */
inline bool IsPowerOfTwo(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/**
 * @brief Default alignment for allocations
 */
constexpr size_t DEFAULT_ALIGNMENT = 16;

// =============================================================================
// Memory Statistics Tracker
// =============================================================================

/**
 * @brief Thread-safe statistics tracker for memory allocations
 *
 * This class tracks allocation statistics per memory tag.
 * It uses atomics for thread-safe updates.
 */
class MemoryStatsTracker {
public:
    MemoryStatsTracker();
    ~MemoryStatsTracker() = default;

    /**
     * @brief Record an allocation
     * @param tag Memory tag
     * @param size Bytes allocated
     */
    void RecordAllocation(AxisMemoryTag tag, size_t size);

    /**
     * @brief Record a deallocation
     * @param tag Memory tag
     * @param size Bytes freed
     */
    void RecordDeallocation(AxisMemoryTag tag, size_t size);

    /**
     * @brief Get statistics for a specific tag
     * @param tag Memory tag
     * @param out_stats Output statistics
     */
    void GetTagStats(AxisMemoryTag tag, AxisMemoryTagStats* out_stats) const;

    /**
     * @brief Get overall statistics
     * @param out_stats Output statistics
     */
    void GetOverallStats(AxisMemoryStats* out_stats) const;

    /**
     * @brief Reset all statistics
     */
    void Reset();

private:
    struct TagStats {
        std::atomic<size_t> current_bytes{0};
        std::atomic<size_t> peak_bytes{0};
        std::atomic<size_t> total_allocations{0};
        std::atomic<size_t> total_frees{0};
    };

    TagStats tag_stats_[AXIS_MEMORY_TAG_COUNT];
};

// =============================================================================
// Global Memory State
// =============================================================================

/**
 * @brief Global memory system state
 *
 * This struct holds the global state of the memory system.
 * Access should be synchronized.
 */
struct MemorySystemState {
    bool initialized{false};
    bool statistics_enabled{false};
    MemoryStatsTracker stats_tracker;
    std::mutex mutex;  // Protects global operations
};

/**
 * @brief Get the global memory system state
 * @return Reference to the global state
 */
MemorySystemState& GetMemoryState();

} // namespace axis::core::memory

#endif // AXIS_MEMORY_INTERNAL_H
