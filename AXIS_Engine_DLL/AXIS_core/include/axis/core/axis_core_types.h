/**
 * @file axis_core_types.h
 * @brief AXIS Core common types and definitions
 *
 * This header defines the fundamental types, macros, and enums used throughout
 * the AXIS Core system. It follows C ABI for cross-language compatibility.
 */

#ifndef AXIS_CORE_TYPES_H
#define AXIS_CORE_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// DLL Export/Import Macro
// =============================================================================

#ifdef _WIN32
    #ifdef AXISCORE_EXPORTS
        #define AXIS_API __declspec(dllexport)
    #else
        #define AXIS_API __declspec(dllimport)
    #endif
#else
    #define AXIS_API
#endif

// =============================================================================
// Result Codes
// =============================================================================

/**
 * @brief Result codes for AXIS Core operations
 */
typedef enum AxisResult {
    AXIS_OK = 0,
    AXIS_ERROR_INVALID_PARAMETER = 1,
    AXIS_ERROR_OUT_OF_MEMORY = 2,
    AXIS_ERROR_NOT_INITIALIZED = 3,
    AXIS_ERROR_ALREADY_INITIALIZED = 4,
    AXIS_ERROR_OVERFLOW = 5,
    AXIS_ERROR_UNDERFLOW = 6,
    AXIS_ERROR_NOT_FOUND = 7,
} AxisResult;

// =============================================================================
// Memory Tags
// =============================================================================

/**
 * @brief Memory allocation tags for tracking and debugging
 *
 * Each allocation is tagged to track which subsystem is using memory.
 * This enables detailed memory profiling and leak detection.
 */
typedef enum AxisMemoryTag {
    AXIS_MEMORY_TAG_CORE = 0,
    AXIS_MEMORY_TAG_RENDERER = 1,
    AXIS_MEMORY_TAG_RESOURCE = 2,
    AXIS_MEMORY_TAG_AUDIO = 3,
    AXIS_MEMORY_TAG_PHYSICS = 4,
    AXIS_MEMORY_TAG_TEMP = 5,
    AXIS_MEMORY_TAG_COUNT
} AxisMemoryTag;

// =============================================================================
// Memory Statistics
// =============================================================================

/**
 * @brief Statistics for a single memory tag
 */
typedef struct AxisMemoryTagStats {
    size_t current_bytes;      /** Current allocated bytes */
    size_t peak_bytes;         /** Peak allocated bytes */
    size_t total_allocations;  /** Total number of allocations */
    size_t total_frees;        /** Total number of frees */
} AxisMemoryTagStats;

/**
 * @brief Overall memory system statistics
 */
typedef struct AxisMemoryStats {
    AxisMemoryTagStats tags[AXIS_MEMORY_TAG_COUNT];
    size_t total_current_bytes;
    size_t total_peak_bytes;
} AxisMemoryStats;

// =============================================================================
// Allocator Types
// =============================================================================

/**
 * @brief Type of allocator
 */
typedef enum AxisAllocatorType {
    AXIS_ALLOCATOR_TYPE_GENERAL = 0,
    AXIS_ALLOCATOR_TYPE_POOL = 1,
    AXIS_ALLOCATOR_TYPE_FRAME = 2,
} AxisAllocatorType;

#ifdef __cplusplus
}
#endif

#endif // AXIS_CORE_TYPES_H
