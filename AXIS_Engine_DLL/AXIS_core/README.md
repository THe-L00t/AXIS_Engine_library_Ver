# AXIS Core Memory System

> **Version**: 1.0
> **Date**: 2025-12-26
> **Status**: Initial Implementation

---

## Overview

AXIS Core Memory System is the foundational memory management layer for the AXIS engine.
It provides three distinct allocators, each designed for specific lifetime patterns:

- **General Allocator**: Variable-size, long-lifetime allocations
- **Pool Allocator**: Fixed-size objects with frequent allocation/deallocation
- **Frame Allocator**: Temporary allocations valid for one frame only

---

## Design Philosophy

### Three Axes Validation

This system passes the AXIS Core design criteria:

1. **TIME** ✅
   - Memory is organized by **lifetime**, not data structure
   - General: Long-term
   - Pool: Medium-term (frequent reuse)
   - Frame: Short-term (one frame)

2. **SPACE** ✅
   - Memory is the direct representation of space
   - All data exists in memory
   - Alignment and layout directly affect performance

3. **DATA** ✅
   - Tag-based tracking reveals data flow honestly
   - Statistics show which systems use how much memory
   - No hidden allocations

### Core Principles

- **No GC**: Explicit, predictable memory management
- **Tracked**: All allocations tagged and tracked
- **Lifetime-based**: Choose allocator by object lifetime, not type
- **C ABI**: Public API is C-compatible for cross-language use

---

## Directory Structure

```
AXIS_core/
├── include/axis/core/          # Public C API headers
│   ├── axis_core_types.h       # Common types, enums, result codes
│   └── axis_memory.h           # Memory system public API
│
├── src/memory/                 # Implementation
│   ├── memory_internal.h       # Internal utilities and stats tracker
│   ├── memory_internal.cpp     # Stats tracker implementation
│   ├── general_allocator.h/cpp # General allocator
│   ├── pool_allocator.h/cpp    # Pool allocator
│   ├── frame_allocator.h/cpp   # Frame allocator
│   └── axis_memory.cpp         # Public C API implementation
│
└── tests/                      # Test code
    └── memory_test.cpp         # Usage examples and tests
```

---

## Public API

### Initialization

```c
#include "axis/core/axis_memory.h"

// Initialize memory system
AxisMemoryConfig config = {
    .general_reserve_bytes = 1024 * 1024,  // 1MB
    .enable_statistics = 1
};
AxisResult result = Axis_InitializeMemory(&config);

// ... use memory system ...

// Shutdown
Axis_ShutdownMemory();
```

### General Allocator

Use for: Resource metadata, system state, variable-size long-lived data

```c
// Create allocator
AxisGeneralAllocator* alloc = Axis_CreateGeneralAllocator("MyAllocator", 1024 * 1024);

// Allocate
void* ptr = Axis_AllocGeneral(alloc, 256, 16, AXIS_MEMORY_TAG_CORE);

// Use memory...

// Free
Axis_FreeGeneral(alloc, ptr);

// Destroy allocator
Axis_DestroyGeneralAllocator(alloc);
```

### Pool Allocator

Use for: Entities, components, handles, any fixed-size frequently allocated objects

```c
// Create pool (100 objects of 64 bytes each)
AxisPoolAllocator* pool = Axis_CreatePoolAllocator("EntityPool", 64, 100, AXIS_MEMORY_TAG_CORE);

// Allocate object (O(1))
void* obj = Axis_AllocPool(pool);

// Use object...

// Free object (O(1))
Axis_FreePool(pool, obj);

// Check free count
size_t free = Axis_GetPoolFreeCount(pool);

// Destroy pool
Axis_DestroyPoolAllocator(pool);
```

### Frame Allocator

Use for: Render commands, temporary calculations, per-frame data

```c
// Create frame allocator (1KB capacity)
AxisFrameAllocator* frame = Axis_CreateFrameAllocator("FrameTemp", 1024, AXIS_MEMORY_TAG_TEMP);

// Allocate temporary data
void* temp = Axis_AllocFrame(frame, 128, 16);

// Use temporary data...

// At end of frame, reset all at once
Axis_ResetFrameAllocator(frame);

// Destroy
Axis_DestroyFrameAllocator(frame);
```

### Statistics

```c
// Get overall statistics
AxisMemoryStats stats;
Axis_GetMemoryStats(&stats);

printf("Total: %zu bytes\n", stats.total_current_bytes);
printf("Peak: %zu bytes\n", stats.total_peak_bytes);

// Get statistics for specific tag
AxisMemoryTagStats tag_stats;
Axis_GetTagStats(AXIS_MEMORY_TAG_RENDERER, &tag_stats);

printf("Renderer: current=%zu, peak=%zu, allocs=%zu, frees=%zu\n",
    tag_stats.current_bytes,
    tag_stats.peak_bytes,
    tag_stats.total_allocations,
    tag_stats.total_frees);
```

---

## Memory Tags

All allocations are tagged for tracking:

- `AXIS_MEMORY_TAG_CORE` - Core system allocations
- `AXIS_MEMORY_TAG_RENDERER` - Rendering subsystem
- `AXIS_MEMORY_TAG_RESOURCE` - Resource management
- `AXIS_MEMORY_TAG_AUDIO` - Audio subsystem
- `AXIS_MEMORY_TAG_PHYSICS` - Physics subsystem
- `AXIS_MEMORY_TAG_TEMP` - Temporary/frame allocations

---

## Implementation Details

### General Allocator

- **Strategy**: Wraps system malloc/free
- **Tracking**: Stores allocation headers with size, alignment, tag
- **Alignment**: Supports custom alignment (power of 2)
- **Thread-safe**: Yes (mutex-protected)
- **Overhead**: Allocation header + alignment padding

### Pool Allocator

- **Strategy**: Pre-allocated block + intrusive free-list
- **Allocation**: O(1) - pop from free-list
- **Deallocation**: O(1) - push to free-list
- **Thread-safe**: Yes (mutex-protected)
- **Overhead**: Max(object_size, sizeof(void*)) per object
- **Limitation**: Fixed capacity (no growth in v1)

### Frame Allocator

- **Strategy**: Bump allocator (linear allocation)
- **Allocation**: O(1) - advance pointer
- **Deallocation**: None (individual free prohibited)
- **Reset**: O(1) - reset pointer to start
- **Thread-safe**: Yes (mutex-protected)
- **Overhead**: Minimal (only alignment padding)

### Statistics Tracker

- **Storage**: Atomic counters per tag
- **Thread-safe**: Yes (lockless via atomics)
- **Tracking**: Current bytes, peak bytes, allocation count, free count
- **Overhead**: Can be disabled via config

---

## Build Instructions

### Requirements

- Visual Studio 2022 or later
- C++20 support
- Windows 10 SDK

### Build Steps

1. Open `AXIS_Engine_DLL.sln` in Visual Studio
2. Select configuration (Debug/Release)
3. Build solution (F7)
4. Output: `AXIS_core.dll` in appropriate output directory

### Integration

Include the public headers in your project:

```c
#include "axis/core/axis_core_types.h"
#include "axis/core/axis_memory.h"
```

Link against `AXIS_core.lib` (import library).

---

## Testing

Test code is provided in `tests/memory_test.cpp`.

To run tests:
1. Create a test executable project
2. Link against AXIS_core
3. Call `RunMemoryTests()` from your main()

Example output:
```
========================================
AXIS Memory System Test
========================================

Memory system initialized

=== Testing General Allocator ===
  Allocated 128 bytes at 0x...
  Allocated 256 bytes at 0x...
  ...
  General allocator test PASSED

=== Testing Pool Allocator ===
  ...
  Pool allocator test PASSED

=== Testing Frame Allocator ===
  ...
  Frame allocator test PASSED

========================================
All tests PASSED!
========================================
```

---

## Known Limitations (v1)

1. **Pool Allocator**: No automatic growth when exhausted
2. **Platform Dependency**: Uses std::malloc (should use platform layer in v2)
3. **No TLS Allocators**: All allocators are global
4. **No Defragmentation**: General allocator can fragment
5. **No Memory Poisoning**: Debug features limited

These are intentional for v1. Future versions will address them.

---

## Next Steps (v2+)

- Platform abstraction layer for OS memory
- Thread-local storage allocators
- Memory poisoning in debug builds
- Pool allocator growth policy
- Slab allocator for common sizes
- Integration with logging system

---

## Design Decisions Log

### Why three allocators instead of one general-purpose?

**Decision**: Separate allocators by lifetime pattern
**Reason**: Performance and clarity

- Different lifetimes have different optimal strategies
- Frame allocator is 10x+ faster than general for temp data
- Pool allocator avoids fragmentation for fixed-size objects
- Clear API communicates intent

### Why C ABI for public API?

**Decision**: Pure C API with opaque pointers
**Reason**: Cross-language compatibility and ABI stability

- Allows use from C, Rust, or other languages
- No STL in public headers = stable ABI across compilers
- Internal implementation can use C++ freely

### Why not use STL allocators?

**Decision**: Custom allocators, not std::allocator-compatible
**Reason**: Simplicity and control

- STL allocator interface is complex
- We need custom tracking and statistics
- Direct control over allocation strategies
- Can add STL adapter later if needed

### Why store allocation metadata?

**Decision**: Track size, alignment, tag per allocation
**Reason**: Debugging and leak detection

- Enables leak detection on shutdown
- Allows verification of free() calls
- Supports statistics collection
- Minimal overhead for the value provided

---

## License

This is part of the AXIS Engine project.
See main project LICENSE for details.

---

## Contact

For questions or contributions, contact the AXIS Core team.
