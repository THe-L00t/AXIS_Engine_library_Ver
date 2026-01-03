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
 * CRITICAL: Anchors are the ONLY persistent state storage.
 * Time slots themselves are NEVER stored.
 *
 * Reconstruction path:
 *   [Anchor_k] → slot k+1 → slot k+2 → ... → slot N (target)
 *
 * The anchor stores:
 *   1. Complete state snapshot at that slot
 *   2. Unique ID for key-based lookup
 *   3. Transition log for deterministic replay to future slots
 */
struct AnchorData {
    uint64_t                anchor_id;      /** Unique anchor identifier */
    AxisSlotIndex           slot_index;     /** Slot at which this anchor was created */

    // Complete state snapshot at this anchor point
    // This is the ONLY stored state - all other slots are reconstructed
    std::unordered_map<uint64_t, AxisStateValue> state_snapshot;

    // Transition log: requests applied between previous anchor and this one
    // Used for deterministic replay when reconstructing intermediate slots
    std::vector<PendingRequest> transition_log;

    // Hash for verifying reconstruction correctness
    uint8_t transition_hash[16];  /** Hash of all transitions from prev anchor */
    uint8_t resolution_hash[16];  /** Hash of conflict resolution decisions */

    // CRITICAL: Termination policy hash from the Time Axis at anchor creation
    // This is the "semantic fingerprint" that must match for reconstruction
    // If anchor's policy hash != axis's policy hash → INCOMPATIBLE
    uint64_t termination_policy_hash{0};
};

/**
 * @brief Slot transition record for deterministic replay
 *
 * Stores the minimal information needed to replay a single slot transition.
 * These are kept between anchors for reconstruction.
 */
struct SlotTransition {
    AxisSlotIndex slot_index;
    std::vector<PendingRequest> requests;   /** All requests targeting this slot */
    std::vector<std::pair<AxisStateKey, AxisStateValue>> resolved_changes;
    uint64_t resolution_hash;               /** For determinism verification */
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
// Slot Termination Policy (Internal)
// =============================================================================

/**
 * @brief Internal termination policy interface (C++ only, advanced)
 *
 * Used for engine-level extensions or experiments.
 * NOT exposed in C ABI.
 */
class ISlotTerminationPolicy {
public:
    virtual ~ISlotTerminationPolicy() = default;

    /**
     * @brief Evaluates whether the slot should terminate
     *
     * @param ctx Current termination context
     * @return true to terminate, false to continue
     *
     * @note Must be deterministic
     * @note Must not modify engine state
     */
    virtual bool ShouldTerminate(const AxisSlotTerminationContext& ctx) const = 0;

    /**
     * @brief Gets a hash representing this policy for determinism verification
     */
    virtual uint64_t GetPolicyHash() const = 0;
};

/**
 * @brief Built-in termination policy implementation
 *
 * Evaluation order (DETERMINISTIC CONTRACT):
 * 1. Safety Cap (ALWAYS first, overrides all)
 * 2. Step Limit
 * 3. Request Drain
 * 4. Group Resolution
 * 5. External Signal
 * 6. Custom Callback (if any)
 */
class BuiltinTerminationPolicy : public ISlotTerminationPolicy {
public:
    AxisTerminationConfig config;
    AxisTerminationReason last_reason{AXIS_TERMINATION_NONE};

    bool ShouldTerminate(const AxisSlotTerminationContext& ctx) const override;
    uint64_t GetPolicyHash() const override;

    /**
     * @brief Evaluates and returns the specific termination reason
     */
    AxisTerminationReason Evaluate(const AxisSlotTerminationContext& ctx) const;
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
    // CRITICAL: Anchors are the ONLY persistent state storage
    std::mutex anchors_mutex;
    std::vector<AnchorData> anchors;
    AxisSlotIndex last_anchor_slot{0};
    std::atomic<uint64_t> next_anchor_id{1};

    // Transition log between last anchor and current slot
    // Used for deterministic reconstruction of intermediate slots
    // Cleared when new anchor is created
    std::mutex transitions_mutex;
    std::vector<SlotTransition> pending_transitions;

    // Current state (for reconstruction)
    // This is a working copy derived from the last anchor + transitions
    std::mutex state_mutex;
    std::unordered_map<uint64_t, AxisStateValue> current_state;

    // Worker pool for parallel resolution
    std::unique_ptr<WorkerPool> worker_pool;

    // Debug callback
    std::mutex callback_mutex;
    AxisSlotCommitCallback commit_callback{nullptr};
    void* callback_user_data{nullptr};

    // Termination policy
    // CRITICAL PHILOSOPHY:
    // "A termination policy is part of the Time Axis definition, not part of gameplay logic."
    //
    // - Policy hash is computed ONCE at creation and NEVER changes
    // - This hash is used ONLY for determinism validation during reconstruction
    // - Anchors created with different policies are INCOMPATIBLE
    // - If you need different termination logic, create a NEW Time Axis
    std::mutex termination_mutex;
    BuiltinTerminationPolicy termination_policy;
    AxisSlotTerminationContext termination_context{};
    std::atomic<uint32_t> external_flags{0};
    AxisTerminationReason last_termination_reason{AXIS_TERMINATION_NONE};

    // IMMUTABLE after creation - computed once, never modified
    // This is the "semantic fingerprint" of the Time Axis
    uint64_t termination_policy_hash{0};

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
 * @brief Generates a reconstruction key
 *
 * IMPORTANT: The key does NOT encode state.
 * It encodes HOW to reconstruct state from an anchor.
 *
 * @param anchor_id       The anchor to start reconstruction from
 * @param target_slot     The slot to reconstruct
 * @param transition_hash Hash of all transitions from anchor to target
 * @param policy_hash     Hash of conflict resolution decisions
 */
AxisReconstructionKey GenerateReconstructionKey(
    uint64_t anchor_id,
    AxisSlotIndex target_slot,
    const uint8_t* transition_hash,
    const uint8_t* policy_hash
);

/**
 * @brief Computes 128-bit hash for transitions
 */
void ComputeTransitionHash(
    const std::vector<SlotTransition>& transitions,
    uint8_t out_hash[16]
);

/**
 * @brief Computes 128-bit hash for policy decisions
 */
void ComputePolicyHash(
    const std::vector<GroupResolutionResult>& results,
    uint8_t out_hash[16]
);

/**
 * @brief Deterministically replays transitions from anchor to target slot
 *
 * This is the core reconstruction engine.
 * Given an anchor and a list of transitions, it replays them to produce
 * the exact state at any intermediate slot.
 *
 * @param anchor          Starting anchor
 * @param transitions     Transitions to replay
 * @param target_slot     Slot to reconstruct (must be >= anchor.slot_index)
 * @param groups          Conflict group configurations for resolution
 * @param out_state       Output: reconstructed state
 *
 * @return true on success
 */
bool ReplayTransitionsToSlot(
    const AnchorData& anchor,
    const std::vector<SlotTransition>& transitions,
    AxisSlotIndex target_slot,
    const std::vector<ConflictGroupData>& groups,
    std::unordered_map<uint64_t, AxisStateValue>& out_state
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
