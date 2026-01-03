/**
 * @file termination.cpp
 * @brief Slot Termination Policy System Implementation
 *
 * PHILOSOPHY:
 * "A time slot does not end because time passed.
 *  It ends because the engine has decided there is nothing left — or must stop."
 *
 * This file implements:
 * - Built-in termination conditions (5 types)
 * - Custom callback support
 * - Termination evaluation in deterministic order
 * - External signal management
 *
 * Evaluation Order (DETERMINISTIC CONTRACT):
 * 1. Safety Cap (ALWAYS first, overrides all)
 * 2. Step Limit
 * 3. Request Drain
 * 4. Group Resolution
 * 5. External Signal
 * 6. Custom Callback (if any)
 */

#include "pch.h"
#include "slot_internal.h"

using namespace axis::time::internal;

// =============================================================================
// Default Configuration
// =============================================================================

extern "C" AXIS_TIME_API AxisTerminationConfig AxisTermination_DefaultConfig(void) {
    AxisTerminationConfig config{};
    config.step_limit = 0;                    // Disabled
    config.safety_cap = 10000;                // Hard upper bound
    config.terminate_on_request_drain = 0;    // Disabled
    config.terminate_on_group_resolution = 0; // Disabled
    config.required_external_flags = 0;       // Disabled
    config.custom_callback = nullptr;
    config.custom_callback_user_data = nullptr;
    return config;
}

// =============================================================================
// BuiltinTerminationPolicy Implementation
// =============================================================================

namespace axis::time::internal {

bool BuiltinTerminationPolicy::ShouldTerminate(const AxisSlotTerminationContext& ctx) const {
    return Evaluate(ctx) != AXIS_TERMINATION_NONE;
}

AxisTerminationReason BuiltinTerminationPolicy::Evaluate(const AxisSlotTerminationContext& ctx) const {
    // Evaluation order is CRITICAL for determinism

    // 1. Safety Cap (ALWAYS first, overrides all)
    if (config.safety_cap > 0 && ctx.elapsed_steps >= config.safety_cap) {
        return AXIS_TERMINATION_SAFETY_CAP;
    }

    // 2. Step Limit
    if (config.step_limit > 0 && ctx.elapsed_steps >= config.step_limit) {
        return AXIS_TERMINATION_STEP_LIMIT;
    }

    // 3. Request Drain
    if (config.terminate_on_request_drain && ctx.pending_requests == 0) {
        return AXIS_TERMINATION_REQUEST_DRAIN;
    }

    // 4. Group Resolution
    if (config.terminate_on_group_resolution &&
        ctx.total_groups > 0 &&
        ctx.resolved_groups >= ctx.total_groups) {
        return AXIS_TERMINATION_GROUP_RESOLUTION;
    }

    // 5. External Signal
    if (config.required_external_flags != 0 &&
        (ctx.external_flags & config.required_external_flags) != 0) {
        return AXIS_TERMINATION_EXTERNAL_SIGNAL;
    }

    // 6. Custom Callback (LAST)
    if (config.custom_callback != nullptr) {
        if (config.custom_callback(&ctx, config.custom_callback_user_data) != 0) {
            return AXIS_TERMINATION_CUSTOM_CALLBACK;
        }
    }

    return AXIS_TERMINATION_NONE;
}

uint64_t BuiltinTerminationPolicy::GetPolicyHash() const {
    // Hash the configuration for determinism verification
    uint64_t hash = 0x9e3779b97f4a7c15ULL;

    hash ^= config.step_limit;
    hash *= 0x100000001b3ULL;
    hash ^= config.safety_cap;
    hash *= 0x100000001b3ULL;
    hash ^= static_cast<uint64_t>(config.terminate_on_request_drain);
    hash *= 0x100000001b3ULL;
    hash ^= static_cast<uint64_t>(config.terminate_on_group_resolution);
    hash *= 0x100000001b3ULL;
    hash ^= config.required_external_flags;
    hash *= 0x100000001b3ULL;

    // Callback presence affects hash (for replay verification)
    if (config.custom_callback != nullptr) {
        hash ^= 0xDEADBEEFCAFEBABEULL;
    }

    return hash;
}

} // namespace axis::time::internal

// =============================================================================
// Public API - Individual Termination Conditions (DEPRECATED)
// =============================================================================
//
// PHILOSOPHY:
// "A termination policy is part of the Time Axis definition, not part of gameplay logic."
//
// All setter functions below are DEPRECATED and return AXIS_TIME_ERROR_POLICY_LOCKED.
// Configure termination policy at Time Axis creation via AxisTimeAxisConfig.termination_config.
//
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationByStepLimit(
    AxisTimeAxis* axis,
    uint32_t /*max_steps*/
) {
    (void)axis;  // Suppress unused parameter warning
    // DEPRECATED: Policy is IMMUTABLE after Time Axis creation
    return AXIS_TIME_ERROR_POLICY_LOCKED;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationOnRequestDrain(
    AxisTimeAxis* axis,
    int /*enabled*/
) {
    (void)axis;
    // DEPRECATED: Policy is IMMUTABLE after Time Axis creation
    return AXIS_TIME_ERROR_POLICY_LOCKED;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationOnGroupResolution(
    AxisTimeAxis* axis,
    int /*enabled*/
) {
    (void)axis;
    // DEPRECATED: Policy is IMMUTABLE after Time Axis creation
    return AXIS_TIME_ERROR_POLICY_LOCKED;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationOnExternalSignal(
    AxisTimeAxis* axis,
    uint32_t /*required_flags_mask*/
) {
    (void)axis;
    // DEPRECATED: Policy is IMMUTABLE after Time Axis creation
    return AXIS_TIME_ERROR_POLICY_LOCKED;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationSafetyCap(
    AxisTimeAxis* axis,
    uint32_t /*max_steps_cap*/
) {
    (void)axis;
    // DEPRECATED: Policy is IMMUTABLE after Time Axis creation
    return AXIS_TIME_ERROR_POLICY_LOCKED;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetCustomTerminationCallback(
    AxisTimeAxis* axis,
    AxisSlotTerminationCallback /*callback*/,
    void* /*user_data*/
) {
    (void)axis;
    // DEPRECATED: Policy is IMMUTABLE after Time Axis creation
    return AXIS_TIME_ERROR_POLICY_LOCKED;
}

// =============================================================================
// Public API - Full Configuration (DEPRECATED)
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetTerminationConfig(
    AxisTimeAxis* axis,
    const AxisTerminationConfig* /*config*/
) {
    (void)axis;
    // DEPRECATED: Policy is IMMUTABLE after Time Axis creation
    return AXIS_TIME_ERROR_POLICY_LOCKED;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_GetTerminationConfig(
    const AxisTimeAxis* axis,
    AxisTerminationConfig* out_config
) {
    if (!axis || !out_config) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);

    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state->termination_mutex));
    *out_config = state->termination_policy.config;

    return AXIS_TIME_OK;
}

// =============================================================================
// Public API - External Signals
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetExternalSignal(
    AxisTimeAxis* axis,
    uint32_t flag
) {
    if (!axis) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    // Thread-safe atomic OR
    state->external_flags.fetch_or(flag);

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_ClearExternalSignal(
    AxisTimeAxis* axis,
    uint32_t flag
) {
    if (!axis) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<TimeAxisState*>(axis);

    // Thread-safe atomic AND with inverted flag
    state->external_flags.fetch_and(~flag);

    return AXIS_TIME_OK;
}

// =============================================================================
// Public API - Context and Reason
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_GetTerminationContext(
    const AxisTimeAxis* axis,
    AxisSlotTerminationContext* out_context
) {
    if (!axis || !out_context) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);

    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state->termination_mutex));
    *out_context = state->termination_context;

    // Update external flags from atomic
    out_context->external_flags = state->external_flags.load();

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTerminationReason AxisTimeAxis_GetLastTerminationReason(
    const AxisTimeAxis* axis
) {
    if (!axis) {
        return AXIS_TERMINATION_NONE;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);
    return state->last_termination_reason;
}

// =============================================================================
// Public API - Termination Policy Hash
// =============================================================================

extern "C" AXIS_TIME_API uint64_t AxisTimeAxis_GetTerminationPolicyHash(
    const AxisTimeAxis* axis
) {
    if (!axis) {
        return 0;
    }

    const auto* state = reinterpret_cast<const TimeAxisState*>(axis);

    // Return the IMMUTABLE policy hash computed at creation
    // This value NEVER changes after Time Axis creation
    // "A termination policy is part of the Time Axis definition, not part of gameplay logic."
    return state->termination_policy_hash;
}
