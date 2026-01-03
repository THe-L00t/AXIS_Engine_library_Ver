/**
 * @file axis_time_slot_types.h
 * @brief AXIS Single Time Axis - Core Types and Definitions
 *
 * This header defines the fundamental types for the Single Time Axis system.
 * The Time Axis is the authoritative execution and state transition model
 * for the AXIS Engine. All state changes flow through discrete Time Slots.
 *
 * Design Principles:
 * - Deterministic execution
 * - Bounded memory (no per-slot storage)
 * - Explainable behavior
 * - No hidden work
 *
 * @note C ABI compatible for cross-language support
 */

#ifndef AXIS_TIME_SLOT_TYPES_H
#define AXIS_TIME_SLOT_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// DLL Export/Import Macro
// =============================================================================

#ifdef _WIN32
    #ifdef AXISTIME_EXPORTS
        #define AXIS_TIME_API __declspec(dllexport)
    #else
        #define AXIS_TIME_API __declspec(dllimport)
    #endif
#else
    #define AXIS_TIME_API
#endif

// =============================================================================
// Forward Declarations (Opaque Pointers)
// =============================================================================

/** @brief Opaque handle to the Time Axis system */
typedef struct AxisTimeAxis AxisTimeAxis;

/** @brief Opaque handle to a State Change Request */
typedef struct AxisStateRequest AxisStateRequest;

/** @brief Opaque handle to a Conflict Group */
typedef struct AxisConflictGroup AxisConflictGroup;

/** @brief Opaque handle to an Anchor */
typedef struct AxisTimeAnchor AxisTimeAnchor;

// =============================================================================
// Basic Types
// =============================================================================

/** @brief Time Slot index - monotonically increasing, unbounded conceptually */
typedef uint64_t AxisSlotIndex;

/** @brief Conflict Group identifier */
typedef uint32_t AxisConflictGroupId;

/** @brief Request priority for conflict resolution */
typedef int32_t AxisRequestPriority;

/**
 * @brief Reconstruction Key - Encodes HOW to reconstruct, NOT the state itself
 *
 * CRITICAL PHILOSOPHY:
 * "A reconstruction key does not encode a state.
 *  It encodes how to reconstruct a state from an anchor."
 *
 * The key alone CANNOT restore state. You need:
 *   Anchor + Key + Deterministic Transition Engine → State
 *
 * This key tells the reconstruction engine:
 *   1. Which anchor to start from (anchor_id)
 *   2. Which slot to reach (target_slot)
 *   3. How to verify the transition path (transition_hash)
 *   4. How to verify conflict resolution was deterministic (policy_hash)
 */
typedef struct AxisReconstructionKey {
    uint64_t anchor_id;           /** Starting Anchor identifier */
    uint64_t target_slot;         /** Target slot to reconstruct */
    uint8_t  transition_hash[16]; /** Hash verifying replay path correctness */
    uint8_t  policy_hash[16];     /** Hash verifying conflict resolution determinism */
} AxisReconstructionKey;

/** @brief Request identifier for tracking */
typedef uint64_t AxisRequestId;

// =============================================================================
// Constants
// =============================================================================

/** @brief Invalid slot index sentinel */
#define AXIS_SLOT_INVALID       ((AxisSlotIndex)UINT64_MAX)

/** @brief Invalid conflict group sentinel */
#define AXIS_CONFLICT_GROUP_INVALID ((AxisConflictGroupId)UINT32_MAX)

/** @brief Invalid request ID sentinel */
#define AXIS_REQUEST_ID_INVALID ((AxisRequestId)UINT64_MAX)

/** @brief Default anchor interval (slots between anchors) */
#define AXIS_DEFAULT_ANCHOR_INTERVAL 1024

/** @brief Maximum concurrent conflict groups */
#define AXIS_MAX_CONFLICT_GROUPS 256

// =============================================================================
// Result Codes
// =============================================================================

/**
 * @brief Result codes for Time Axis operations
 */
typedef enum AxisTimeResult {
    AXIS_TIME_OK = 0,
    AXIS_TIME_ERROR_INVALID_PARAMETER = 1,
    AXIS_TIME_ERROR_OUT_OF_MEMORY = 2,
    AXIS_TIME_ERROR_NOT_INITIALIZED = 3,
    AXIS_TIME_ERROR_ALREADY_INITIALIZED = 4,
    AXIS_TIME_ERROR_SLOT_IN_PAST = 5,
    AXIS_TIME_ERROR_CONFLICT_GROUP_FULL = 6,
    AXIS_TIME_ERROR_REQUEST_QUEUE_FULL = 7,
    AXIS_TIME_ERROR_ANCHOR_NOT_FOUND = 8,
    AXIS_TIME_ERROR_RECONSTRUCTION_FAILED = 9,
    AXIS_TIME_ERROR_INVALID_POLICY = 10,
    AXIS_TIME_ERROR_THREAD_POOL_FAILED = 11,
    AXIS_TIME_ERROR_NOT_FOUND = 12,
    /**
     * @brief Anchor's termination policy hash does not match the current Time Axis.
     *
     * PHILOSOPHY:
     * "A termination policy is part of the Time Axis definition, not part of gameplay logic."
     *
     * This error occurs when attempting to use an anchor created with a different
     * termination policy. Anchors are INCOMPATIBLE across different policy semantics.
     * If you need different termination logic, create a NEW Time Axis.
     */
    AXIS_TIME_ERROR_POLICY_MISMATCH = 13,
    /**
     * @brief Termination policy is locked and cannot be modified.
     *
     * PHILOSOPHY:
     * "A termination policy is part of the Time Axis definition, not part of gameplay logic."
     *
     * The termination policy is IMMUTABLE after Time Axis creation.
     * Use AxisTimeAxisConfig.termination_config to set the policy at creation time.
     */
    AXIS_TIME_ERROR_POLICY_LOCKED = 14,
    /**
     * @brief Time Axis has terminated and cannot be ticked further.
     *
     * PHILOSOPHY:
     * "Once time decides to stop, it cannot be restarted.
     *  A terminated axis is semantically complete."
     *
     * This error occurs when attempting to Tick() an axis that has already
     * reached a termination condition. The axis lifecycle has transitioned to TERMINATED.
     *
     * To continue execution, create a NEW Time Axis.
     */
    AXIS_TIME_ERROR_TERMINATED = 15,
} AxisTimeResult;

// =============================================================================
// Conflict Resolution Policies
// =============================================================================

/**
 * @brief Built-in conflict resolution policy types
 *
 * When multiple requests target the same slot AND same conflict group,
 * a policy determines the final state.
 */
typedef enum AxisConflictPolicy {
    /** Higher priority value wins */
    AXIS_POLICY_PRIORITY = 0,

    /** Last submitted request wins (deterministic by request ID) */
    AXIS_POLICY_LAST_WRITER = 1,

    /** First submitted request wins (deterministic by request ID) */
    AXIS_POLICY_FIRST_WRITER = 2,

    /** Custom user-defined policy function */
    AXIS_POLICY_CUSTOM = 3,
} AxisConflictPolicy;

// =============================================================================
// State Change Request Types
// =============================================================================

/**
 * @brief Type of state mutation
 */
typedef enum AxisMutationType {
    /** Set a value (overwrites previous) */
    AXIS_MUTATION_SET = 0,

    /** Add/increment a value */
    AXIS_MUTATION_ADD = 1,

    /** Multiply a value */
    AXIS_MUTATION_MULTIPLY = 2,

    /** Delete/clear a value */
    AXIS_MUTATION_DELETE = 3,

    /** Custom mutation with user data */
    AXIS_MUTATION_CUSTOM = 4,
} AxisMutationType;

/**
 * @brief Fixed-size state key (identifies what is being mutated)
 */
typedef struct AxisStateKey {
    uint64_t primary;    /** Primary key component */
    uint64_t secondary;  /** Secondary key component (e.g., field ID) */
} AxisStateKey;

/**
 * @brief State value union for mutations
 *
 * @note Fixed 64-bit value for determinism and simplicity.
 *       Complex data should use indirection via handles.
 */
typedef union AxisStateValue {
    int64_t  as_int;
    uint64_t as_uint;
    double   as_float;
    void*    as_ptr;  /** For custom mutation - user manages lifetime */
} AxisStateValue;

/**
 * @brief Describes a single state change request
 */
typedef struct AxisStateChangeDesc {
    AxisSlotIndex       target_slot;      /** Target slot for this change */
    AxisConflictGroupId conflict_group;   /** Which conflict group this belongs to */
    AxisRequestPriority priority;         /** Priority for resolution (if applicable) */
    AxisStateKey        key;              /** What is being changed */
    AxisMutationType    mutation_type;    /** How to change it */
    AxisStateValue      value;            /** New value */
} AxisStateChangeDesc;

// =============================================================================
// Custom Policy Callback
// =============================================================================

/**
 * @brief Custom conflict resolution callback
 *
 * Called when AXIS_POLICY_CUSTOM is used and multiple requests conflict.
 *
 * @param group_id      The conflict group being resolved
 * @param requests      Array of conflicting request descriptions
 * @param request_count Number of conflicting requests
 * @param out_winner    Output: index of the winning request (0 to request_count-1)
 * @param user_data     User-provided context
 *
 * @return 0 on success, non-zero to indicate error (uses first request as fallback)
 *
 * @note This function MUST be deterministic - same inputs must produce same output.
 * @note This function is called from worker threads - must be thread-safe.
 */
typedef int (*AxisCustomPolicyFn)(
    AxisConflictGroupId group_id,
    const AxisStateChangeDesc* requests,
    size_t request_count,
    size_t* out_winner,
    void* user_data
);

// =============================================================================
// Configuration Structures
// =============================================================================

/**
 * @brief Configuration for creating a Time Axis
 *
 * PHILOSOPHY:
 * "A termination policy is part of the Time Axis definition, not part of gameplay logic."
 *
 * The termination policy is set at creation and becomes IMMUTABLE for the lifetime
 * of the Time Axis. If you need different termination logic, create a NEW Time Axis.
 *
 * All anchors in a Time Axis share the same termination policy hash.
 * Anchors created with different policies are INCOMPATIBLE.
 */
typedef struct AxisTimeAxisConfig {
    /** Number of worker threads for parallel resolution (0 = auto-detect) */
    uint32_t worker_thread_count;

    /** Maximum pending requests in the queue */
    uint32_t max_pending_requests;

    /** Interval between anchors (in slots) */
    uint32_t anchor_interval;

    /** Maximum number of anchors to retain (bounded memory) */
    uint32_t max_anchors;

    /** Initial capacity for conflict groups */
    uint32_t initial_conflict_group_capacity;

    /**
     * Termination policy configuration (IMMUTABLE after creation)
     *
     * If NULL, default termination config is used.
     * Once the Time Axis is created, this policy cannot be changed.
     * The policy hash is computed once at creation and used for
     * anchor compatibility verification.
     *
     * @note Use AxisTermination_DefaultConfig() to get a default config,
     *       then modify as needed before passing to AxisTimeAxis_Create().
     */
    const struct AxisTerminationConfig* termination_config;
} AxisTimeAxisConfig;

/**
 * @brief Returns default configuration
 */
AXIS_TIME_API AxisTimeAxisConfig AxisTimeAxis_DefaultConfig(void);

// =============================================================================
// Statistics and Debug Info
// =============================================================================

/**
 * @brief Statistics for the Time Axis system
 */
typedef struct AxisTimeAxisStats {
    AxisSlotIndex current_slot;           /** Current slot index */
    AxisSlotIndex oldest_reconstructible; /** Oldest slot that can be reconstructed */
    uint64_t      total_requests_processed;
    uint64_t      total_conflicts_resolved;
    uint32_t      active_conflict_groups;
    uint32_t      current_anchor_count;
    size_t        memory_usage_bytes;
} AxisTimeAxisStats;

/**
 * @brief Debug callback for slot transitions
 *
 * @param slot_index    The slot that was just committed
 * @param changes_count Number of state changes in this slot
 * @param user_data     User-provided context
 */
typedef void (*AxisSlotCommitCallback)(
    AxisSlotIndex slot_index,
    size_t changes_count,
    void* user_data
);

/**
 * @brief Callback for receiving reconstructed state
 *
 * @param key        State key
 * @param value      State value at the requested slot
 * @param user_data  User-provided context
 *
 * @return 0 to continue enumeration, non-zero to stop
 */
typedef int (*AxisStateEnumerator)(
    const AxisStateKey* key,
    const AxisStateValue* value,
    void* user_data
);

// =============================================================================
// Slot Termination Policy System
// =============================================================================

// =============================================================================
// Causality Axis Extension Point (FORWARD COMPATIBILITY)
// =============================================================================

/**
 * @brief Causality Summary - Abstract causal dependency metrics
 *
 * PHILOSOPHY:
 * "Time decides when the world progresses.
 *  Causality decides why the world changes.
 *  Termination decides whether time itself is allowed to continue."
 *
 * This struct represents a FUTURE extension point for a Causality Axis
 * that tracks causal dependencies and state evolution.
 *
 * CRITICAL RULES:
 * - Termination policy MUST NOT depend on concrete state data
 * - Only meta-observations (counts, flags, summaries) are permitted
 * - This struct is UNUSED in current implementation
 * - Reserved for future Causality / Data Axis integration
 *
 * @note NOT IMPLEMENTED YET - Placeholder only
 * @note Will NOT affect policy hashing until actually used
 * @note Ensures ABI stability for future causality features
 */
typedef struct AxisCausalitySummary {
    /** Number of causal events processed (future: state mutations, effects, etc.) */
    uint64_t causal_event_count;

    /** Number of unresolved causal dependencies (future: pending effects, deferred actions) */
    uint64_t unresolved_dependencies;

    /** Number of committed state mutations (future: finalized state changes) */
    uint64_t committed_mutations;
} AxisCausalitySummary;

/**
 * @brief Slot Termination Context (LOW COST, POD)
 *
 * PHILOSOPHY:
 * "A time slot does not end because time passed.
 *  It ends because the engine has decided there is nothing left — or must stop."
 *
 * This struct provides the minimal, cache-friendly context for termination decisions.
 * Updated incrementally during slot execution.
 *
 * SEMANTIC CONTRACT (IMMUTABLE):
 * These semantics MUST NOT change without breaking compatibility.
 *
 *   elapsed_steps     : Cumulative, monotonic, increments once per completed Tick
 *                       Resets to 0 only on Time Axis creation
 *                       Never decreases
 *
 *   pending_requests  : Snapshot of ALL pending requests at end of Tick
 *                       Count of requests still in queue (not yet processed)
 *                       Does NOT include requests processed in current tick
 *
 *   resolved_groups   : Number of groups SUCCESSFULLY resolved in THIS Tick only
 *                       Only counts groups that completed resolution and committed results
 *                       Does NOT count groups that failed or deferred
 *
 *   total_groups      : Number of conflict groups observed in THIS Tick
 *                       Includes all groups that had requests, regardless of resolution status
 *                       May differ from resolved_groups if some groups fail
 *
 *   external_flags    : Snapshot of external runtime signals at evaluation time
 *                       Bitmask of AxisExternalSignalFlag values
 *                       Updated atomically before termination evaluation
 *
 *   causality_summary : Optional causality metrics (FUTURE - currently NULL)
 *                       Reserved for future Causality Axis integration
 *                       Does NOT affect current termination logic
 *
 * @note Must be trivially copyable
 * @note No heap allocation
 * @note Safe for deterministic evaluation
 * @note Termination policy belongs to Time Axis definition, NOT gameplay logic
 */
typedef struct AxisSlotTerminationContext {
    uint32_t elapsed_steps;      /** Cumulative ticks executed (monotonic, never resets) */
    uint32_t pending_requests;   /** Snapshot of remaining queue size */
    uint32_t resolved_groups;    /** Groups that completed resolution THIS tick */
    uint32_t total_groups;       /** Groups observed THIS tick */
    uint32_t external_flags;     /** Runtime signal bitmask */

    /**
     * @brief Optional causality metrics (FUTURE EXTENSION)
     *
     * Currently ALWAYS NULL. Reserved for future Causality Axis.
     * Does NOT participate in termination decisions yet.
     * Does NOT affect policy hashing.
     *
     * Future usage:
     *   Termination may consider abstract causality metrics
     *   e.g., "terminate when all causal dependencies resolved"
     *   BUT NEVER direct inspection of state data
     */
    const AxisCausalitySummary* causality_summary;
} AxisSlotTerminationContext;

/**
 * @brief External signal flags for termination decisions
 */
typedef enum AxisExternalSignalFlag {
    AXIS_SIGNAL_NONE            = 0,
    AXIS_SIGNAL_NETWORK_SYNC    = (1 << 0),  /** Network frame synchronization */
    AXIS_SIGNAL_SERVER_AUTHORITY = (1 << 1), /** Server authority signal */
    AXIS_SIGNAL_SCENE_TRANSITION = (1 << 2), /** Scene/level transition */
    AXIS_SIGNAL_PAUSE_REQUEST   = (1 << 3),  /** Pause requested */
    AXIS_SIGNAL_FORCE_COMMIT    = (1 << 4),  /** Force immediate commit */
    AXIS_SIGNAL_USER_DEFINED_1  = (1 << 16), /** User-defined signal 1 */
    AXIS_SIGNAL_USER_DEFINED_2  = (1 << 17), /** User-defined signal 2 */
    AXIS_SIGNAL_USER_DEFINED_3  = (1 << 18), /** User-defined signal 3 */
    AXIS_SIGNAL_USER_DEFINED_4  = (1 << 19), /** User-defined signal 4 */
} AxisExternalSignalFlag;

/**
 * @brief Termination reason (for debugging and logging)
 */
typedef enum AxisTerminationReason {
    AXIS_TERMINATION_NONE = 0,            /** Slot has not terminated */
    AXIS_TERMINATION_SAFETY_CAP,          /** Hard upper bound reached */
    AXIS_TERMINATION_STEP_LIMIT,          /** Step count limit reached */
    AXIS_TERMINATION_REQUEST_DRAIN,       /** All requests processed */
    AXIS_TERMINATION_GROUP_RESOLUTION,    /** All conflict groups resolved */
    AXIS_TERMINATION_EXTERNAL_SIGNAL,     /** External signal received */
    AXIS_TERMINATION_CUSTOM_CALLBACK,     /** Custom callback decided to terminate */
} AxisTerminationReason;

/**
 * @brief Custom slot termination callback (C ABI safe)
 *
 * Called LAST in the termination evaluation order.
 * Allows developers to inject custom termination logic.
 *
 * @param context   Current termination context (read-only)
 * @param user_data User-provided context
 *
 * @return Non-zero to terminate the slot, zero to continue
 *
 * RULES:
 * - Callback must NOT mutate engine state
 * - Callback result MUST be deterministic (same inputs → same output)
 * - Callback presence affects policy_hash for replay verification
 */
typedef int (*AxisSlotTerminationCallback)(
    const AxisSlotTerminationContext* context,
    void* user_data
);

/**
 * @brief Termination policy configuration
 *
 * Evaluation order (DETERMINISTIC CONTRACT):
 * 1. Safety Cap (ALWAYS checked first, overrides all)
 * 2. Step Limit
 * 3. Request Drain
 * 4. Group Resolution
 * 5. External Signal
 * 6. Custom Callback (if any)
 */
typedef struct AxisTerminationConfig {
    /** Step limit (0 = disabled) */
    uint32_t step_limit;

    /** Safety cap - hard upper bound (0 = disabled, but NOT recommended) */
    uint32_t safety_cap;

    /** Terminate when all pending requests are processed */
    int terminate_on_request_drain;

    /** Terminate when all conflict groups are resolved */
    int terminate_on_group_resolution;

    /** Required external flags mask (0 = disabled) */
    uint32_t required_external_flags;

    /** Custom termination callback (NULL = disabled) */
    AxisSlotTerminationCallback custom_callback;

    /** User data for custom callback */
    void* custom_callback_user_data;
} AxisTerminationConfig;

/**
 * @brief Returns default termination configuration
 *
 * Default: Safety cap of 10000 steps, no other conditions
 */
AXIS_TIME_API AxisTerminationConfig AxisTermination_DefaultConfig(void);

#ifdef __cplusplus
}
#endif

#endif // AXIS_TIME_SLOT_TYPES_H
