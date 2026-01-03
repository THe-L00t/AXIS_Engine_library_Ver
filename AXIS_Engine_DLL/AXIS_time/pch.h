/**
 * @file pch.h
 * @brief Precompiled header for AXIS Time module
 *
 * Contains commonly used headers to improve compilation performance.
 */

#ifndef PCH_H
#define PCH_H

// Windows headers
#include "framework.h"

// Standard library headers used across the module
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <functional>
#include <memory>
#include <queue>
#include <algorithm>
#include <cstdint>
#include <cstring>

// Public API headers
#include "include/axis/time/axis_time_slot_types.h"

#endif // PCH_H
