/**
 * @file slot_internal.h
 * @brief Internal definitions for Time Slot management
 *
 * This header contains internal data structures and functions for the
 * Single Time Axis system. Not part of the public API.
 *
 * Design Intent:
 * - Requests are stored in a queue until their target slot is reached
 * - When a slot is processed, requests are grouped by conflict group
 * - Each group is resolved independently (parallelizable)
 * - Results are committed in deterministic order
 */

#ifndef AXIS_TIME_SLOT_INTERNAL_H
#define AXIS_TIME_SLOT_INTERNAL_H

#include "../include/axis/time/axis_time_slot_types.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <functional>
#include <memory>

namespace axis::time::internal {

// =============================================================================
// Internal Request Storage
// =============================================================================

/**
 * @brief Internal representation of a state change request
 */
struct PendingRequest {
    AxisRequestId           id;
    AxisStateChangeDesc     desc;
    bool                    cancelled;
};

// =============================================================================
// Conflict Group Internal
// =============================================================================

/**
 * @brief Internal representation of a conflict group
 */
struct ConflictGroupData {
    AxisConflictGroupId     id;
    AxisConflictPolicy      policy;
    AxisCustomPolicyFn      custom_fn;
    void*                   custom_user_data;
    bool                    active;
};

// =============================================================================
// Anchor Storage
// =============================================================================

/**
 * @brief Anchor data for state reconstruction
 *
 * Anchors store a compact summary of state at fixed intervals.
 * Between anchors, state is derived algorithmically.
 */
struct AnchorData {
    AxisSlotIndex           slot_index;
    AxisReconstructionKey   key;

    // Compact state summary: map of keys to values at this anchor point
    // This is bounded by the number of unique keys, not by time
    std::unordered_map<uint64_t, AxisStateValue> state_snapshot;

    // Hash of all changes from previous anchor to this one (for verification)
    uint64_t                transition_hash;
};

// =============================================================================
// Resolution Result
// =============================================================================

/**
 * @brief Result of resolving a conflict group for a single slot
 */
struct GroupResolutionResult {
    AxisConflictGroupId                     group_id;
    std::vector<std::pair<AxisStateKey, AxisStateValue>> resolved_changes;
    uint64_t                                change_hash;
};

// =============================================================================
// Thread Pool for Parallel Resolution
// =============================================================================

/**
 * @brief Simple thread pool for parallel conflict resolution
 */
class WorkerPool {
public:
    explicit WorkerPool(uint32_t thread_count);
    ~WorkerPool();

    // Non-copyable
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    /**
     * @brief Submits a task for execution
     */
    void Submit(std::function<void()> task);

    /**
     * @brief Waits for all submitted tasks to complete
     */
    void WaitAll();

    /**
     * @brief Gets the number of worker threads
     */
    uint32_t GetThreadCount() const { return static_cast<uint32_t>(workers_.size()); }

private:
    void WorkerThread();

    std::vector<std::thread>            workers_;
    std::queue<std::function<void()>>   tasks_;
    std::mutex                          queue_mutex_;
    std::condition_variable             condition_;
    std::condition_variable             done_condition_;
    std::atomic<uint32_t>               active_tasks_{0};
    std::atomic<bool>                   stop_{false};
};

// =============================================================================
// Time Axis Internal State
// =============================================================================

/**
 * @brief Internal state of the Time Axis system
 *
 * This is the actual implementation behind the opaque AxisTimeAxis handle.
 */
struct TimeAxisState {
    // Configuration
    AxisTimeAxisConfig config;

    // Current slot (atomic for read access from multiple threads)
    std::atomic<AxisSlotIndex> current_slot{0};

    // Request ID generator
    std::atomic<AxisRequestId> next_request_id{1};

    // Conflict group ID generator
    std::atomic<AxisConflictGroupId> next_group_id{0};

    // Pending requests (protected by mutex)
    std::mutex requests_mutex;
    std::vector<PendingRequest> pending_requests;

    // Conflict groups (protected by mutex)
    std::mutex groups_mutex;
    std::vector<ConflictGroupData> conflict_groups;

    // Anchors (protected by mutex)
    std::mutex anchors_mutex;
    std::vector<AnchorData> anchors;
    AxisSlotIndex last_anchor_slot{0};

    // Current state (for reconstruction)
    std::mutex state_mutex;
    std::unordered_map<uint64_t, AxisStateValue> current_state;

    // Worker pool for parallel resolution
    std::unique_ptr<WorkerPool> worker_pool;

    // Debug callback
    std::mutex callback_mutex;
    AxisSlotCommitCallback commit_callback{nullptr};
    void* callback_user_data{nullptr};

    // Statistics
    std::atomic<uint64_t> total_requests_processed{0};
    std::atomic<uint64_t> total_conflicts_resolved{0};

    // Initialization flag
    bool initialized{false};
};

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * @brief Creates a combined key from AxisStateKey
 */
inline uint64_t MakeStateKeyHash(const AxisStateKey& key) {
    // Simple hash combining primary and secondary
    return key.primary ^ (key.secondary * 0x9e3779b97f4a7c15ULL);
}

/**
 * @brief Computes a deterministic hash for state changes
 */
uint64_t ComputeChangesHash(
    const std::vector<std::pair<AxisStateKey, AxisStateValue>>& changes
);

/**
 * @brief Generates a reconstruction key from state
 */
AxisReconstructionKey GenerateReconstructionKey(
    AxisSlotIndex slot,
    uint64_t state_hash,
    uint64_t transition_hash
);

/**
 * @brief Resolves conflicts within a single group
 *
 * @param group         The conflict group configuration
 * @param requests      Requests targeting this group for this slot
 * @param out_result    Output: resolved changes
 *
 * @return true on success
 */
bool ResolveConflictGroup(
    const ConflictGroupData& group,
    const std::vector<const PendingRequest*>& requests,
    GroupResolutionResult& out_result
);

} // namespace axis::time::internal

#endif // AXIS_TIME_SLOT_INTERNAL_H
