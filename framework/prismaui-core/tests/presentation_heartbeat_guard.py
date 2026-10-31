#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "PrismaUI"

runtime = (SRC / "WebRuntimeLifecycle.cpp").read_text(encoding="utf-8")
state = (SRC / "WebRuntimeLifecycle.cpp").read_text(encoding="utf-8")
lifecycle = (SRC / "WebRuntimeLifecycle.cpp").read_text(encoding="utf-8")
present = (SRC / "WebRuntimePresent.cpp").read_text(encoding="utf-8")
ci = (ROOT / "tests" / "ci_windows_gate.py").read_text(encoding="utf-8")

assert '#include "PresentationHeartbeatPolicy.h"' in runtime, (
    "WebRuntime must include the pure presentation-heartbeat policy"
)
assert "st.lastCompositeMs" in state, "fresh-composite diagnostic timestamp disappeared"
assert "st.lastSuccessfulPresentationMs" in state, "successful-presentation heartbeat state is missing"

success = present.index("if (blitResult == CompositeResult::Success)")
safe_skip = present.index("else if (blitResult == CompositeResult::SafeSkip)", success)
success_block = present[success:safe_skip]
assert "std::find(hp.committedViews.begin(), hp.committedViews.end(), static_cast<uint64_t>(focused))" in success_block, (
    "only a successful final blit containing the focused view may refresh its heartbeat"
)
assert "st.lastSuccessfulPresentationMs.store(HealthNowMs()" in success_block, (
    "successful-presentation heartbeat must use final blit completion time"
)
assert "st.lastSuccessfulPresentationMs.store(presentNowMs" not in success_block, (
    "the heartbeat must not use the pre-composition presentation timestamp"
)

watchdog_start = lifecycle.index("void MaybeRecoverDeadFocus()")
health_start = lifecycle.index("void LogPrismaHealthPeriodic()", watchdog_start)
watchdog = lifecycle[watchdog_start:health_start]
assert "PresentationHeartbeatPolicy::ShouldEmergencyReleaseFocus" in watchdog, (
    "focused kLive emergency release must use the presentation-heartbeat policy"
)
assert "st.lastSuccessfulPresentationMs.load" in watchdog, (
    "watchdog must observe final successful presentation, not only fresh composition"
)
assert "(std::max)(st.focusEpisodeStartMs.load(), st.lastCompositeMs.load())" not in watchdog, (
    "watchdog regressed to fresh-composite age; static pages would be falsely unfocused again"
)
assert "last fresh composite" in watchdog, (
    "fresh-composite age should remain diagnostic even though it no longer owns the release decision"
)

assert "presentationAge={}ms" in lifecycle and "compositeAge={}ms" in lifecycle, (
    "health logging must expose fresh composition and final presentation as separate boundaries"
)
assert "presentation_heartbeat_policy_test.cpp" in ci, (
    "the deterministic presentation-heartbeat policy test must run in the Linux framework gate"
)

print("presentation heartbeat wiring guard passed")
