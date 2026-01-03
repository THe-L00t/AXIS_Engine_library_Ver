/**
 * @file anchor.cpp
 * @brief Anchor-based Deterministic Reconstruction System
 *
 * CORE PHILOSOPHY:
 * "A reconstruction key does not encode a state.
 *  It encodes how to reconstruct a state from an anchor."
 *
 * CRITICAL CONSTRAINTS:
 * - Time slots are NEVER stored
 * - Only Anchors persist state
 * - Any slot is reconstructed via: Anchor + Transitions + Deterministic Replay
 *
 * Reconstruction path:
 *   [Anchor_k] → replay transitions → slot k+1 → ... → slot N (target)
 *
 * Memory is bounded by:
 *   - Max anchor count (e.g., 64)
 *   - Transition log between anchors (cleared on anchor creation)
 */

#include "pch.h"
#include "../slot/slot_internal.h"
#include <algorithm>
#include <cstring>

using namespace axis::time::internal;

// =============================================================================
// Hash Computation Helpers
// =============================================================================

namespace axis::time::internal {

/**
 * @brief Simple 128-bit FNV-1a variant for hashing
 */
static void FNV128(const uint8_t* data, size_t len, uint8_t out[16]) {
    // FNV offset basis (128-bit split into two 64-bit)
    uint64_t h0 = 0x6c62272e07bb0142ULL;
    uint64_t h1 = 0x62b821756295c58dULL;

    for (size_t i = 0; i < len; ++i) {
        h0 ^= data[i];
        h0 *= 0x100000001b3ULL;
        h1 ^= data[i];
        h1 *= 0x100000001b3ULL;
        // Mix
        h0 ^= h1 >> 32;
        h1 ^= h0 >> 32;
    }

    std::memcpy(out, &h0, 8);
    std::memcpy(out + 8, &h1, 8);
}

void ComputeTransitionHash(
    const std::vector<SlotTransition>& transitions,
    uint8_t out_hash[16]
) {
    std::vector<uint8_t> buffer;

    for (const auto& trans : transitions) {
        // Add slot index
        buffer.insert(buffer.end(),
            reinterpret_cast<const uint8_t*>(&trans.slot_index),
            reinterpret_cast<const uint8_t*>(&trans.slot_index) + sizeof(trans.slot_index));

        // Add resolution hash
        buffer.insert(buffer.end(),
            reinterpret_cast<const uint8_t*>(&trans.resolution_hash),
            reinterpret_cast<const uint8_t*>(&trans.resolution_hash) + sizeof(trans.resolution_hash));

        // Add each resolved change
        for (const auto& [key, value] : trans.resolved_changes) {
            buffer.insert(buffer.end(),
                reinterpret_cast<const uint8_t*>(&key),
                reinterpret_cast<const uint8_t*>(&key) + sizeof(key));
            buffer.insert(buffer.end(),
                reinterpret_cast<const uint8_t*>(&value.as_uint),
                reinterpret_cast<const uint8_t*>(&value.as_uint) + sizeof(value.as_uint));
        }
    }

    if (buffer.empty()) {
        std::memset(out_hash, 0, 16);
    } else {
        FNV128(buffer.data(), buffer.size(), out_hash);
    }
}

void ComputePolicyHash(
    const std::vector<GroupResolutionResult>& results,
    uint8_t out_hash[16]
) {
    std::vector<uint8_t> buffer;

    for (const auto& result : results) {
        // Add group ID
        buffer.insert(buffer.end(),
            reinterpret_cast<const uint8_t*>(&result.group_id),
            reinterpret_cast<const uint8_t*>(&result.group_id) + sizeof(result.group_id));

        // Add change hash
        buffer.insert(buffer.end(),
            reinterpret_cast<const uint8_t*>(&result.change_hash),
            reinterpret_cast<const uint8_t*>(&result.change_hash) + sizeof(result.change_hash));
    }

    if (buffer.empty()) {
        std::memset(out_hash, 0, 16);
    } else {
        FNV128(buffer.data(), buffer.size(), out_hash);
    }
}

AxisReconstructionKey GenerateReconstructionKey(
    uint64_t anchor_id,
    AxisSlotIndex target_slot,
    const uint8_t* transition_hash,
    const uint8_t* policy_hash
) {
    AxisReconstructionKey key{};

    key.anchor_id = anchor_id;
    key.target_slot = target_slot;

    if (transition_hash) {
        std::memcpy(key.transition_hash, transition_hash, 16);
    }
    if (policy_hash) {
        std::memcpy(key.policy_hash, policy_hash, 16);
    }

    return key;
}

/**
 * @brief Deterministically replays a single slot's transitions
 */
static void ApplyTransitionToState(
    const SlotTransition& transition,
    std::unordered_map<uint64_t, AxisStateValue>& state
) {
    for (const auto& [key, value] : transition.resolved_changes) {
        uint64_t key_hash = MakeStateKeyHash(key);
        state[key_hash] = value;
    }
}

bool ReplayTransitionsToSlot(
    const AnchorData& anchor,
    const std::vector<SlotTransition>& transitions,
    AxisSlotIndex target_slot,
    const std::vector<ConflictGroupData>& groups,
    std::unordered_map<uint64_t, AxisStateValue>& out_state
) {
    // Start with anchor state
    out_state = anchor.state_snapshot;

    // Replay each transition up to and including target slot
    for (const auto& trans : transitions) {
        if (trans.slot_index > target_slot) {
            break;  // We've reached the target
        }

        ApplyTransitionToState(trans, out_state);
    }

    return true;
}

uint64_t ComputeChangesHash(
    const std::vector<std::pair<AxisStateKey, AxisStateValue>>& changes
) {
    uint64_t hash = 0x517cc1b727220a95ULL;  // FNV offset basis

    for (const auto& [key, value] : changes) {
        hash ^= MakeStateKeyHash(key);
        hash *= 0x100000001b3ULL;  // FNV prime
        hash ^= value.as_uint;
        hash *= 0x100000001b3ULL;
    }

    return hash;
}

} // namespace axis::time::internal

// =============================================================================
// Anchor Management
// =============================================================================

extern "C" AXIS_TIME_API AxisSlotIndex AxisTimeAxis_GetOldestReconstructibleSlot(
    const AxisTimeAxis* axis
) {
    if (!axis) {
        return AXIS_SLOT_INVALID;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);

    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state->anchors_mutex));

    if (state->anchors.empty()) {
        return 0;
    }

    return state->anchors.front().slot_index;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_GetReconstructionKey(
    const AxisTimeAxis* axis,
    AxisSlotIndex slot_index,
    AxisReconstructionKey* out_key
) {
    if (!axis || !out_key) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);

    // Find the anchor at or before the requested slot
    const AnchorData* target_anchor = nullptr;
    std::vector<SlotTransition> relevant_transitions;

    {
        std::lock_guard<std::mutex> anchors_lock(const_cast<std::mutex&>(state->anchors_mutex));
        std::lock_guard<std::mutex> trans_lock(const_cast<std::mutex&>(state->transitions_mutex));

        if (state->anchors.empty()) {
            return AXIS_TIME_ERROR_ANCHOR_NOT_FOUND;
        }

        if (slot_index < state->anchors.front().slot_index) {
            return AXIS_TIME_ERROR_SLOT_IN_PAST;
        }

        if (slot_index > state->current_slot.load()) {
            return AXIS_TIME_ERROR_INVALID_PARAMETER;
        }

        // Find nearest anchor before target slot
        for (auto it = state->anchors.rbegin(); it != state->anchors.rend(); ++it) {
            if (it->slot_index <= slot_index) {
                target_anchor = &(*it);
                break;
            }
        }

        if (!target_anchor) {
            return AXIS_TIME_ERROR_ANCHOR_NOT_FOUND;
        }

        // CRITICAL: Verify anchor compatibility
        // "A termination policy is part of the Time Axis definition, not part of gameplay logic."
        // Anchors with different policies are INCOMPATIBLE
        if (target_anchor->termination_policy_hash != state->termination_policy_hash) {
            return AXIS_TIME_ERROR_POLICY_MISMATCH;
        }

        // Collect transitions from anchor to target slot
        for (const auto& trans : state->pending_transitions) {
            if (trans.slot_index > target_anchor->slot_index &&
                trans.slot_index <= slot_index) {
                relevant_transitions.push_back(trans);
            }
        }
    }

    // Compute hashes for the reconstruction path
    uint8_t transition_hash[16];
    uint8_t resolution_hash[16];

    ComputeTransitionHash(relevant_transitions, transition_hash);
    std::memcpy(resolution_hash, target_anchor->resolution_hash, 16);

    // Generate the key
    // THE KEY TELLS US: "Start from anchor X, replay to slot Y, verify with these hashes"
    *out_key = GenerateReconstructionKey(
        target_anchor->anchor_id,
        slot_index,
        transition_hash,
        resolution_hash
    );

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_CreateAnchorNow(AxisTimeAxis* axis) {
    if (!axis) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    std::lock_guard<std::mutex> anchors_lock(state->anchors_mutex);
    std::lock_guard<std::mutex> state_lock(state->state_mutex);
    std::lock_guard<std::mutex> trans_lock(state->transitions_mutex);

    AxisSlotIndex current = state->current_slot.load();

    AnchorData anchor;
    anchor.anchor_id = state->next_anchor_id.fetch_add(1);
    anchor.slot_index = current;
    anchor.state_snapshot = state->current_state;

    // Copy transition log for this anchor (for future reconstruction)
    anchor.transition_log = {};
    for (const auto& trans : state->pending_transitions) {
        for (const auto& req : trans.requests) {
            anchor.transition_log.push_back(req);
        }
    }

    // Compute hashes
    ComputeTransitionHash(state->pending_transitions, anchor.transition_hash);

    // Resolution hash would be computed during resolution (not available here)
    std::memset(anchor.resolution_hash, 0, 16);

    // Store the axis's IMMUTABLE termination policy hash
    // This anchor inherits the Time Axis's semantic identity
    anchor.termination_policy_hash = state->termination_policy_hash;

    state->anchors.push_back(std::move(anchor));
    state->last_anchor_slot = current;

    // Clear pending transitions (they're now part of the anchor)
    state->pending_transitions.clear();

    // Prune old anchors if needed
    while (state->anchors.size() > state->config.max_anchors) {
        state->anchors.erase(state->anchors.begin());
    }

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetAnchorInterval(
    AxisTimeAxis* axis,
    uint32_t interval
) {
    if (!axis || interval == 0) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);
    state->config.anchor_interval = interval;

    return AXIS_TIME_OK;
}

// =============================================================================
// State Reconstruction (Deterministic Replay)
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_ReconstructState(
    const AxisTimeAxis* axis,
    AxisSlotIndex slot_index,
    AxisConflictGroupId group_id,
    AxisStateEnumerator enumerator,
    void* user_data
) {
    if (!axis || !enumerator) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);

    // Find the anchor at or before the requested slot
    const AnchorData* target_anchor = nullptr;
    std::vector<SlotTransition> relevant_transitions;
    std::vector<ConflictGroupData> groups_copy;

    {
        std::lock_guard<std::mutex> anchors_lock(const_cast<std::mutex&>(state->anchors_mutex));
        std::lock_guard<std::mutex> trans_lock(const_cast<std::mutex&>(state->transitions_mutex));
        std::lock_guard<std::mutex> groups_lock(const_cast<std::mutex&>(state->groups_mutex));

        if (state->anchors.empty()) {
            return AXIS_TIME_ERROR_ANCHOR_NOT_FOUND;
        }

        if (slot_index < state->anchors.front().slot_index) {
            return AXIS_TIME_ERROR_RECONSTRUCTION_FAILED;
        }

        // Find nearest anchor before target slot
        for (auto it = state->anchors.rbegin(); it != state->anchors.rend(); ++it) {
            if (it->slot_index <= slot_index) {
                target_anchor = &(*it);
                break;
            }
        }

        if (!target_anchor) {
            return AXIS_TIME_ERROR_ANCHOR_NOT_FOUND;
        }

        // CRITICAL: Verify anchor compatibility
        // "A termination policy is part of the Time Axis definition, not part of gameplay logic."
        // Anchors with different policies are INCOMPATIBLE
        if (target_anchor->termination_policy_hash != state->termination_policy_hash) {
            return AXIS_TIME_ERROR_POLICY_MISMATCH;
        }

        // Collect transitions from anchor to target slot
        for (const auto& trans : state->pending_transitions) {
            if (trans.slot_index > target_anchor->slot_index &&
                trans.slot_index <= slot_index) {
                relevant_transitions.push_back(trans);
            }
        }

        groups_copy = state->conflict_groups;
    }

    // Deterministically replay from anchor to target slot
    std::unordered_map<uint64_t, AxisStateValue> reconstructed_state;

    if (!ReplayTransitionsToSlot(*target_anchor, relevant_transitions, slot_index, groups_copy, reconstructed_state)) {
        return AXIS_TIME_ERROR_RECONSTRUCTION_FAILED;
    }

    // Enumerate the reconstructed state
    for (const auto& [key_hash, value] : reconstructed_state) {
        AxisStateKey key;
        key.primary = key_hash;
        key.secondary = 0;

        if (enumerator(&key, &value, user_data) != 0) {
            break;
        }
    }

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_QueryState(
    const AxisTimeAxis* axis,
    AxisSlotIndex slot_index,
    const AxisStateKey* key,
    AxisStateValue* out_value
) {
    if (!axis || !key || !out_value) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);

    // For current slot, use current state directly (optimization)
    if (slot_index == state->current_slot.load()) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state->state_mutex));

        uint64_t key_hash = MakeStateKeyHash(*key);
        auto it = state->current_state.find(key_hash);
        if (it != state->current_state.end()) {
            *out_value = it->second;
            return AXIS_TIME_OK;
        }
        return AXIS_TIME_ERROR_NOT_FOUND;
    }

    // For past slots, find anchor and replay
    const AnchorData* target_anchor = nullptr;
    std::vector<SlotTransition> relevant_transitions;
    std::vector<ConflictGroupData> groups_copy;

    {
        std::lock_guard<std::mutex> anchors_lock(const_cast<std::mutex&>(state->anchors_mutex));
        std::lock_guard<std::mutex> trans_lock(const_cast<std::mutex&>(state->transitions_mutex));
        std::lock_guard<std::mutex> groups_lock(const_cast<std::mutex&>(state->groups_mutex));

        if (state->anchors.empty()) {
            return AXIS_TIME_ERROR_ANCHOR_NOT_FOUND;
        }

        // Find nearest anchor before target slot
        for (auto it = state->anchors.rbegin(); it != state->anchors.rend(); ++it) {
            if (it->slot_index <= slot_index) {
                target_anchor = &(*it);
                break;
            }
        }

        if (!target_anchor) {
            return AXIS_TIME_ERROR_ANCHOR_NOT_FOUND;
        }

        // CRITICAL: Verify anchor compatibility
        // "A termination policy is part of the Time Axis definition, not part of gameplay logic."
        // Anchors with different policies are INCOMPATIBLE
        if (target_anchor->termination_policy_hash != state->termination_policy_hash) {
            return AXIS_TIME_ERROR_POLICY_MISMATCH;
        }

        // Collect transitions
        for (const auto& trans : state->pending_transitions) {
            if (trans.slot_index > target_anchor->slot_index &&
                trans.slot_index <= slot_index) {
                relevant_transitions.push_back(trans);
            }
        }

        groups_copy = state->conflict_groups;
    }

    // Replay to get state at target slot
    std::unordered_map<uint64_t, AxisStateValue> reconstructed_state;

    if (!ReplayTransitionsToSlot(*target_anchor, relevant_transitions, slot_index, groups_copy, reconstructed_state)) {
        return AXIS_TIME_ERROR_RECONSTRUCTION_FAILED;
    }

    uint64_t key_hash = MakeStateKeyHash(*key);
    auto it = reconstructed_state.find(key_hash);
    if (it != reconstructed_state.end()) {
        *out_value = it->second;
        return AXIS_TIME_OK;
    }

    return AXIS_TIME_ERROR_NOT_FOUND;
}
