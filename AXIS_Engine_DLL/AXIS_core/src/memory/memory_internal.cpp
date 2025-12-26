/**
 * @file memory_internal.cpp
 * @brief Implementation of internal memory utilities
 */

#include "pch.h"
#include "memory_internal.h"
#include <algorithm>

namespace axis::core::memory {

// =============================================================================
// MemoryStatsTracker Implementation
// =============================================================================

MemoryStatsTracker::MemoryStatsTracker() {
    Reset();
}

void MemoryStatsTracker::RecordAllocation(AxisMemoryTag tag, size_t size) {
    if (tag >= AXIS_MEMORY_TAG_COUNT) {
        return;
    }

    auto& stats = tag_stats_[tag];

    // Update current bytes
    size_t current = stats.current_bytes.fetch_add(size, std::memory_order_relaxed) + size;

    // Update peak bytes
    size_t peak = stats.peak_bytes.load(std::memory_order_relaxed);
    while (current > peak) {
        if (stats.peak_bytes.compare_exchange_weak(peak, current, std::memory_order_release, std::memory_order_relaxed)) {
            break;
        }
    }

    // Update allocation count
    stats.total_allocations.fetch_add(1, std::memory_order_relaxed);
}

void MemoryStatsTracker::RecordDeallocation(AxisMemoryTag tag, size_t size) {
    if (tag >= AXIS_MEMORY_TAG_COUNT) {
        return;
    }

    auto& stats = tag_stats_[tag];

    // Update current bytes
    stats.current_bytes.fetch_sub(size, std::memory_order_relaxed);

    // Update free count
    stats.total_frees.fetch_add(1, std::memory_order_relaxed);
}

void MemoryStatsTracker::GetTagStats(AxisMemoryTag tag, AxisMemoryTagStats* out_stats) const {
    if (tag >= AXIS_MEMORY_TAG_COUNT || !out_stats) {
        return;
    }

    const auto& stats = tag_stats_[tag];

    out_stats->current_bytes = stats.current_bytes.load(std::memory_order_relaxed);
    out_stats->peak_bytes = stats.peak_bytes.load(std::memory_order_relaxed);
    out_stats->total_allocations = stats.total_allocations.load(std::memory_order_relaxed);
    out_stats->total_frees = stats.total_frees.load(std::memory_order_relaxed);
}

void MemoryStatsTracker::GetOverallStats(AxisMemoryStats* out_stats) const {
    if (!out_stats) {
        return;
    }

    size_t total_current = 0;
    size_t total_peak = 0;

    for (int i = 0; i < AXIS_MEMORY_TAG_COUNT; ++i) {
        GetTagStats(static_cast<AxisMemoryTag>(i), &out_stats->tags[i]);
        total_current += out_stats->tags[i].current_bytes;
        total_peak += out_stats->tags[i].peak_bytes;
    }

    out_stats->total_current_bytes = total_current;
    out_stats->total_peak_bytes = total_peak;
}

void MemoryStatsTracker::Reset() {
    for (auto& stats : tag_stats_) {
        stats.current_bytes.store(0, std::memory_order_relaxed);
        stats.peak_bytes.store(0, std::memory_order_relaxed);
        stats.total_allocations.store(0, std::memory_order_relaxed);
        stats.total_frees.store(0, std::memory_order_relaxed);
    }
}

// =============================================================================
// Global Memory State
// =============================================================================

MemorySystemState& GetMemoryState() {
    static MemorySystemState state;
    return state;
}

} // namespace axis::core::memory
