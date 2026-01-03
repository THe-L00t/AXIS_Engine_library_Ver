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

/** @brief Fixed-size reconstruction key (32 bytes) */
typedef struct AxisReconstructionKey {
    uint8_t data[32];
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

#ifdef __cplusplus
}
#endif

#endif // AXIS_TIME_SLOT_TYPES_H
