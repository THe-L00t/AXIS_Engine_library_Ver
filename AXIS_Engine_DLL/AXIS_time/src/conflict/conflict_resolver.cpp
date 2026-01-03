/**
 * @file conflict_resolver.cpp
 * @brief Conflict resolution and worker pool implementation
 *
 * Implements:
 * - Thread pool for parallel conflict resolution
 * - Conflict resolution policies (priority, last-writer, first-writer, custom)
 * - Deterministic resolution regardless of thread scheduling
 */

#include "pch.h"
#include "../slot/slot_internal.h"
#include <algorithm>

namespace axis::time::internal {

// =============================================================================
// Worker Pool Implementation
// =============================================================================

WorkerPool::WorkerPool(uint32_t thread_count) {
    for (uint32_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back([this]() { WorkerThread(); });
    }
}

WorkerPool::~WorkerPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_.store(true);
    }
    condition_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void WorkerPool::Submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        tasks_.push(std::move(task));
        active_tasks_.fetch_add(1);
    }
    condition_.notify_one();
}

void WorkerPool::WaitAll() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    done_condition_.wait(lock, [this]() {
        return tasks_.empty() && active_tasks_.load() == 0;
    });
}

void WorkerPool::WorkerThread() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this]() {
                return stop_.load() || !tasks_.empty();
            });

            if (stop_.load() && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task) {
            task();
            uint32_t remaining = active_tasks_.fetch_sub(1) - 1;
            if (remaining == 0) {
                done_condition_.notify_all();
            }
        }
    }
}

// =============================================================================
// Conflict Resolution Logic
// =============================================================================

/**
 * @brief Resolves conflicts using priority policy
 *
 * Higher priority value wins. Ties are broken by request ID (lower ID wins).
 */
static bool ResolvePriorityPolicy(
    const std::vector<const PendingRequest*>& requests,
    size_t& out_winner
) {
    if (requests.empty()) return false;
    if (requests.size() == 1) {
        out_winner = 0;
        return true;
    }

    size_t winner = 0;
    AxisRequestPriority best_priority = requests[0]->desc.priority;
    AxisRequestId best_id = requests[0]->id;

    for (size_t i = 1; i < requests.size(); ++i) {
        AxisRequestPriority priority = requests[i]->desc.priority;
        AxisRequestId id = requests[i]->id;

        if (priority > best_priority ||
            (priority == best_priority && id < best_id)) {
            winner = i;
            best_priority = priority;
            best_id = id;
        }
    }

    out_winner = winner;
    return true;
}

/**
 * @brief Resolves conflicts using last-writer policy
 *
 * Highest request ID wins (deterministic "last" based on submission order).
 */
static bool ResolveLastWriterPolicy(
    const std::vector<const PendingRequest*>& requests,
    size_t& out_winner
) {
    if (requests.empty()) return false;
    if (requests.size() == 1) {
        out_winner = 0;
        return true;
    }

    size_t winner = 0;
    AxisRequestId best_id = requests[0]->id;

    for (size_t i = 1; i < requests.size(); ++i) {
        if (requests[i]->id > best_id) {
            winner = i;
            best_id = requests[i]->id;
        }
    }

    out_winner = winner;
    return true;
}

/**
 * @brief Resolves conflicts using first-writer policy
 *
 * Lowest request ID wins (deterministic "first" based on submission order).
 */
static bool ResolveFirstWriterPolicy(
    const std::vector<const PendingRequest*>& requests,
    size_t& out_winner
) {
    if (requests.empty()) return false;
    if (requests.size() == 1) {
        out_winner = 0;
        return true;
    }

    size_t winner = 0;
    AxisRequestId best_id = requests[0]->id;

    for (size_t i = 1; i < requests.size(); ++i) {
        if (requests[i]->id < best_id) {
            winner = i;
            best_id = requests[i]->id;
        }
    }

    out_winner = winner;
    return true;
}

bool ResolveConflictGroup(
    const ConflictGroupData& group,
    const std::vector<const PendingRequest*>& requests,
    GroupResolutionResult& out_result
) {
    out_result.group_id = group.id;
    out_result.resolved_changes.clear();
    out_result.change_hash = 0;

    if (requests.empty()) {
        return true;
    }

    // Group requests by state key
    std::unordered_map<uint64_t, std::vector<const PendingRequest*>> by_key;
    for (const auto* req : requests) {
        uint64_t key_hash = MakeStateKeyHash(req->desc.key);
        by_key[key_hash].push_back(req);
    }

    // Resolve each key's conflicts
    for (auto& [key_hash, key_requests] : by_key) {
        // Sort by request ID for determinism
        std::sort(key_requests.begin(), key_requests.end(),
            [](const PendingRequest* a, const PendingRequest* b) {
                return a->id < b->id;
            });

        size_t winner_index = 0;
        bool resolved = false;

        switch (group.policy) {
            case AXIS_POLICY_PRIORITY:
                resolved = ResolvePriorityPolicy(key_requests, winner_index);
                break;

            case AXIS_POLICY_LAST_WRITER:
                resolved = ResolveLastWriterPolicy(key_requests, winner_index);
                break;

            case AXIS_POLICY_FIRST_WRITER:
                resolved = ResolveFirstWriterPolicy(key_requests, winner_index);
                break;

            case AXIS_POLICY_CUSTOM:
                if (group.custom_fn) {
                    // Build array of descriptions for custom function
                    std::vector<AxisStateChangeDesc> descs;
                    descs.reserve(key_requests.size());
                    for (const auto* req : key_requests) {
                        descs.push_back(req->desc);
                    }

                    int result = group.custom_fn(
                        group.id,
                        descs.data(),
                        descs.size(),
                        &winner_index,
                        group.custom_user_data
                    );

                    resolved = (result == 0 && winner_index < key_requests.size());
                    if (!resolved) {
                        // Fallback to first writer
                        winner_index = 0;
                        resolved = true;
                    }
                } else {
                    // No custom function, fallback
                    winner_index = 0;
                    resolved = true;
                }
                break;

            default:
                winner_index = 0;
                resolved = true;
                break;
        }

        if (resolved && winner_index < key_requests.size()) {
            const PendingRequest* winner = key_requests[winner_index];

            // Apply mutation based on type
            AxisStateValue final_value = winner->desc.value;

            // For SET mutations, value is used directly
            // For ADD/MULTIPLY, we would need current state (simplified here)
            // DELETE mutations would remove the key

            if (winner->desc.mutation_type != AXIS_MUTATION_DELETE) {
                out_result.resolved_changes.emplace_back(
                    winner->desc.key,
                    final_value
                );
            }
        }
    }

    // Compute hash for determinism verification
    out_result.change_hash = ComputeChangesHash(out_result.resolved_changes);

    return true;
}

} // namespace axis::time::internal

// =============================================================================
// Debug & Statistics API
// =============================================================================

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_GetStats(
    const AxisTimeAxis* axis,
    AxisTimeAxisStats* out_stats
) {
    if (!axis || !out_stats) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    const auto* state = reinterpret_cast<const axis::time::internal::TimeAxisState*>(axis);

    out_stats->current_slot = state->current_slot.load();
    out_stats->total_requests_processed = state->total_requests_processed.load();
    out_stats->total_conflicts_resolved = state->total_conflicts_resolved.load();

    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state->groups_mutex));
        out_stats->active_conflict_groups = 0;
        for (const auto& g : state->conflict_groups) {
            if (g.active) out_stats->active_conflict_groups++;
        }
    }

    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state->anchors_mutex));
        out_stats->current_anchor_count = static_cast<uint32_t>(state->anchors.size());
        if (!state->anchors.empty()) {
            out_stats->oldest_reconstructible = state->anchors.front().slot_index;
        } else {
            out_stats->oldest_reconstructible = 0;
        }
    }

    // Estimate memory usage
    out_stats->memory_usage_bytes =
        sizeof(axis::time::internal::TimeAxisState) +
        state->pending_requests.capacity() * sizeof(axis::time::internal::PendingRequest) +
        state->conflict_groups.capacity() * sizeof(axis::time::internal::ConflictGroupData) +
        state->anchors.capacity() * sizeof(axis::time::internal::AnchorData);

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API AxisTimeResult AxisTimeAxis_SetCommitCallback(
    AxisTimeAxis* axis,
    AxisSlotCommitCallback callback,
    void* user_data
) {
    if (!axis) {
        return AXIS_TIME_ERROR_INVALID_PARAMETER;
    }

    auto* state = reinterpret_cast<axis::time::internal::TimeAxisState*>(axis);

    std::lock_guard<std::mutex> lock(state->callback_mutex);
    state->commit_callback = callback;
    state->callback_user_data = user_data;

    return AXIS_TIME_OK;
}

extern "C" AXIS_TIME_API size_t AxisTimeAxis_GetPendingRequestCount(
    const AxisTimeAxis* axis,
    AxisSlotIndex slot_index
) {
    if (!axis) {
        return 0;
    }

    const auto* state = reinterpret_cast<const axis::time::internal::TimeAxisState*>(axis);

    if (slot_index <= state->current_slot.load()) {
        return 0;  // Past slots have no pending requests
    }

    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state->requests_mutex));

    size_t count = 0;
    for (const auto& req : state->pending_requests) {
        if (!req.cancelled && req.desc.target_slot == slot_index) {
            count++;
        }
    }

    return count;
}
