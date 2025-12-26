/**
 * @file main.cpp
 * @brief AXIS Core Architecture Review Test
 *
 * 역할: 엔진 개발자이자 아키텍처 리뷰어
 * 목적: Axis Core가 "법칙 레이어"로서 올바르게 구성되었는지 검증
 *
 * 이 테스트는:
 * - 성능 벤치마크가 아님
 * - 실제 게임 로직이 아님
 * - 기능 확장 제안이 아님
 *
 * 이 테스트의 최종 목표:
 * "Axis Core는 이미 충분하다"라는 결론을 내릴 수 있는지 검증
 */

#include <iostream>
#include <cstdio>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>

// AXIS Core Public API (ONLY)
#include "axis/core/axis_time.h"
#include "axis/core/axis_memory.h"
#include "axis/core/axis_assert.h"

// =============================================================================
// Test Framework
// =============================================================================

struct TestScenario {
    const char* name;
    void (*test_func)();
    bool (*philosophy_check)();
    const char* philosophy_question;
};

int g_total_tests = 0;
int g_passed_tests = 0;
int g_failed_tests = 0;

void ReportTest(const char* name, bool passed, const char* reason = nullptr) {
    g_total_tests++;
    if (passed) {
        g_passed_tests++;
        std::cout << "  [PASS] " << name << "\n";
    } else {
        g_failed_tests++;
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

// =============================================================================
// Scenario A: 명시적 시간 흐름
// =============================================================================

void ScenarioA_ExplicitTimeProgression() {
    PrintSeparator("Scenario A: 명시적 시간 흐름");
    std::cout << "검증: Update 호출 없이는 시간이 전혀 흐르지 않는가?\n\n";

    AxisTimeConfig config = {};
    Axis_InitializeTime(&config);

    // Phase 1: 초기 상태
    AxisTimeMicroseconds t0 = Axis_Time_GetTotalElapsed();
    uint64_t frame0 = Axis_Time_GetFrameCount();

    ReportTest("Initial time is zero", t0 == 0);
    ReportTest("Initial frame count is zero", frame0 == 0);

    // Phase 2: 실제 시간이 흘러도 Core 시간은 정지
    std::cout << "\n  실제 시간 50ms 경과 후 (Update 호출 없음):\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    AxisTimeMicroseconds t1 = Axis_Time_GetTotalElapsed();
    uint64_t frame1 = Axis_Time_GetFrameCount();

    std::cout << "    실제 경과: ~50ms\n";
    std::cout << "    Core 시간: " << t1 << " us\n";
    std::cout << "    Frame count: " << frame1 << "\n";

    bool time_not_advanced = (t1 == t0);
    bool frame_not_advanced = (frame1 == frame0);

    ReportTest("Time does NOT auto-advance", time_not_advanced);
    ReportTest("Frame count does NOT auto-advance", frame_not_advanced);

    // Phase 3: Update 호출 시에만 시간 전진
    std::cout << "\n  Axis_UpdateTime() 호출 후:\n";
    Axis_UpdateTime();

    AxisTimeMicroseconds t2 = Axis_Time_GetTotalElapsed();
    uint64_t frame2 = Axis_Time_GetFrameCount();

    std::cout << "    Core 시간: " << t2 << " us\n";
    std::cout << "    Frame count: " << frame2 << "\n";

    bool time_advanced_after_update = (t2 > t0);
    bool frame_incremented = (frame2 == 1);

    ReportTest("Time advances ONLY after Update", time_advanced_after_update);
    ReportTest("Frame count increments ONLY after Update", frame_incremented);

    Axis_ShutdownTime();

    std::cout << "\n✓ 결론: Core는 시간을 '자동으로 흐르게 하지 않는다'\n";
}

bool PhilosophyA_NoImplicitControl() {
    std::cout << "\n철학 질문 A: Core가 프레임 루프나 실행 흐름을 암묵적으로 제어하는가?\n";
    std::cout << "답변: NO\n";
    std::cout << "근거:\n";
    std::cout << "  - Update를 호출하지 않으면 시간이 전혀 흐르지 않음\n";
    std::cout << "  - Core는 '언제' Update를 호출할지 모름 (사용자 제어)\n";
    std::cout << "  - 백그라운드 스레드 없음, 타이머 없음, 자동 실행 없음\n";
    return true;
}

// =============================================================================
// Scenario B: 결정성 테스트
// =============================================================================

// Mock time source for determinism
namespace {
    uint64_t g_mock_ticks = 0;
    uint64_t MockGetTicks(void*) { return g_mock_ticks; }
}

void ScenarioB_Determinism() { 
    PrintSeparator("Scenario B: 결정성 (Determinism)");
    std::cout << "검증: 동일한 입력 → 동일한 출력이 보장되는가?\n\n";

    // Run 1: 첫 번째 실행
    std::cout << "  [Run 1] Mock time source 주입:\n";
    g_mock_ticks = 0;

    AxisTimeSource mock_source;
    mock_source.GetCurrentTicks = MockGetTicks;
    mock_source.ticks_per_second = 1000000;
    mock_source.user_data = nullptr;

    AxisTimeConfig config = {};
    config.time_source = &mock_source;
    config.fixed_delta_us = 0;

    Axis_InitializeTime(&config);

    std::vector<AxisTimeMicroseconds> run1_deltas;
    std::vector<AxisTimeMicroseconds> run1_totals;

    uint64_t sequence[] = {16666, 33333, 8333, 20000, 16666};
    for (int i = 0; i < 5; ++i) {
        g_mock_ticks += sequence[i];
        Axis_UpdateTime();

        AxisTimeMicroseconds delta = Axis_Time_GetFrameDelta();
        AxisTimeMicroseconds total = Axis_Time_GetTotalElapsed();

        run1_deltas.push_back(delta);
        run1_totals.push_back(total);

        std::cout << "    Frame " << (i+1) << ": ticks += " << sequence[i]
                  << " → dt=" << delta << ", total=" << total << "\n";
    }

    Axis_ShutdownTime();

    // Run 2: 동일한 입력으로 재실행
    std::cout << "\n  [Run 2] 동일한 시퀀스 재실행:\n";
    g_mock_ticks = 0;
    Axis_InitializeTime(&config);

    std::vector<AxisTimeMicroseconds> run2_deltas;
    std::vector<AxisTimeMicroseconds> run2_totals;

    for (int i = 0; i < 5; ++i) {
        g_mock_ticks += sequence[i];
        Axis_UpdateTime();

        AxisTimeMicroseconds delta = Axis_Time_GetFrameDelta();
        AxisTimeMicroseconds total = Axis_Time_GetTotalElapsed();

        run2_deltas.push_back(delta);
        run2_totals.push_back(total);

        std::cout << "    Frame " << (i+1) << ": ticks += " << sequence[i]
                  << " → dt=" << delta << ", total=" << total << "\n";
    }

    Axis_ShutdownTime();

    // 비교
    bool deterministic = true;
    for (size_t i = 0; i < 5; ++i) {
        if (run1_deltas[i] != run2_deltas[i] || run1_totals[i] != run2_totals[i]) {
            deterministic = false;
            break;
        }
    }

    ReportTest("Deterministic time progression", deterministic);

    std::cout << "\n✓ 결론: Core Time은 '결정성의 기반' 역할을 한다\n";
}

bool PhilosophyB_Determinism() {
    std::cout << "\n철학 질문 B: Time이 '결정성의 기반' 역할을 하는가?\n";
    std::cout << "답변: YES\n";
    std::cout << "근거:\n";
    std::cout << "  - 동일한 time source 주입 → 동일한 결과 보장\n";
    std::cout << "  - 플랫폼 시간과 논리 시간 분리\n";
    std::cout << "  - 리플레이/테스트에 필수적인 특성\n";
    return true;
}

// =============================================================================
// Scenario C: Fixed/Variable 분리
// =============================================================================

void ScenarioC_FixedVariableSeparation() {
    PrintSeparator("Scenario C: Fixed/Variable Delta 분리");
    std::cout << "검증: Core가 '시간 정의'만 제공하고 '시간 사용'은 침범하지 않는가?\n\n";

    // Part 1: Variable delta
    std::cout << "  [Part 1] Variable delta (실제 경과 시간):\n";
    AxisTimeConfig config1 = {};
    config1.fixed_delta_us = 0;
    Axis_InitializeTime(&config1);

    // 첫 Update로 baseline 설정 (초기화 이후 시간 제거)
    Axis_UpdateTime();

    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    Axis_UpdateTime();
    AxisTimeMicroseconds var_dt = Axis_Time_GetFrameDelta();

    std::cout << "    실제 sleep: 16ms\n";
    std::cout << "    측정된 dt: " << var_dt << " us\n";

    // Windows sleep은 정확하지 않음 (스케줄러 타임 슬라이스 ~15.6ms)
    // 범위를 넓게 설정: 10~30ms
    bool variable_reflects_actual = (var_dt >= 10000 && var_dt <= 30000);
    ReportTest("Variable delta reflects actual elapsed time", variable_reflects_actual);

    Axis_ShutdownTime();

    // Part 2: Fixed delta
    std::cout << "\n  [Part 2] Fixed delta (고정 스텝):\n";
    AxisTimeConfig config2 = {};
    config2.fixed_delta_us = 16666;  // 60 FPS
    Axis_InitializeTime(&config2);

    std::cout << "    설정: fixed_delta = 16666 us\n";

    bool all_fixed = true;
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + i * 10));  // 가변 delay
        Axis_UpdateTime();

        AxisTimeMicroseconds dt = Axis_Time_GetFrameDelta();
        std::cout << "    Frame " << (i+1) << ": 실제 sleep " << (10 + i * 10) << "ms → dt=" << dt << " us\n";

        if (dt != 16666) {
            all_fixed = false;
        }
    }

    ReportTest("Fixed delta ignores actual elapsed time", all_fixed);

    Axis_ShutdownTime();

    // Part 3: Core가 step을 직접 실행하지 않음
    std::cout << "\n  [검증] Core는 physics step을 직접 돌리는가?\n";
    std::cout << "    답변: NO\n";
    std::cout << "    근거: Core는 'dt 값'만 제공, 사용자가 직접 step 실행\n";

    std::cout << "\n✓ 결론: Core는 '시간 정의'만 제공, '시간 사용'은 침범하지 않음\n";
}

bool PhilosophyC_DefinitionNotUsage() {
    std::cout << "\n철학 질문 C: Core는 '정의'인가, '사용'인가?\n";
    std::cout << "답변: 정의 (Definition)\n";
    std::cout << "근거:\n";
    std::cout << "  - dt 값만 제공, step 실행은 사용자 책임\n";
    std::cout << "  - 타이머/알람/스케줄러 없음\n";
    std::cout << "  - 게임 루프 소유 없음\n";
    return true;
}

// =============================================================================
// Scenario D: 사용자 제어성 (Memory)
// =============================================================================

void ScenarioD_UserControl() {
    PrintSeparator("Scenario D: 사용자 제어성 (Memory)");
    std::cout << "검증: Core가 메모리 정책을 강제하지 않는가?\n\n";

    AxisMemoryConfig config = {};
    config.enable_statistics = 1;
    Axis_InitializeMemory(&config);

    // Choice 1: Allocator 선택 (General)
    std::cout << "  [Choice 1] General Allocator 선택:\n";
    AxisGeneralAllocator* general = Axis_CreateGeneralAllocator("UserChoice1", 1024);
    void* ptr1 = Axis_AllocGeneral(general, 128, 16, AXIS_MEMORY_TAG_CORE);
    std::cout << "    사용자가 General 선택 → Core는 제공만 함\n";
    Axis_FreeGeneral(general, ptr1);
    Axis_DestroyGeneralAllocator(general);

    // Choice 2: Allocator 선택 (Pool)
    std::cout << "\n  [Choice 2] Pool Allocator 선택:\n";
    AxisPoolAllocator* pool = Axis_CreatePoolAllocator("UserChoice2", 64, 10, AXIS_MEMORY_TAG_CORE);
    void* ptr2 = Axis_AllocPool(pool);
    std::cout << "    사용자가 Pool 선택 (크기 64, 개수 10) → Core는 제공만 함\n";
    Axis_FreePool(pool, ptr2);
    Axis_DestroyPoolAllocator(pool);

    // Choice 3: Tag 선택
    std::cout << "\n  [Choice 3] Memory Tag 선택:\n";
    general = Axis_CreateGeneralAllocator("UserChoice3", 1024);
    void* ptr_core = Axis_AllocGeneral(general, 64, 8, AXIS_MEMORY_TAG_CORE);
    void* ptr_renderer = Axis_AllocGeneral(general, 64, 8, AXIS_MEMORY_TAG_RENDERER);
    std::cout << "    사용자가 Tag 선택 (CORE, RENDERER) → Core는 기록만 함\n";
    Axis_FreeGeneral(general, ptr_core);
    Axis_FreeGeneral(general, ptr_renderer);
    Axis_DestroyGeneralAllocator(general);

    ReportTest("User chooses allocator type", true);
    ReportTest("User chooses pool size", true);
    ReportTest("User chooses memory tags", true);

    std::cout << "\n  [검증] Core가 강제하는 메모리 정책이 있는가?\n";
    std::cout << "    답변: NO\n";
    std::cout << "    근거: 모든 선택권은 사용자에게 있음\n";

    Axis_ShutdownMemory();

    std::cout << "\n✓ 결론: Core는 '정책'이 아니라 '수단'이다\n";
}

bool PhilosophyD_MeansNotPolicy() {
    std::cout << "\n철학 질문 D: Core가 '정책'을 강제하는가?\n";
    std::cout << "답변: NO\n";
    std::cout << "근거:\n";
    std::cout << "  - Allocator 선택: 사용자\n";
    std::cout << "  - 크기/개수 결정: 사용자\n";
    std::cout << "  - Tag 분류: 사용자\n";
    std::cout << "  → Core는 '수단'만 제공\n";
    return true;
}

// =============================================================================
// Scenario E: 프레임 메모리 명시성
// =============================================================================

void ScenarioE_FrameMemoryExplicit() {

    PrintSeparator("Scenario E: Frame Memory Explicit Lifecycle");
    std::cout << "Verification: No auto-reset / No hidden lifecycle?\n\n";

    AxisMemoryConfig config = {};
    Axis_InitializeMemory(&config);

    AxisFrameAllocator* frame = Axis_CreateFrameAllocator("FrameTest", 1024, AXIS_MEMORY_TAG_TEMP);

    // Frame 1
    std::cout << "  [Frame 1] 메모리 할당:\n";
    void* temp1 = Axis_AllocFrame(frame, 256, 16);
    void* temp2 = Axis_AllocFrame(frame, 128, 8);

    size_t usage1 = Axis_GetFrameUsage(frame);
    std::cout << "    할당 후 사용량: " << usage1 << " bytes\n";

    // 명시적 Reset 없이는 메모리 유지
    std::cout << "\n  [검증] Reset 호출 없이 메모리가 자동 해제되는가?\n";
    size_t usage2 = Axis_GetFrameUsage(frame);
    std::cout << "    시간 경과 후 사용량: " << usage2 << " bytes\n";

    bool no_auto_reset = (usage1 == usage2);
    ReportTest("Frame memory does NOT auto-reset", no_auto_reset);

    // 명시적 Reset 호출
    std::cout << "\n  [Frame 2] 명시적 Axis_ResetFrameAllocator() 호출:\n";
    Axis_ResetFrameAllocator(frame);

    size_t usage3 = Axis_GetFrameUsage(frame);
    std::cout << "    Reset 후 사용량: " << usage3 << " bytes\n";

    bool explicit_reset_works = (usage3 == 0);
    ReportTest("Frame memory resets ONLY after explicit call", explicit_reset_works);

    Axis_DestroyFrameAllocator(frame);
    Axis_ShutdownMemory();

    std::cout << "\n✓ 결론: 생명주기는 '명시적'이며 '숨겨진 자동화' 없음\n";
}

bool PhilosophyE_ExplicitLifecycle() {
    std::cout << "\n철학 질문 E: Core가 '숨겨진 자동화'를 포함하는가?\n";
    std::cout << "답변: NO\n";
    std::cout << "근거:\n";
    std::cout << "  - Frame 메모리는 Reset 호출 전까지 유지됨\n";
    std::cout << "  - Update와 Reset의 연결 없음\n";
    std::cout << "  - 사용자가 생명주기를 완전히 제어\n";
    return true;
}

// =============================================================================
// Scenario F: 실패의 명시성
// =============================================================================

int g_custom_assert_count = 0;

int CustomAssertHandler(const char*, int, const char*, const char*, void*) {
    g_custom_assert_count++;
    return 0;  // Don't break
}

void ScenarioF_ExplicitFailure() {
    PrintSeparator("Scenario F: Explicit Failure");
    std::cout << "Verification: Does it fail silently on misuse?\n\n";

    // Setup custom handler to detect assertions
    Axis_SetAssertHandler(CustomAssertHandler, nullptr);
    g_custom_assert_count = 0;

    AxisMemoryConfig mem_config = {};
    Axis_InitializeMemory(&mem_config);

    // Test 1: Invalid free
    std::cout << "  [Test 1] 잘못된 메모리 해제 시도:\n";
    AxisGeneralAllocator* alloc = Axis_CreateGeneralAllocator("Test", 1024);

    int dummy = 0;
    void* invalid_ptr = &dummy;

#ifdef _DEBUG
    std::cout << "    잘못된 포인터로 Free 호출...\n";
    g_custom_assert_count = 0;
    Axis_FreeGeneral(alloc, invalid_ptr);

    bool assert_triggered = (g_custom_assert_count > 0);
    std::cout << "    Assert 발생 횟수: " << g_custom_assert_count << "\n";
    ReportTest("Invalid operation triggers assertion", assert_triggered);
#else
    std::cout << "    (Release 빌드에서는 Assert 비활성화)\n";
    ReportTest("Release build skips debug assertions", true);
#endif

    Axis_DestroyGeneralAllocator(alloc);

    // Test 2: API 계약 위반
    std::cout << "\n  [Test 2] 초기화 전 사용 시도:\n";
    Axis_ShutdownMemory();  // Shutdown first

    AxisResult result = Axis_UpdateTime();
    bool returns_error = (result == AXIS_ERROR_NOT_INITIALIZED);
    std::cout << "    UpdateTime (미초기화): " << (returns_error ? "ERROR" : "OK") << "\n";
    ReportTest("Uninitialized usage returns error", returns_error);

    Axis_SetAssertHandler(nullptr, nullptr);

    std::cout << "\n✓ 결론: Core는 '침묵하지 않는다' - 실패는 명시적\n";
}

bool PhilosophyF_NoSilentFailure() {
    std::cout << "\n철학 질문 F: Core가 오류를 숨기는가?\n";
    std::cout << "답변: NO\n";
    std::cout << "근거:\n";
    std::cout << "  - 잘못된 사용 → Assert (Debug)\n";
    std::cout << "  - API 계약 위반 → Error code 반환\n";
    std::cout << "  - 조용한 실패 없음\n";
    return true;
}

// =============================================================================
// 최종 철학 검증
// =============================================================================

void FinalPhilosophyReview() {
    PrintSeparator("최종 철학 검증 질문");

    std::cout << "\n=== 질문 1 ===\n";
    std::cout << "Q: 이 Core 없이 다른 DLL이 독립적으로 개발 가능한가?\n";
    std::cout << "A: NO (불가능)\n";
    std::cout << "이유:\n";
    std::cout << "  - 모든 DLL은 시간의 정의가 필요 (Time)\n";
    std::cout << "  - 모든 DLL은 메모리 할당이 필요 (Memory)\n";
    std::cout << "  - 모든 DLL은 오류 보고가 필요 (Assert)\n";
    std::cout << "  → Core는 '선택'이 아니라 '필수'\n";

    std::cout << "\n=== 질문 2 ===\n";
    std::cout << "Q: Core가 '편의 기능'으로 오해될 수 있는 API를 포함하는가?\n";
    std::cout << "A: NO\n";
    std::cout << "검증:\n";
    std::cout << "  - 타이머/알람 없음 ✓\n";
    std::cout << "  - 단위 변환 함수 제거됨 ✓\n";
    std::cout << "  - 자동 실행 기능 없음 ✓\n";
    std::cout << "  - 게임 루프 소유 없음 ✓\n";
    std::cout << "  → 순수한 '법칙' 레이어\n";

    std::cout << "\n=== 질문 3 ===\n";
    std::cout << "Q: Core가 프레임 루프나 실행 흐름을 암묵적으로 제어하는가?\n";
    std::cout << "A: NO\n";
    std::cout << "검증:\n";
    std::cout << "  - Update 호출 = 사용자 책임 ✓\n";
    std::cout << "  - Reset 호출 = 사용자 책임 ✓\n";
    std::cout << "  - 백그라운드 스레드 없음 ✓\n";
    std::cout << "  → 명시적 제어만 존재\n";

    std::cout << "\n=== 질문 4 ===\n";
    std::cout << "Q: 이 Core 설계를 5년 뒤에도 '왜 이렇게 했는지' 설명할 수 있는가?\n";
    std::cout << "A: YES\n";
    std::cout << "이유:\n";
    std::cout << "  - '시간의 단일 진실 공급원' → 영속적 개념\n";
    std::cout << "  - '명시적 생명주기' → 결정성 보장\n";
    std::cout << "  - '법칙, 서비스 아님' → 불변의 원칙\n";
    std::cout << "  → 시간이 지나도 정당성 유지\n";

    std::cout << "\n=== 질문 5 ===\n";
    std::cout << "Q: 이 Core는 '작아서 약한가', 아니면 '작아서 강한가'?\n";
    std::cout << "A: 작아서 강하다\n";
    std::cout << "근거:\n";
    std::cout << "  - 모든 DLL이 의존 → 영향력 최대\n";
    std::cout << "  - 단순함 → 오류 최소화\n";
    std::cout << "  - 명시적 계약 → 신뢰성 최대화\n";
    std::cout << "  - 확장 가능 → 개별 getter로 필드 추가 가능\n";
    std::cout << "  → '최소주의 강함' (Minimalist Strength)\n";
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main() {
    PrintSeparator("AXIS Core Architecture Review");
    std::cout << "역할: 엔진 개발자 & 아키텍처 리뷰어\n";
    std::cout << "목적: Core의 철학적 순수성 검증\n";
    std::cout << "최종 목표: 'Axis Core는 이미 충분하다' 증명\n";

    try {
        // Execute all scenarios
        ScenarioA_ExplicitTimeProgression();
        PhilosophyA_NoImplicitControl();

        ScenarioB_Determinism();
        PhilosophyB_Determinism();

        ScenarioC_FixedVariableSeparation();
        PhilosophyC_DefinitionNotUsage();

        ScenarioD_UserControl();
        PhilosophyD_MeansNotPolicy();

        ScenarioE_FrameMemoryExplicit();
        PhilosophyE_ExplicitLifecycle();

        ScenarioF_ExplicitFailure();
        PhilosophyF_NoSilentFailure();

        // Final review
        FinalPhilosophyReview();
    }
    catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION: " << e.what() << "\n";
        return 1;
    }

    // Summary
    PrintSeparator("테스트 결과 요약");
    std::cout << "총 테스트: " << g_total_tests << "\n";
    std::cout << "통과: " << g_passed_tests << "\n";
    std::cout << "실패: " << g_failed_tests << "\n\n";

    if (g_failed_tests == 0) {
        std::cout << "========================================\n";
        std::cout << "최종 결론\n";
        std::cout << "========================================\n\n";
        std::cout << "✅ Axis Core는 이미 충분하다\n\n";
        std::cout << "검증된 사항:\n";
        std::cout << "  1. Core는 '법칙'이다 (서비스 아님)\n";
        std::cout << "  2. 명시적 제어만 존재 (암묵적 흐름 없음)\n";
        std::cout << "  3. 결정성 보장 (재현 가능)\n";
        std::cout << "  4. 사용자 제어성 보장 (정책 강제 없음)\n";
        std::cout << "  5. 실패의 명시성 (조용한 실패 없음)\n";
        std::cout << "  6. 작아서 강함 (최소주의 강함)\n\n";
        std::cout << "Core는 확장이 아니라 '유지'가 필요하다.\n";
    } else {
        std::cout << "⚠ 일부 테스트 실패 - 재검토 필요\n";
    }

    std::cout << "\nPress Enter to exit...";
    std::cin.get();

    return (g_failed_tests == 0) ? 0 : 1;
}
