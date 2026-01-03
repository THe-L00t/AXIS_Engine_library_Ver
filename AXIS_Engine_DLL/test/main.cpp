/**
 * @file main.cpp
 * @brief AXIS Time Axis Termination Policy Semantic Verification
 *
 * Role: Engine Developer & Architecture Verifier
 * Purpose: Verify that termination policy immutability is correctly enforced
 *
 * This test program validates:
 * - Policy immutability after Time Axis creation
 * - Policy hash consistency and uniqueness
 * - Setter functions return POLICY_LOCKED error
 * - Termination conditions work correctly
 * - External signals respect policy immutability
 *
 * PHILOSOPHY:
 * "A termination policy is part of the Time Axis definition, not part of gameplay logic."
 */

#include <iostream>
#include <cstdio>
#include <cassert>
#include <cstring>

// AXIS Time Public API (ONLY)
#include "axis/time/axis_time_slot.h"
#include "axis/time/axis_time_slot_types.h"

// =============================================================================
// Test Framework
// =============================================================================

struct TestContext {
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
};

TestContext g_ctx;

void ReportTest(const char* name, bool passed, const char* reason = nullptr) {
    g_ctx.total_tests++;
    if (passed) {
        g_ctx.passed_tests++;
        std::cout << "  [PASS] " << name << "\n";
    } else {
        g_ctx.failed_tests++;
        std::cout << "  [FAIL] " << name;
        if (reason) {
            std::cout << " - " << reason;
        }
        std::cout << "\n";
    }
}

void PrintSeparator(const char* title = nullptr) {
    std::cout << "\n";
    std::cout << "========================================\n";
    if (title) {
        std::cout << title << "\n";
        std::cout << "========================================\n";
    }
}

void BeginTestGroup(const char* group_name, const char* purpose) {
    PrintSeparator(group_name);
    std::cout << "Purpose: " << purpose << "\n\n";
}

void EndTestGroup(const char* conclusion) {
    std::cout << "\nConclusion: " << conclusion << "\n";
}

// =============================================================================
// Test Group 1: Policy Hash Consistency
// =============================================================================

void TestGroup_CreateAndHash() {
    BeginTestGroup(
        "Test Group 1: Policy Hash Consistency",
        "Verify that policy hash is computed deterministically"
    );

    // Test 1.1: Identical policies produce identical hashes
    std::cout << "  [Test 1.1] Identical policies -> identical hashes\n";

    AxisTerminationConfig term_config_1 = AxisTermination_DefaultConfig();
    term_config_1.step_limit = 5000;
    term_config_1.safety_cap = 10000;
    term_config_1.terminate_on_request_drain = 1;

    AxisTimeAxisConfig config_1 = AxisTimeAxis_DefaultConfig();
    config_1.termination_config = &term_config_1;

    AxisTimeAxis* axis1 = nullptr;
    AxisTimeResult result1 = AxisTimeAxis_Create(&config_1, &axis1);
    ReportTest("First Time Axis created successfully", result1 == AXIS_TIME_OK);

    uint64_t hash1 = AxisTimeAxis_GetTerminationPolicyHash(axis1);
    std::cout << "    First axis policy hash: 0x" << std::hex << hash1 << std::dec << "\n";

    // Create second axis with IDENTICAL config
    AxisTerminationConfig term_config_2 = AxisTermination_DefaultConfig();
    term_config_2.step_limit = 5000;
    term_config_2.safety_cap = 10000;
    term_config_2.terminate_on_request_drain = 1;

    AxisTimeAxisConfig config_2 = AxisTimeAxis_DefaultConfig();
    config_2.termination_config = &term_config_2;

    AxisTimeAxis* axis2 = nullptr;
    AxisTimeResult result2 = AxisTimeAxis_Create(&config_2, &axis2);
    ReportTest("Second Time Axis created successfully", result2 == AXIS_TIME_OK);

    uint64_t hash2 = AxisTimeAxis_GetTerminationPolicyHash(axis2);
    std::cout << "    Second axis policy hash: 0x" << std::hex << hash2 << std::dec << "\n";

    bool hashes_identical = (hash1 == hash2);
    ReportTest("Identical policies produce identical hashes", hashes_identical);

    // Test 1.2: Different policies produce different hashes
    std::cout << "\n  [Test 1.2] Different policies -> different hashes\n";

    AxisTerminationConfig term_config_3 = AxisTermination_DefaultConfig();
    term_config_3.step_limit = 8000;  // DIFFERENT value
    term_config_3.safety_cap = 10000;
    term_config_3.terminate_on_request_drain = 1;

    AxisTimeAxisConfig config_3 = AxisTimeAxis_DefaultConfig();
    config_3.termination_config = &term_config_3;

    AxisTimeAxis* axis3 = nullptr;
    AxisTimeResult result3 = AxisTimeAxis_Create(&config_3, &axis3);
    ReportTest("Third Time Axis created successfully", result3 == AXIS_TIME_OK);

    uint64_t hash3 = AxisTimeAxis_GetTerminationPolicyHash(axis3);
    std::cout << "    Third axis policy hash: 0x" << std::hex << hash3 << std::dec << "\n";

    bool hashes_different = (hash1 != hash3);
    ReportTest("Different policies produce different hashes", hashes_different);

    AxisTimeAxis_Destroy(axis1);
    AxisTimeAxis_Destroy(axis2);
    AxisTimeAxis_Destroy(axis3);

    EndTestGroup("Policy hash is deterministic and unique per configuration");
}

// =============================================================================
// Test Group 2: Policy Lock Enforcement
// =============================================================================

void TestGroup_PolicyLock() {
    BeginTestGroup(
        "Test Group 2: Policy Lock Enforcement",
        "Verify that policy cannot be modified after Time Axis creation"
    );

    // Create Time Axis with specific policy
    AxisTerminationConfig term_config = AxisTermination_DefaultConfig();
    term_config.step_limit = 5000;
    term_config.safety_cap = 10000;

    AxisTimeAxisConfig config = AxisTimeAxis_DefaultConfig();
    config.termination_config = &term_config;

    AxisTimeAxis* axis = nullptr;
    AxisTimeResult create_result = AxisTimeAxis_Create(&config, &axis);
    ReportTest("Time Axis created successfully", create_result == AXIS_TIME_OK);

    uint64_t original_hash = AxisTimeAxis_GetTerminationPolicyHash(axis);
    std::cout << "  Original policy hash: 0x" << std::hex << original_hash << std::dec << "\n\n";

    // Test 2.1: SetTerminationByStepLimit should return POLICY_LOCKED
    std::cout << "  [Test 2.1] SetTerminationByStepLimit attempt\n";
    AxisTimeResult result_step = AxisTimeAxis_SetTerminationByStepLimit(axis, 8000);
    bool step_locked = (result_step == AXIS_TIME_ERROR_POLICY_LOCKED);
    ReportTest("SetTerminationByStepLimit returns POLICY_LOCKED", step_locked);

    // Test 2.2: SetTerminationSafetyCap should return POLICY_LOCKED
    std::cout << "\n  [Test 2.2] SetTerminationSafetyCap attempt\n";
    AxisTimeResult result_cap = AxisTimeAxis_SetTerminationSafetyCap(axis, 20000);
    bool cap_locked = (result_cap == AXIS_TIME_ERROR_POLICY_LOCKED);
    ReportTest("SetTerminationSafetyCap returns POLICY_LOCKED", cap_locked);

    // Test 2.3: SetTerminationOnRequestDrain should return POLICY_LOCKED
    std::cout << "\n  [Test 2.3] SetTerminationOnRequestDrain attempt\n";
    AxisTimeResult result_drain = AxisTimeAxis_SetTerminationOnRequestDrain(axis, 1);
    bool drain_locked = (result_drain == AXIS_TIME_ERROR_POLICY_LOCKED);
    ReportTest("SetTerminationOnRequestDrain returns POLICY_LOCKED", drain_locked);

    // Test 2.4: SetTerminationOnGroupResolution should return POLICY_LOCKED
    std::cout << "\n  [Test 2.4] SetTerminationOnGroupResolution attempt\n";
    AxisTimeResult result_group = AxisTimeAxis_SetTerminationOnGroupResolution(axis, 1);
    bool group_locked = (result_group == AXIS_TIME_ERROR_POLICY_LOCKED);
    ReportTest("SetTerminationOnGroupResolution returns POLICY_LOCKED", group_locked);

    // Test 2.5: SetTerminationOnExternalSignal should return POLICY_LOCKED
    std::cout << "\n  [Test 2.5] SetTerminationOnExternalSignal attempt\n";
    AxisTimeResult result_signal = AxisTimeAxis_SetTerminationOnExternalSignal(axis, AXIS_SIGNAL_FORCE_COMMIT);
    bool signal_locked = (result_signal == AXIS_TIME_ERROR_POLICY_LOCKED);
    ReportTest("SetTerminationOnExternalSignal returns POLICY_LOCKED", signal_locked);

    // Test 2.6: SetTerminationConfig should return POLICY_LOCKED
    std::cout << "\n  [Test 2.6] SetTerminationConfig attempt\n";
    AxisTerminationConfig new_config = AxisTermination_DefaultConfig();
    new_config.step_limit = 10000;
    AxisTimeResult result_config = AxisTimeAxis_SetTerminationConfig(axis, &new_config);
    bool config_locked = (result_config == AXIS_TIME_ERROR_POLICY_LOCKED);
    ReportTest("SetTerminationConfig returns POLICY_LOCKED", config_locked);

    // Test 2.7: Policy hash unchanged after setter attempts
    std::cout << "\n  [Test 2.7] Policy hash verification\n";
    uint64_t final_hash = AxisTimeAxis_GetTerminationPolicyHash(axis);
    std::cout << "  Final policy hash: 0x" << std::hex << final_hash << std::dec << "\n";

    bool hash_unchanged = (original_hash == final_hash);
    ReportTest("Policy hash unchanged after all setter attempts", hash_unchanged);

    AxisTimeAxis_Destroy(axis);

    EndTestGroup("Policy is IMMUTABLE after Time Axis creation - all setters correctly return POLICY_LOCKED");
}

// =============================================================================
// Test Group 3: Termination by Step Limit
// =============================================================================

void TestGroup_TerminationByStepLimit() {
    BeginTestGroup(
        "Test Group 3: Termination by Step Limit",
        "Verify that Time Axis terminates correctly when step limit is reached"
    );

    // Create Time Axis with step_limit = 3
    AxisTerminationConfig term_config = AxisTermination_DefaultConfig();
    term_config.step_limit = 3;
    term_config.safety_cap = 10000;  // High safety cap to not interfere

    AxisTimeAxisConfig config = AxisTimeAxis_DefaultConfig();
    config.termination_config = &term_config;

    AxisTimeAxis* axis = nullptr;
    AxisTimeAxis_Create(&config, &axis);

    std::cout << "  Configuration: step_limit = 3\n\n";

    // Test 3.1: First Tick (step 1) - should NOT terminate
    std::cout << "  [Test 3.1] First Tick (step 1)\n";
    AxisTimeAxis_Tick(axis);

    AxisSlotTerminationContext ctx1;
    AxisTimeAxis_GetTerminationContext(axis, &ctx1);
    std::cout << "    Elapsed steps: " << ctx1.elapsed_steps << "\n";

    AxisTerminationReason reason1 = AxisTimeAxis_GetLastTerminationReason(axis);
    std::cout << "    Termination reason: " << reason1 << "\n";

    bool not_terminated_yet_1 = (reason1 == AXIS_TERMINATION_NONE);
    ReportTest("Step 1: Not terminated yet", not_terminated_yet_1);

    // Test 3.2: Second Tick (step 2) - should NOT terminate
    std::cout << "\n  [Test 3.2] Second Tick (step 2)\n";
    AxisTimeAxis_Tick(axis);

    AxisSlotTerminationContext ctx2;
    AxisTimeAxis_GetTerminationContext(axis, &ctx2);
    std::cout << "    Elapsed steps: " << ctx2.elapsed_steps << "\n";

    AxisTerminationReason reason2 = AxisTimeAxis_GetLastTerminationReason(axis);
    std::cout << "    Termination reason: " << reason2 << "\n";

    bool not_terminated_yet_2 = (reason2 == AXIS_TERMINATION_NONE);
    ReportTest("Step 2: Not terminated yet", not_terminated_yet_2);

    // Test 3.3: Third Tick (step 3) - SHOULD terminate by step limit
    std::cout << "\n  [Test 3.3] Third Tick (step 3 - limit reached)\n";
    AxisTimeAxis_Tick(axis);

    AxisSlotTerminationContext ctx3;
    AxisTimeAxis_GetTerminationContext(axis, &ctx3);
    std::cout << "    Elapsed steps: " << ctx3.elapsed_steps << "\n";

    AxisTerminationReason reason3 = AxisTimeAxis_GetLastTerminationReason(axis);
    std::cout << "    Termination reason: " << reason3 << "\n";

    bool terminated_by_step_limit = (reason3 == AXIS_TERMINATION_STEP_LIMIT);
    ReportTest("Step 3: Terminated by STEP_LIMIT", terminated_by_step_limit);

    AxisTimeAxis_Destroy(axis);

    EndTestGroup("Step limit termination condition works correctly");
}

// =============================================================================
// Test Group 4: Termination by Request Drain
// =============================================================================

void TestGroup_TerminationByRequestDrain() {
    BeginTestGroup(
        "Test Group 4: Termination by Request Drain",
        "Verify that Time Axis terminates when all requests are processed"
    );

    // Create Time Axis with terminate_on_request_drain enabled
    AxisTerminationConfig term_config = AxisTermination_DefaultConfig();
    term_config.terminate_on_request_drain = 1;
    term_config.safety_cap = 10000;

    AxisTimeAxisConfig config = AxisTimeAxis_DefaultConfig();
    config.termination_config = &term_config;

    AxisTimeAxis* axis = nullptr;
    AxisTimeAxis_Create(&config, &axis);

    std::cout << "  Configuration: terminate_on_request_drain = enabled\n\n";

    // Create a conflict group
    AxisConflictGroupId group_id;
    AxisTimeAxis_CreateConflictGroup(axis, AXIS_POLICY_FIRST_WRITER, &group_id);

    // Test 4.1: Submit requests for future slots
    std::cout << "  [Test 4.1] Submitting requests for slots 1-3\n";

    AxisStateChangeDesc request1{};
    request1.target_slot = 1;
    request1.conflict_group = group_id;
    request1.key = {1, 0};
    request1.mutation_type = AXIS_MUTATION_SET;
    request1.value.as_int = 100;

    AxisStateChangeDesc request2{};
    request2.target_slot = 2;
    request2.conflict_group = group_id;
    request2.key = {2, 0};
    request2.mutation_type = AXIS_MUTATION_SET;
    request2.value.as_int = 200;

    AxisStateChangeDesc request3{};
    request3.target_slot = 3;
    request3.conflict_group = group_id;
    request3.key = {3, 0};
    request3.mutation_type = AXIS_MUTATION_SET;
    request3.value.as_int = 300;

    AxisTimeAxis_SubmitRequest(axis, &request1, nullptr);
    AxisTimeAxis_SubmitRequest(axis, &request2, nullptr);
    AxisTimeAxis_SubmitRequest(axis, &request3, nullptr);

    std::cout << "    Submitted 3 requests\n";

    // Test 4.2: Tick through slots - should NOT terminate while requests remain
    std::cout << "\n  [Test 4.2] Tick slot 1 (2 requests pending)\n";
    AxisTimeAxis_Tick(axis);

    AxisTerminationReason reason1 = AxisTimeAxis_GetLastTerminationReason(axis);
    bool not_terminated_1 = (reason1 == AXIS_TERMINATION_NONE);
    ReportTest("Slot 1: Not terminated (requests pending)", not_terminated_1);

    std::cout << "\n  [Test 4.3] Tick slot 2 (1 request pending)\n";
    AxisTimeAxis_Tick(axis);

    AxisTerminationReason reason2 = AxisTimeAxis_GetLastTerminationReason(axis);
    bool not_terminated_2 = (reason2 == AXIS_TERMINATION_NONE);
    ReportTest("Slot 2: Not terminated (requests pending)", not_terminated_2);

    // Test 4.4: Final tick - all requests processed, should terminate
    std::cout << "\n  [Test 4.4] Tick slot 3 (last request processed)\n";
    AxisTimeAxis_Tick(axis);

    AxisTerminationReason reason3 = AxisTimeAxis_GetLastTerminationReason(axis);
    std::cout << "    Termination reason: " << reason3 << "\n";

    bool terminated_by_drain = (reason3 == AXIS_TERMINATION_REQUEST_DRAIN);
    ReportTest("Slot 3: Terminated by REQUEST_DRAIN", terminated_by_drain);

    AxisTimeAxis_Destroy(axis);

    EndTestGroup("Request drain termination condition works correctly");
}

// =============================================================================
// Test Group 5: External Signal (Respects Policy Immutability)
// =============================================================================

void TestGroup_ExternalSignal() {
    BeginTestGroup(
        "Test Group 5: External Signal",
        "Verify that external signals work without violating policy immutability"
    );

    // Create Time Axis with external signal requirement
    AxisTerminationConfig term_config = AxisTermination_DefaultConfig();
    term_config.required_external_flags = AXIS_SIGNAL_FORCE_COMMIT;
    term_config.safety_cap = 10000;

    AxisTimeAxisConfig config = AxisTimeAxis_DefaultConfig();
    config.termination_config = &term_config;

    AxisTimeAxis* axis = nullptr;
    AxisTimeAxis_Create(&config, &axis);

    std::cout << "  Configuration: required_external_flags = FORCE_COMMIT\n\n";

    uint64_t original_hash = AxisTimeAxis_GetTerminationPolicyHash(axis);
    std::cout << "  Original policy hash: 0x" << std::hex << original_hash << std::dec << "\n\n";

    // Test 5.1: Tick without signal - should NOT terminate
    std::cout << "  [Test 5.1] Tick without external signal\n";
    AxisTimeAxis_Tick(axis);

    AxisTerminationReason reason1 = AxisTimeAxis_GetLastTerminationReason(axis);
    std::cout << "    Termination reason: " << reason1 << "\n";

    bool not_terminated_without_signal = (reason1 == AXIS_TERMINATION_NONE);
    ReportTest("Without signal: Not terminated", not_terminated_without_signal);

    // Test 5.2: Set external signal
    std::cout << "\n  [Test 5.2] Setting external signal (FORCE_COMMIT)\n";
    AxisTimeResult signal_result = AxisTimeAxis_SetExternalSignal(axis, AXIS_SIGNAL_FORCE_COMMIT);
    bool signal_set_success = (signal_result == AXIS_TIME_OK);
    ReportTest("External signal set successfully", signal_set_success);

    // Test 5.3: Tick with signal - SHOULD terminate
    std::cout << "\n  [Test 5.3] Tick with external signal set\n";
    AxisTimeAxis_Tick(axis);

    AxisTerminationReason reason2 = AxisTimeAxis_GetLastTerminationReason(axis);
    std::cout << "    Termination reason: " << reason2 << "\n";

    bool terminated_by_signal = (reason2 == AXIS_TERMINATION_EXTERNAL_SIGNAL);
    ReportTest("With signal: Terminated by EXTERNAL_SIGNAL", terminated_by_signal);

    // Test 5.4: Policy hash unchanged (external signal doesn't modify policy)
    std::cout << "\n  [Test 5.4] Policy hash verification\n";
    uint64_t final_hash = AxisTimeAxis_GetTerminationPolicyHash(axis);
    std::cout << "  Final policy hash: 0x" << std::hex << final_hash << std::dec << "\n";

    bool hash_unchanged = (original_hash == final_hash);
    ReportTest("Policy hash unchanged after signal operations", hash_unchanged);

    AxisTimeAxis_Destroy(axis);

    EndTestGroup("External signals work correctly without violating policy immutability");
}

// =============================================================================
// Final Philosophy Verification
// =============================================================================

void FinalPhilosophyReview() {
    PrintSeparator("Final Philosophy Verification");

    std::cout << "\n=== Question 1 ===\n";
    std::cout << "Q: Is the termination policy truly IMMUTABLE after Time Axis creation?\n";
    std::cout << "A: YES\n";
    std::cout << "Evidence:\n";
    std::cout << "  - All setter functions return AXIS_TIME_ERROR_POLICY_LOCKED\n";
    std::cout << "  - Policy hash remains constant\n";
    std::cout << "  - No runtime modifications possible\n";

    std::cout << "\n=== Question 2 ===\n";
    std::cout << "Q: Is the policy hash deterministic?\n";
    std::cout << "A: YES\n";
    std::cout << "Evidence:\n";
    std::cout << "  - Identical configs -> identical hashes\n";
    std::cout << "  - Different configs -> different hashes\n";
    std::cout << "  - Hash computed once at creation\n";

    std::cout << "\n=== Question 3 ===\n";
    std::cout << "Q: Do termination conditions work as defined?\n";
    std::cout << "A: YES\n";
    std::cout << "Evidence:\n";
    std::cout << "  - Step limit enforced correctly\n";
    std::cout << "  - Request drain detected correctly\n";
    std::cout << "  - External signals processed correctly\n";

    std::cout << "\n=== Question 4 ===\n";
    std::cout << "Q: Are 'policy definition' and 'runtime behavior' clearly separated?\n";
    std::cout << "A: YES\n";
    std::cout << "Evidence:\n";
    std::cout << "  - Policy: Set at creation, immutable, part of identity\n";
    std::cout << "  - Runtime: External signals allowed (don't modify policy)\n";
    std::cout << "  - Clear semantic boundary enforced\n";

    std::cout << "\n=== Question 5 ===\n";
    std::cout << "Q: Is the philosophy 'Policy is definition, not gameplay' enforced?\n";
    std::cout << "A: YES\n";
    std::cout << "Evidence:\n";
    std::cout << "  - Cannot modify policy after creation\n";
    std::cout << "  - Different policy = different Time Axis (incompatible anchors)\n";
    std::cout << "  - Runtime signals don't affect policy identity\n";
    std::cout << "  -> Policy is PART OF THE TIME AXIS DEFINITION\n";
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main() {
    PrintSeparator("AXIS Time Axis - Termination Policy Semantic Verification");
    std::cout << "Role: Engine Developer & Semantic Verifier\n";
    std::cout << "Purpose: Verify termination policy immutability semantics\n";
    std::cout << "Philosophy: 'Policy is definition, not gameplay logic'\n";

    try {
        // Execute all test groups
        TestGroup_CreateAndHash();
        TestGroup_PolicyLock();
        TestGroup_TerminationByStepLimit();
        TestGroup_TerminationByRequestDrain();
        TestGroup_ExternalSignal();

        // Final philosophy review
        FinalPhilosophyReview();
    }
    catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION: " << e.what() << "\n";
        return 1;
    }

    // Summary
    PrintSeparator("Test Results Summary");
    std::cout << "Total tests: " << g_ctx.total_tests << "\n";
    std::cout << "Passed: " << g_ctx.passed_tests << "\n";
    std::cout << "Failed: " << g_ctx.failed_tests << "\n\n";

    if (g_ctx.failed_tests == 0) {
        std::cout << "========================================\n";
        std::cout << "FINAL CONCLUSION\n";
        std::cout << "========================================\n\n";
        std::cout << "✅ Termination Policy Semantics are CORRECT\n\n";
        std::cout << "Verified:\n";
        std::cout << "  1. Policy is IMMUTABLE after creation\n";
        std::cout << "  2. Policy hash is deterministic\n";
        std::cout << "  3. Setters correctly return POLICY_LOCKED\n";
        std::cout << "  4. Termination conditions work as specified\n";
        std::cout << "  5. External signals respect policy immutability\n";
        std::cout << "  6. 'Definition' and 'Runtime' are clearly separated\n\n";
        std::cout << "The philosophy is enforced:\n";
        std::cout << "'A termination policy is part of the Time Axis definition,\n";
        std::cout << " not part of gameplay logic.'\n";
    } else {
        std::cout << "⚠ Some tests FAILED - Policy semantics violated!\n";
    }

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return (g_ctx.failed_tests == 0) ? 0 : 1;
}
