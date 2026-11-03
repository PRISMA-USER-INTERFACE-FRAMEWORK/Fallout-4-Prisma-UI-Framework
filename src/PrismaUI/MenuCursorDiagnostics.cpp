#include "MenuCursorDiagnostics.h"

#include <Windows.h>

#include <array>
#include <atomic>

#include "Engine/EngineMenuCursor.h"
#include "Engine/EngineMenuCursorInputProbe.h"
#include "Utils/ModulePath.h"

namespace PrismaUI::MenuCursorDiagnostics {

namespace {

std::atomic<std::uint64_t> g_focusGeneration{0};
std::atomic<std::uint64_t> g_pendingPresentGeneration{0};
std::atomic<std::uint64_t> g_pendingView{0};
std::atomic<bool> g_presentSampled{true};
std::atomic<bool> g_inputProbeInstallQueued{false};
std::array<std::atomic<std::uint64_t>, 4> g_lastInputLogMs{};

bool Enabled()
{
    static const bool enabled = [] {
        const auto ini = Utils::PluginIniPath().string();
        const bool on = GetPrivateProfileIntA("Diagnostics", "bMap76Cursor", 0, ini.c_str()) != 0;
        if (on) {
            logger::info("[MAP76-CURSOR] diagnostics enabled by [Diagnostics] bMap76Cursor=1");
        }
        return on;
    }();
    return enabled;
}

void ResetInputLogSlots() noexcept
{
    for (auto& slot : g_lastInputLogMs) slot.store(0, std::memory_order_release);
}

[[nodiscard]] constexpr std::size_t InputLogSlotIndex(Engine::CursorInputChain chain,
                                                       Engine::CursorInputKind kind) noexcept
{
    return static_cast<std::size_t>(chain) * 2u + static_cast<std::size_t>(kind);
}

bool ClaimInputLogSlot(Engine::CursorInputChain chain, Engine::CursorInputKind kind) noexcept
{

    constexpr std::uint64_t kIntervalMs = 50;
    const std::uint64_t now = GetTickCount64();
    auto& last = g_lastInputLogMs[InputLogSlotIndex(chain, kind)];
    std::uint64_t observed = last.load(std::memory_order_relaxed);
    while (now - observed >= kIntervalMs) {
        if (last.compare_exchange_weak(observed, now, std::memory_order_acq_rel, std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void DumpSnapshot(const char* stage, std::uint64_t view, std::uint64_t generation,
                  const Engine::MenuCursorSnapshot& cursor)
{
    if (!cursor.valid) {
        logger::warn("[MAP76-CURSOR] stage={} view={} generation={} MenuCursor=null", stage, view, generation);
        return;
    }

    logger::info(
        "[MAP76-CURSOR] stage={} view={} generation={} pos=({}, {}) min=({}, {}) max=({}, {}) "
        "constraints=({:.6f},{:.6f},{:.6f},{:.6f}) registered={} forceOSCursorPos={} "
        "allowGamepadCursorOverride={}",
        stage,
        view,
        generation,
        cursor.cursorPosX,
        cursor.cursorPosY,
        cursor.minCursorX,
        cursor.minCursorY,
        cursor.maxCursorX,
        cursor.maxCursorY,
        cursor.leftConstraintPct,
        cursor.rightConstraintPct,
        cursor.topConstraintPct,
        cursor.bottomConstraintPct,
        cursor.registeredCursors,
        cursor.forceOSCursorPos,
        cursor.allowGamepadCursorOverride);
}

void Dump(const char* stage, std::uint64_t view, std::uint64_t generation)
{
    DumpSnapshot(stage, view, generation, Engine::SnapshotMenuCursor());
}

void OnEngineCursorInput(const Engine::CursorInputEvent& input) noexcept
{
    if (!Enabled()) return;

    const std::uint64_t view = g_pendingView.load(std::memory_order_acquire);
    const std::uint64_t generation = g_pendingPresentGeneration.load(std::memory_order_acquire);
    if (!view || !generation) return;

    if (!ClaimInputLogSlot(input.chain, input.kind)) return;

    const auto sample = Engine::CaptureCursorInputSample(input);

    if (g_pendingPresentGeneration.load(std::memory_order_acquire) != generation ||
        g_pendingView.load(std::memory_order_acquire) != view) {
        return;
    }

    const char* chain = input.chain == Engine::CursorInputChain::Menu ? "menu" : "gameplay";
    const char* kind = input.kind == Engine::CursorInputKind::MouseDelta ? "mouse-delta" : "cursor-position";
    const auto& cursor = sample.cursor;
    if (!cursor.valid) {
        logger::warn(
            "[MAP76-CURSOR] stage=engine-input chain={} kind={} view={} generation={} input=({}, {}) "
            "pipboyPresent={} pipboyOnStack={} pipboyCursorEnabled={} MenuCursor=null",
            chain,
            kind,
            view,
            generation,
            input.x,
            input.y,
            sample.pipboyMenuPresent,
            sample.pipboyMenuOnStack,
            sample.pipboyCursorEnabled);
        return;
    }

    logger::info(
        "[MAP76-CURSOR] stage=engine-input chain={} kind={} view={} generation={} input=({}, {}) "
        "pos=({}, {}) min=({}, {}) max=({}, {}) constraints=({:.6f},{:.6f},{:.6f},{:.6f}) "
        "registered={} forceOSCursorPos={} allowGamepadCursorOverride={} "
        "pipboyPresent={} pipboyOnStack={} pipboyCursorEnabled={}",
        chain,
        kind,
        view,
        generation,
        input.x,
        input.y,
        cursor.cursorPosX,
        cursor.cursorPosY,
        cursor.minCursorX,
        cursor.minCursorY,
        cursor.maxCursorX,
        cursor.maxCursorY,
        cursor.leftConstraintPct,
        cursor.rightConstraintPct,
        cursor.topConstraintPct,
        cursor.bottomConstraintPct,
        cursor.registeredCursors,
        cursor.forceOSCursorPos,
        cursor.allowGamepadCursorOverride,
        sample.pipboyMenuPresent,
        sample.pipboyMenuOnStack,
        sample.pipboyCursorEnabled);
}

void QueueInputProbeInstall()
{
    if (g_inputProbeInstallQueued.exchange(true, std::memory_order_acq_rel)) return;
    if (auto* tasks = F4SE::GetTaskInterface()) {
        tasks->AddTask([] {
            if (Engine::InstallMenuCursorInputProbe(&OnEngineCursorInput)) {
                logger::info("[MAP76-CURSOR] engine input probe installed on MenuControls/PlayerControls");
            } else {
                logger::warn("[MAP76-CURSOR] engine input probe could not find MenuControls or PlayerControls");
                g_inputProbeInstallQueued.store(false, std::memory_order_release);
            }
        });
    } else {
        g_inputProbeInstallQueued.store(false, std::memory_order_release);
    }
}

}

void OnFocusedCaptureArmed(std::uint64_t view)
{
    if (!view || !Enabled()) return;

    const std::uint64_t generation = g_focusGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    g_pendingView.store(view, std::memory_order_release);
    g_pendingPresentGeneration.store(generation, std::memory_order_release);
    g_presentSampled.store(false, std::memory_order_release);
    ResetInputLogSlots();

    QueueInputProbeInstall();

    logger::info("[MAP76-CURSOR] stage=focus-policy view={} generation={}", view, generation);
    Dump("focus-pre-constraint-policy", view, generation);

    if (auto* tasks = F4SE::GetTaskInterface()) {
        tasks->AddTask([view, generation] {
            if (g_focusGeneration.load(std::memory_order_acquire) != generation ||
                g_pendingPresentGeneration.load(std::memory_order_acquire) != generation ||
                g_pendingView.load(std::memory_order_acquire) != view) {
                return;
            }
            Dump("focus-post-focus-task", view, generation);
        });
    }
}

void OnFocusedCaptureReleased()
{
    if (!Enabled()) return;

    const std::uint64_t releasedView = g_pendingView.exchange(0, std::memory_order_acq_rel);
    const std::uint64_t releasedGeneration = g_pendingPresentGeneration.exchange(0, std::memory_order_acq_rel);
    g_presentSampled.store(true, std::memory_order_release);
    ResetInputLogSlots();

    g_focusGeneration.fetch_add(1, std::memory_order_acq_rel);

    if (releasedView || releasedGeneration) {
        logger::info("[MAP76-CURSOR] stage=focus-released view={} generation={}", releasedView,
                     releasedGeneration);
    }
}

void OnPresentCursorSample()
{
    if (!Enabled() || g_presentSampled.load(std::memory_order_acquire)) return;

    const std::uint64_t generation = g_pendingPresentGeneration.load(std::memory_order_acquire);
    const std::uint64_t view = g_pendingView.load(std::memory_order_acquire);
    if (!generation || !view) return;

    bool expected = false;
    if (!g_presentSampled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

    if (g_pendingPresentGeneration.load(std::memory_order_acquire) != generation ||
        g_pendingView.load(std::memory_order_acquire) != view) {
        return;
    }
    Dump("focus-next-present", view, generation);
}

}
