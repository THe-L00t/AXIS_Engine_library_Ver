/**
 * @file axis_time_slot.h
 * @brief AXIS Single Time Axis - Public API
 *
 * The Single Time Axis is the authoritative execution and state transition
 * model for the AXIS Engine. This is NOT a scheduler, timeline editor, or
 * gameplay system. It is a deterministic, explainable time-structure that
 * all other systems depend on.
 *
 * Key Concepts:
 * - Time progresses in discrete Time Slots
 * - All state changes are requested, resolved, and committed through Time Slots
 * - No system may mutate state directly outside the Time Axis
 * - Past slots are reconstructible without per-slot storage
 *
 * Usage Pattern:
 * 1. Create Time Axis with AxisTimeAxis_Create()
 * 2. Register conflict groups with AxisTimeAxis_CreateConflictGroup()
 * 3. Submit state change requests with AxisTimeAxis_SubmitRequest()
 * 4. Advance time with AxisTimeAxis_Tick()
 * 5. Query or reconstruct past state as needed
 *
 * Thread Safety:
 * - SubmitRequest is thread-safe
 * - Tick must be called from a single thread (the "main" thread)
 * - Conflict resolution happens in parallel across worker threads
 * - Commit phase is single-threaded and deterministic
 *
 * @note C ABI compatible
 */

#ifndef AXIS_TIME_SLOT_H
#define AXIS_TIME_SLOT_H

#include "axis_time_slot_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Time Axis Lifecycle
// =============================================================================

/**
 * @brief Creates a new Time Axis system
 *
 * @param config    Configuration parameters (NULL for defaults)
 * @param out_axis  Output: handle to the created Time Axis
 *
 * @return AXIS_TIME_OK on success
 *
 * @note Caller must destroy with AxisTimeAxis_Destroy()
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_Create(
    const AxisTimeAxisConfig* config,
    AxisTimeAxis** out_axis
);

/**
 * @brief Destroys a Time Axis system
 *
 * All pending requests are discarded. All resources are freed.
 *
 * @param axis  The Time Axis to destroy (may be NULL)
 */
AXIS_TIME_API void AxisTimeAxis_Destroy(AxisTimeAxis* axis);

// =============================================================================
// Time Progression
// =============================================================================

/**
 * @brief Advances the Time Axis by one slot
 *
 * This function:
 * 1. Resolves all requests targeting the next slot
 * 2. Applies conflict resolution in parallel across groups
 * 3. Commits the resolved state in deterministic order
 * 4. Creates an anchor if interval is reached
 *
 * @param axis  The Time Axis
 *
 * @return AXIS_TIME_OK on success
 *
 * @note Must be called from a single thread only
 * @note This is the ONLY way time progresses - no automatic advancement
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_Tick(AxisTimeAxis* axis);

/**
 * @brief Advances the Time Axis by multiple slots
 *
 * Equivalent to calling Tick() count times, but may be optimized.
 * Empty slots (no pending requests) are processed efficiently.
 *
 * @param axis   The Time Axis
 * @param count  Number of slots to advance
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_TickMultiple(
    AxisTimeAxis* axis,
    uint32_t count
);

/**
 * @brief Gets the current slot index
 *
 * @param axis  The Time Axis
 *
 * @return Current slot index (0 before first Tick)
 */
AXIS_TIME_API AxisSlotIndex AxisTimeAxis_GetCurrentSlot(const AxisTimeAxis* axis);

// =============================================================================
// Conflict Group Management
// =============================================================================

/**
 * @brief Creates a conflict group with a specified policy
 *
 * Conflict groups define isolation boundaries. Requests in different groups
 * are resolved independently and can be processed in parallel.
 *
 * @param axis       The Time Axis
 * @param policy     Conflict resolution policy
 * @param out_group  Output: ID of the created group
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_CreateConflictGroup(
    AxisTimeAxis* axis,
    AxisConflictPolicy policy,
    AxisConflictGroupId* out_group
);

/**
 * @brief Creates a conflict group with a custom policy function
 *
 * @param axis       The Time Axis
 * @param policy_fn  Custom resolution function (must be deterministic)
 * @param user_data  User data passed to policy function
 * @param out_group  Output: ID of the created group
 *
 * @return AXIS_TIME_OK on success
 *
 * @note policy_fn must remain valid for the lifetime of the group
 * @note policy_fn must be thread-safe
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_CreateConflictGroupCustom(
    AxisTimeAxis* axis,
    AxisCustomPolicyFn policy_fn,
    void* user_data,
    AxisConflictGroupId* out_group
);

/**
 * @brief Destroys a conflict group
 *
 * All pending requests in this group are discarded.
 *
 * @param axis      The Time Axis
 * @param group_id  The conflict group to destroy
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_DestroyConflictGroup(
    AxisTimeAxis* axis,
    AxisConflictGroupId group_id
);

// =============================================================================
// Request Submission
// =============================================================================

/**
 * @brief Submits a state change request
 *
 * The request targets a specific future slot and conflict group.
 * Multiple requests to the same slot/group are resolved according to policy.
 *
 * @param axis       The Time Axis
 * @param desc       Description of the state change
 * @param out_id     Output: ID of the submitted request (may be NULL)
 *
 * @return AXIS_TIME_OK on success
 *         AXIS_TIME_ERROR_SLOT_IN_PAST if target_slot <= current_slot
 *
 * @note Thread-safe - may be called from any thread
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SubmitRequest(
    AxisTimeAxis* axis,
    const AxisStateChangeDesc* desc,
    AxisRequestId* out_id
);

/**
 * @brief Submits multiple state change requests in batch
 *
 * More efficient than individual submissions for many requests.
 *
 * @param axis         The Time Axis
 * @param descs        Array of state change descriptions
 * @param count        Number of descriptions
 * @param out_ids      Output: array of request IDs (may be NULL)
 *
 * @return AXIS_TIME_OK if all succeeded, first error code otherwise
 *
 * @note Thread-safe
 * @note Atomic: either all are submitted or none
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SubmitRequestBatch(
    AxisTimeAxis* axis,
    const AxisStateChangeDesc* descs,
    size_t count,
    AxisRequestId* out_ids
);

/**
 * @brief Cancels a pending request
 *
 * @param axis        The Time Axis
 * @param request_id  ID of the request to cancel
 *
 * @return AXIS_TIME_OK on success
 *         AXIS_TIME_ERROR_NOT_FOUND if request doesn't exist or already processed
 *
 * @note Thread-safe
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_CancelRequest(
    AxisTimeAxis* axis,
    AxisRequestId request_id
);

// =============================================================================
// Anchor & Reconstruction
// =============================================================================

/**
 * @brief Gets the oldest slot that can be reconstructed
 *
 * Due to bounded memory, only slots back to the oldest anchor can be
 * reconstructed. Anchors older than max_anchors are automatically pruned.
 *
 * @param axis  The Time Axis
 *
 * @return Oldest reconstructible slot index
 */
AXIS_TIME_API AxisSlotIndex AxisTimeAxis_GetOldestReconstructibleSlot(
    const AxisTimeAxis* axis
);

/**
 * @brief Gets the reconstruction key for a specific slot
 *
 * Keys can be stored externally and used later to verify or derive state.
 * This is useful for debugging, replays, and network synchronization.
 *
 * @param axis       The Time Axis
 * @param slot_index Target slot (must be >= oldest reconstructible)
 * @param out_key    Output: reconstruction key
 *
 * @return AXIS_TIME_OK on success
 *         AXIS_TIME_ERROR_SLOT_IN_PAST if slot is too old
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_GetReconstructionKey(
    const AxisTimeAxis* axis,
    AxisSlotIndex slot_index,
    AxisReconstructionKey* out_key
);

/**
 * @brief Manually creates an anchor at the current slot
 *
 * Useful for marking significant points (e.g., game save points).
 * Does not affect the automatic anchor interval.
 *
 * @param axis  The Time Axis
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_CreateAnchorNow(AxisTimeAxis* axis);

/**
 * @brief Sets the anchor creation interval
 *
 * @param axis      The Time Axis
 * @param interval  New interval (in slots, minimum 1)
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetAnchorInterval(
    AxisTimeAxis* axis,
    uint32_t interval
);

// =============================================================================
// State Query (via Reconstruction)
// =============================================================================

/**
 * @brief Reconstructs and enumerates state at a specific past slot
 *
 * This function:
 * 1. Finds the nearest anchor before the target slot
 * 2. Derives state changes from anchor to target
 * 3. Calls the enumerator for each state entry
 *
 * Cost: O(anchor_interval) per reconstruction
 *
 * @param axis         The Time Axis
 * @param slot_index   Target slot to reconstruct
 * @param group_id     Conflict group to query (AXIS_CONFLICT_GROUP_INVALID for all)
 * @param enumerator   Callback for each state entry
 * @param user_data    User data for callback
 *
 * @return AXIS_TIME_OK on success
 *
 * @note This is a read-only operation
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_ReconstructState(
    const AxisTimeAxis* axis,
    AxisSlotIndex slot_index,
    AxisConflictGroupId group_id,
    AxisStateEnumerator enumerator,
    void* user_data
);

/**
 * @brief Queries a single state value at a specific slot
 *
 * More efficient than full reconstruction for single values.
 *
 * @param axis         The Time Axis
 * @param slot_index   Target slot
 * @param key          State key to query
 * @param out_value    Output: state value
 *
 * @return AXIS_TIME_OK on success
 *         AXIS_TIME_ERROR_NOT_FOUND if key doesn't exist at that slot
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_QueryState(
    const AxisTimeAxis* axis,
    AxisSlotIndex slot_index,
    const AxisStateKey* key,
    AxisStateValue* out_value
);

// =============================================================================
// Debug & Inspection
// =============================================================================

/**
 * @brief Gets current statistics
 *
 * @param axis       The Time Axis
 * @param out_stats  Output: statistics
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_GetStats(
    const AxisTimeAxis* axis,
    AxisTimeAxisStats* out_stats
);

/**
 * @brief Sets a callback for slot commit events
 *
 * Useful for debugging and logging slot transitions.
 *
 * @param axis       The Time Axis
 * @param callback   Callback function (NULL to disable)
 * @param user_data  User data for callback
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetCommitCallback(
    AxisTimeAxis* axis,
    AxisSlotCommitCallback callback,
    void* user_data
);

/**
 * @brief Gets the number of pending requests for a specific slot
 *
 * @param axis        The Time Axis
 * @param slot_index  Target slot
 *
 * @return Number of pending requests (0 if slot is in the past)
 */
AXIS_TIME_API size_t AxisTimeAxis_GetPendingRequestCount(
    const AxisTimeAxis* axis,
    AxisSlotIndex slot_index
);

// =============================================================================
// Slot Termination Policy
// =============================================================================
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  CRITICAL: TERMINATION POLICY SEMANTICS                                │
// │                                                                         │
// │  "A termination policy is part of the Time Axis definition,            │
// │   not part of gameplay logic."                                         │
// │                                                                         │
// │  The termination policy is IMMUTABLE after Time Axis creation.         │
// │                                                                         │
// │  CORRECT USAGE:                                                         │
// │    AxisTerminationConfig term = AxisTermination_DefaultConfig();       │
// │    term.step_limit = 5000;                                             │
// │    config.termination_config = &term;                                  │
// │    AxisTimeAxis_Create(&config, &axis);  // Policy locked here         │
// │                                                                         │
// │  The setter functions below are DEPRECATED and will return             │
// │  AXIS_TIME_ERROR_POLICY_LOCKED after Time Axis creation.               │
// │                                                                         │
// │  If you need different termination logic, create a NEW Time Axis.      │
// └─────────────────────────────────────────────────────────────────────────┘
//
// =============================================================================

/**
 * @brief Sets the step limit termination condition
 *
 * @deprecated This function is DEPRECATED. Configure termination policy
 *             at Time Axis creation via AxisTimeAxisConfig.termination_config.
 *
 * Slot ends when: elapsed_steps >= max_steps
 *
 * @param axis       The Time Axis
 * @param max_steps  Maximum steps per slot (0 to disable)
 *
 * @return AXIS_TIME_ERROR_POLICY_LOCKED - Policy cannot be modified after creation
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationByStepLimit(
    AxisTimeAxis* axis,
    uint32_t max_steps
);

/**
 * @brief Sets the request drain termination condition
 *
 * @deprecated This function is DEPRECATED. Configure termination policy
 *             at Time Axis creation via AxisTimeAxisConfig.termination_config.
 *
 * @return AXIS_TIME_ERROR_POLICY_LOCKED - Policy cannot be modified after creation
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationOnRequestDrain(
    AxisTimeAxis* axis,
    int enabled
);

/**
 * @brief Sets the conflict group resolution termination condition
 *
 * @deprecated This function is DEPRECATED. Configure termination policy
 *             at Time Axis creation via AxisTimeAxisConfig.termination_config.
 *
 * @return AXIS_TIME_ERROR_POLICY_LOCKED - Policy cannot be modified after creation
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationOnGroupResolution(
    AxisTimeAxis* axis,
    int enabled
);

/**
 * @brief Sets the external signal termination condition
 *
 * @deprecated This function is DEPRECATED. Configure termination policy
 *             at Time Axis creation via AxisTimeAxisConfig.termination_config.
 *
 * @return AXIS_TIME_ERROR_POLICY_LOCKED - Policy cannot be modified after creation
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationOnExternalSignal(
    AxisTimeAxis* axis,
    uint32_t required_flags_mask
);

/**
 * @brief Sets the safety cap (hard upper bound)
 *
 * @deprecated This function is DEPRECATED. Configure termination policy
 *             at Time Axis creation via AxisTimeAxisConfig.termination_config.
 *
 * @return AXIS_TIME_ERROR_POLICY_LOCKED - Policy cannot be modified after creation
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationSafetyCap(
    AxisTimeAxis* axis,
    uint32_t max_steps_cap
);

/**
 * @brief Sets a custom termination callback
 *
 * @deprecated This function is DEPRECATED. Configure termination policy
 *             at Time Axis creation via AxisTimeAxisConfig.termination_config.
 *
 * @return AXIS_TIME_ERROR_POLICY_LOCKED - Policy cannot be modified after creation
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetCustomTerminationCallback(
    AxisTimeAxis* axis,
    AxisSlotTerminationCallback callback,
    void* user_data
);

/**
 * @brief Sets the complete termination configuration
 *
 * @deprecated This function is DEPRECATED. Configure termination policy
 *             at Time Axis creation via AxisTimeAxisConfig.termination_config.
 *
 * @return AXIS_TIME_ERROR_POLICY_LOCKED - Policy cannot be modified after creation
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationConfig(
    AxisTimeAxis* axis,
    const AxisTerminationConfig* config
);

/**
 * @brief Gets the current termination configuration
 *
 * @param axis        The Time Axis
 * @param out_config  Output: current configuration
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_GetTerminationConfig(
    const AxisTimeAxis* axis,
    AxisTerminationConfig* out_config
);

/**
 * @brief Sets an external signal flag
 *
 * @param axis  The Time Axis
 * @param flag  Flag to set (OR'd with existing flags)
 *
 * @return AXIS_TIME_OK on success
 *
 * @note Thread-safe
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetExternalSignal(
    AxisTimeAxis* axis,
    uint32_t flag
);

/**
 * @brief Clears an external signal flag
 *
 * @param axis  The Time Axis
 * @param flag  Flag to clear
 *
 * @return AXIS_TIME_OK on success
 *
 * @note Thread-safe
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_ClearExternalSignal(
    AxisTimeAxis* axis,
    uint32_t flag
);

/**
 * @brief Gets the current termination context
 *
 * @param axis         The Time Axis
 * @param out_context  Output: current context
 *
 * @return AXIS_TIME_OK on success
 */
AXIS_TIME_API AxisTimeResult AxisTimeAxis_GetTerminationContext(
    const AxisTimeAxis* axis,
    AxisSlotTerminationContext* out_context
);

/**
 * @brief Gets the reason for the last slot termination
 *
 * @param axis  The Time Axis
 *
 * @return The termination reason
 */
AXIS_TIME_API AxisTerminationReason AxisTimeAxis_GetLastTerminationReason(
    const AxisTimeAxis* axis
);

/**
 * @brief Gets the IMMUTABLE termination policy hash
 *
 * PHILOSOPHY:
 * "A termination policy is part of the Time Axis definition, not part of gameplay logic."
 *
 * This hash is computed ONCE at Time Axis creation and NEVER changes.
 * It represents the "semantic fingerprint" of the Time Axis.
 *
 * Use cases:
 * - Verify two Time Axes have the same termination semantics
 * - Validate anchor compatibility during reconstruction
 * - Debugging and logging
 *
 * @param axis  The Time Axis
 *
 * @return The 64-bit termination policy hash (0 if axis is NULL)
 *
 * @note This value is constant for the lifetime of the Time Axis
 * @note Modifying termination settings after creation does NOT change this hash
 */
AXIS_TIME_API uint64_t AxisTimeAxis_GetTerminationPolicyHash(
    const AxisTimeAxis* axis
);

#ifdef __cplusplus
}
#endif

#endif // AXIS_TIME_SLOT_H
