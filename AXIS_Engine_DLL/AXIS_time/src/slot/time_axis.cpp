/**
 * @file time_axis.cpp
 * @brief Core implementation of the Time Axis system
 *
 * This file implements the main Time Axis functionality including:
 * - Lifecycle management (create/destroy)
 * - Time progression (tick)
 * - Request submission and cancellation
 * - Conflict group management
 */

#include "pch.h"
#include "slot_internal.h"
#include <algorithm>
#include <queue>

using namespace axis::time::internal;

// =============================================================================
// Default Configuration
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeAxisConfig AxisTimeAxis_DefaultConfig(void) {
    AxisTimeAxisConfig config{};
    config.worker_thread_count = 0;  // Auto-detect
    config.max_pending_requests = 65536;
    config.anchor_interval = AXIS_DEFAULT_ANCHOR_INTERVAL;
    config.max_anchors = 64;
    config.initial_conflict_group_capacity = 32;
    config.termination_config = nullptr;  // Uses default termination config
    return config;
}

// =============================================================================
// Time Axis Lifecycle
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_Create(
    const AxisTimeAxisConfig* config,
    AxisTimeAxis** out_axis
) {
    if (!out_axis) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    *out_axis = nullptr;

    // Use default config if not provided
    AxisTimeAxisConfig actual_config = config ? *config : AxisTimeAxis_DefaultConfig();

    // Validate config
    if (actual_config.anchor_interval == 0) {
        actual_config.anchor_interval = AXIS_DEFAULT_ANCHOR_INTERVAL;
    }
    if (actual_config.max_anchors == 0) {
        actual_config.max_anchors = 64;
    }

    // Determine thread count
    uint32_t thread_count = actual_config.worker_thread_count;
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = 4;  // Fallback
        }
    }

    // Allocate state
    auto* state = new (std::nothrow) TimeAxisState();
    if (!state) {
        return AXIS_TIME_ERROR_OUT_OF_MEMORY;
    }

    state->config = actual_config;
    state->conflict_groups.reserve(actual_config.initial_conflict_group_capacity);
    state->anchors.reserve(actual_config.max_anchors);

    // Create worker pool
    try {
        state->worker_pool = std::make_unique<WorkerPool>(thread_count);
    } catch (...) {
        delete state;
        return AXIS_TIME_ERROR_THREAD_POOL_FAILED;
    }

    // Initialize termination policy FIRST - needed before creating genesis anchor
    // CRITICAL: Policy is set at creation and NEVER changes after
    // "A termination policy is part of the Time Axis definition, not part of gameplay logic."
    //
    // The termination policy defines the semantic identity of this Time Axis.
    // Anchors created with different policies are INCOMPATIBLE.
    if (actual_config.termination_config) {
        // Use provided termination config
        state->termination_policy.config = *actual_config.termination_config;
    } else {
        // Use default termination config
        state->termination_policy.config = AxisTermination_DefaultConfig();
    }
    state->termination_context = {};

    // Compute termination policy hash ONCE - this is IMMUTABLE for the lifetime of this Time Axis
    // This hash is the "semantic fingerprint" that defines this Time Axis's identity
    // Anchors with different policy hashes are INCOMPATIBLE
    // Modifying termination policy after this point has NO effect on the hash
    state->termination_policy_hash = state->termination_policy.GetPolicyHash();

    // Create initial anchor at slot 0
    // This is the genesis anchor - all reconstruction starts from here or later anchors
    AnchorData initial_anchor{};
    initial_anchor.anchor_id = state->next_anchor_id.fetch_add(1);
    initial_anchor.slot_index = 0;
    std::memset(initial_anchor.transition_hash, 0, 16);
    std::memset(initial_anchor.resolution_hash, 0, 16);
    // Store the axis's termination policy hash - this anchor belongs to THIS Time Axis
    initial_anchor.termination_policy_hash = state->termination_policy_hash;
    state->anchors.push_back(std::move(initial_anchor));

    state->initialized = true;
    *out_axis = reinterpret_cast<AxisTimeAxis*>(state);

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API void AxisTimeAxis_Destroy(AxisTimeAxis* axis) {
    if (!axis) {
        return;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    // Worker pool destructor handles thread shutdown
    state->worker_pool.reset();

    delete state;
}

// =============================================================================
// Time Progression
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_Tick(AxisTimeAxis* axis) {
    if (!axis) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);
    if (!state->initialized) {
        return AXIS_TIME_ERROR_NOT_INITIALIZED;
    }

    // CRITICAL LIFECYCLE CHECK
    // "Once time decides to stop, it cannot be restarted."
    // If axis has terminated, reject all further ticks deterministically
    if (state->lifecycle.load() == AxisLifecycle::TERMINATED) {
        return AXIS_TIME_ERROR_TERMINATED;
    }

    AxisSlotIndex target_slot = state->current_slot.load() + 1;

    // Step 1: Collect requests for this slot
    std::vector<PendingRequest> slot_requests;
    {
        std::lock_guard<std::mutex> lock(state->requests_mutex);

        auto it = state->pending_requests.begin();
        while (it != state->pending_requests.end()) {
            if (it->cancelled) {
                it = state->pending_requests.erase(it);
                continue;
            }

            if (it->desc.target_slot == target_slot) {
                slot_requests.push_back(*it);
                it = state->pending_requests.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Step 2: Group requests by conflict group
    std::unordered_map<AxisConflictGroupId, std::vector<const PendingRequest*>> grouped_requests;
    for (const auto& req : slot_requests) {
        grouped_requests[req.desc.conflict_group].push_back(&req);
    }

    // Step 3: Resolve each conflict group (in parallel)
    //
    // SEMANTIC DISTINCTION (CRITICAL):
    //   total_groups = number of groups OBSERVED (all groups with requests)
    //   resolved_groups = number of groups that COMPLETED SUCCESSFULLY
    //
    // These may differ when:
    //   - Some groups fail resolution
    //   - Some groups defer processing
    //   - Resolution errors occur
    //
    // This distinction enables terminate_on_group_resolution to have real meaning.
    uint32_t total_groups = static_cast<uint32_t>(grouped_requests.size());

    std::vector<GroupResolutionResult> resolution_results;
    resolution_results.resize(grouped_requests.size());

    std::atomic<size_t> result_index{0};
    std::atomic<uint32_t> resolved_group_count{0};  // Track successful resolutions
    std::atomic<bool> resolution_error{false};

    std::vector<ConflictGroupData> groups_copy;
    {
        std::lock_guard<std::mutex> lock(state->groups_mutex);
        groups_copy = state->conflict_groups;
    }

    for (const auto& [group_id, requests] : grouped_requests) {
        state->worker_pool->Submit([&, group_id]() {
            // Find group configuration
            ConflictGroupData group{};
            group.id = group_id;
            group.policy = AXIS_POLICY_FIRST_WRITER;  // Default
            group.active = true;

            for (const auto& g : groups_copy) {
                if (g.id == group_id && g.active) {
                    group = g;
                    break;
                }
            }

            GroupResolutionResult result;
            if (!ResolveConflictGroup(group, requests, result)) {
                resolution_error.store(true);
                return;
            }

            // Group resolved successfully - increment both counters
            size_t idx = result_index.fetch_add(1);
            resolved_group_count.fetch_add(1);  // Only incremented on success

            if (idx < resolution_results.size()) {
                resolution_results[idx] = std::move(result);
            }
        });
    }

    state->worker_pool->WaitAll();

    // Store the final resolved count (may be less than total if some groups failed)
    uint32_t resolved_groups = resolved_group_count.load();

    if (resolution_error.load()) {
        // Resolution failed for some groups, but we still advance time
        // This prevents the system from stalling
        // resolved_groups will be < total_groups, reflecting the partial failure
    }

    // Step 4: Commit results in deterministic order (sorted by group ID)
    std::sort(resolution_results.begin(), resolution_results.end(),
        [](const GroupResolutionResult& a, const GroupResolutionResult& b) {
            return a.group_id < b.group_id;
        });

    size_t total_changes = 0;
    uint64_t combined_hash = 0;

    {
        std::lock_guard<std::mutex> lock(state->state_mutex);

        for (const auto& result : resolution_results) {
            for (const auto& [key, value] : result.resolved_changes) {
                uint64_t key_hash = MakeStateKeyHash(key);
                state->current_state[key_hash] = value;
                total_changes++;
            }
            combined_hash ^= result.change_hash;
        }
    }

    // Step 5: Record this slot's transition for reconstruction
    // CRITICAL: This is how we reconstruct past slots without storing them
    {
        std::lock_guard<std::mutex> lock(state->transitions_mutex);

        SlotTransition transition;
        transition.slot_index = target_slot;
        transition.requests = slot_requests;
        transition.resolution_hash = combined_hash;

        // Collect all resolved changes
        for (const auto& result : resolution_results) {
            for (const auto& change : result.resolved_changes) {
                transition.resolved_changes.push_back(change);
            }
        }

        state->pending_transitions.push_back(std::move(transition));
    }

    // Step 6: Update statistics
    state->total_requests_processed.fetch_add(slot_requests.size());
    state->total_conflicts_resolved.fetch_add(
        slot_requests.size() > total_changes ? slot_requests.size() - total_changes : 0
    );

    // Step 7: Create anchor if interval reached
    if (target_slot - state->last_anchor_slot >= state->config.anchor_interval) {
        std::lock_guard<std::mutex> anchors_lock(state->anchors_mutex);
        std::lock_guard<std::mutex> state_lock(state->state_mutex);
        std::lock_guard<std::mutex> trans_lock(state->transitions_mutex);

        AnchorData anchor;
        anchor.anchor_id = state->next_anchor_id.fetch_add(1);
        anchor.slot_index = target_slot;
        anchor.state_snapshot = state->current_state;

        // Store transition log for reconstruction of slots between anchors
        for (const auto& trans : state->pending_transitions) {
            for (const auto& req : trans.requests) {
                anchor.transition_log.push_back(req);
            }
        }

        // Compute hashes for determinism verification
        ComputeTransitionHash(state->pending_transitions, anchor.transition_hash);
        ComputePolicyHash(resolution_results, anchor.resolution_hash);

        // Store the axis's IMMUTABLE termination policy hash
        // This anchor inherits the Time Axis's semantic identity
        anchor.termination_policy_hash = state->termination_policy_hash;

        state->anchors.push_back(std::move(anchor));
        state->last_anchor_slot = target_slot;

        // Clear pending transitions (now stored in anchor)
        state->pending_transitions.clear();

        // Prune old anchors if needed
        while (state->anchors.size() > state->config.max_anchors) {
            state->anchors.erase(state->anchors.begin());
        }
    }

    // Step 8: Advance current slot
    state->current_slot.store(target_slot);

    // Step 9: Call debug callback if set
    {
        std::lock_guard<std::mutex> lock(state->callback_mutex);
        if (state->commit_callback) {
            state->commit_callback(target_slot, total_changes, state->callback_user_data);
        }
    }

    // Step 10: Update termination context and evaluate termination policy
    // CRITICAL: This happens AFTER the tick completes to determine if this was a terminating tick
    {
        std::lock_guard<std::mutex> lock(state->termination_mutex);

        // Increment elapsed steps (total ticks executed)
        state->termination_context.elapsed_steps++;

        // Update remaining pending requests count
        {
            std::lock_guard<std::mutex> req_lock(state->requests_mutex);
            state->termination_context.pending_requests = static_cast<uint32_t>(state->pending_requests.size());
        }

        // Update group resolution stats
        state->termination_context.resolved_groups = static_cast<uint32_t>(grouped_requests.size());
        state->termination_context.total_groups = static_cast<uint32_t>(grouped_requests.size());

        // Update external flags
        state->termination_context.external_flags = state->external_flags.load();

        // Evaluate termination policy
        AxisTerminationReason reason = state->termination_policy.Evaluate(state->termination_context);
        state->last_termination_reason = reason;
    }

    // Step 10: Update termination context and evaluate termination policy
    //
    // CRITICAL PHILOSOPHY:
    // "Time decides when the world progresses.
    //  Causality decides why the world changes.
    //  Termination decides whether time itself is allowed to continue."
    //
    // WHY THIS HAPPENS AFTER TICK COMPLETES:
    // - Termination policy is NOT gameplay logic
    // - It observes meta-state (counts, flags, summaries), NEVER concrete state data
    // - Evaluation determines if THIS tick was the final tick
    // - Once terminated, lifecycle transitions to TERMINATED (irreversible)
    //
    // SEMANTIC CONTRACT ENFORCEMENT:
    // - elapsed_steps: cumulative, monotonic (incremented here)
    // - pending_requests: snapshot of remaining queue
    // - resolved_groups: SUCCESSFUL resolutions only (may be < total_groups)
    // - total_groups: all groups observed this tick
    // - external_flags: runtime signals
    // - causality_summary: FUTURE extension (currently NULL)
    {
        std::lock_guard<std::mutex> lock(state->termination_mutex);

        // Increment elapsed steps (cumulative tick count, monotonic, never resets)
        state->termination_context.elapsed_steps++;

        // Update remaining pending requests count (snapshot of queue size)
        {
            std::lock_guard<std::mutex> req_lock(state->requests_mutex);
            state->termination_context.pending_requests = static_cast<uint32_t>(state->pending_requests.size());
        }

        // Update group resolution stats
        // CRITICAL: resolved_groups may be < total_groups if some groups failed
        // This distinction gives terminate_on_group_resolution real semantic meaning
        state->termination_context.resolved_groups = resolved_groups;
        state->termination_context.total_groups = total_groups;

        // Update external flags (runtime signals)
        state->termination_context.external_flags = state->external_flags.load();

        // Causality summary (FUTURE EXTENSION - currently NULL)
        // Reserved for future Causality / Data Axis integration
        // Does NOT affect termination policy yet
        state->termination_context.causality_summary = nullptr;

        // Evaluate termination policy
        // PHILOSOPHY: "Termination policy is part of Time Axis definition, not gameplay logic"
        // Policy was set at creation and is IMMUTABLE
        AxisTerminationReason reason = state->termination_policy.Evaluate(state->termination_context);
        state->last_termination_reason = reason;

        // CRITICAL LIFECYCLE TRANSITION
        // "Once time decides to stop, it cannot be restarted."
        // If termination condition met, transition to TERMINATED state
        // All future Tick() calls will return AXIS_TIME_ERROR_TERMINATED
        if (reason != AXIS_TERMINATION_NONE) {
            state->lifecycle.store(AxisLifecycle::TERMINATED);
        }
    }

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_TickMultiple(
    AxisTimeAxis* axis,
    uint32_t count
) {
    if (!axis) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < count; ++i) {
        AxisTimeResult result = AxisTimeAxis_Tick(axis);
        if (result != AXIS_TIME_OK) {
            return result;
        }
    }

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisSlotIndex AxisTimeAxis_GetCurrentSlot(const AxisTimeAxis* axis) {
    if (!axis) {
        return AXIS_SLOT_INVALID;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);
    return state->current_slot.load();
}

// =============================================================================
// Conflict Group Management
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_CreateConflictGroup(
    AxisTimeAxis* axis,
    AxisConflictPolicy policy,
    AxisConflictGroupId* out_group
) {
    if (!axis || !out_group) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    if (policy == AXIS_POLICY_CUSTOM) {
        return AXIS_TIME_ERROR_INVALID_POLICY;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    std::lock_guard<std::mutex> lock(state->groups_mutex);

    if (state->conflict_groups.size() >= AXIS_MAX_CONFLICT_GROUPS) {
        return AXIS_TIME_ERROR_CONFLICT_GROUP_FULL;
    }

    ConflictGroupData group;
    group.id = state->next_group_id.fetch_add(1);
    group.policy = policy;
    group.custom_fn = nullptr;
    group.custom_user_data = nullptr;
    group.active = true;

    state->conflict_groups.push_back(group);
    *out_group = group.id;

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_CreateConflictGroupCustom(
    AxisTimeAxis* axis,
    AxisCustomPolicyFn policy_fn,
    void* user_data,
    AxisConflictGroupId* out_group
) {
    if (!axis || !policy_fn || !out_group) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    std::lock_guard<std::mutex> lock(state->groups_mutex);

    if (state->conflict_groups.size() >= AXIS_MAX_CONFLICT_GROUPS) {
        return AXIS_TIME_ERROR_CONFLICT_GROUP_FULL;
    }

    ConflictGroupData group;
    group.id = state->next_group_id.fetch_add(1);
    group.policy = AXIS_POLICY_CUSTOM;
    group.custom_fn = policy_fn;
    group.custom_user_data = user_data;
    group.active = true;

    state->conflict_groups.push_back(group);
    *out_group = group.id;

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_DestroyConflictGroup(
    AxisTimeAxis* axis,
    AxisConflictGroupId group_id
) {
    if (!axis) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    std::lock_guard<std::mutex> lock(state->groups_mutex);

    for (auto& group : state->conflict_groups) {
        if (group.id == group_id) {
            group.active = false;
            return AXIS_TIME_OK;
        }
    }

    return AXIS_TIME_ERROR_NOT_FOUND;
}

// =============================================================================
// Request Submission
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SubmitRequest(
    AxisTimeAxis* axis,
    const AxisStateChangeDesc* desc,
    AxisRequestId* out_id
) {
    if (!axis || !desc) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    // Validate target slot is in the future
    if (desc->target_slot <= state->current_slot.load()) {
        return AXIS_TIME_ERROR_SLOT_IN_PAST;
    }

    std::lock_guard<std::mutex> lock(state->requests_mutex);

    if (state->pending_requests.size() >= state->config.max_pending_requests) {
        return AXIS_TIME_ERROR_REQUEST_QUEUE_FULL;
    }

    PendingRequest request;
    request.id = state->next_request_id.fetch_add(1);
    request.desc = *desc;
    request.cancelled = false;

    state->pending_requests.push_back(request);

    if (out_id) {
        *out_id = request.id;
    }

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SubmitRequestBatch(
    AxisTimeAxis* axis,
    const AxisStateChangeDesc* descs,
    size_t count,
    AxisRequestId* out_ids
) {
    if (!axis || !descs || count == 0) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);
    AxisSlotIndex current = state->current_slot.load();

    // Validate all target slots first
    for (size_t i = 0; i < count; ++i) {
        if (descs[i].target_slot <= current) {
            return AXIS_TIME_ERROR_SLOT_IN_PAST;
        }
    }

    std::lock_guard<std::mutex> lock(state->requests_mutex);

    if (state->pending_requests.size() + count > state->config.max_pending_requests) {
        return AXIS_TIME_ERROR_REQUEST_QUEUE_FULL;
    }

    for (size_t i = 0; i < count; ++i) {
        PendingRequest request;
        request.id = state->next_request_id.fetch_add(1);
        request.desc = descs[i];
        request.cancelled = false;

        state->pending_requests.push_back(request);

        if (out_ids) {
            out_ids[i] = request.id;
        }
    }

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_CancelRequest(
    AxisTimeAxis* axis,
    AxisRequestId request_id
) {
    if (!axis || request_id == AXIS_REQUEST_ID_INVALID) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    std::lock_guard<std::mutex> lock(state->requests_mutex);

    for (auto& request : state->pending_requests) {
        if (request.id == request_id && !request.cancelled) {
            request.cancelled = true;
            return AXIS_TIME_OK;
        }
    }

    return AXIS_TIME_ERROR_NOT_FOUND;
}
