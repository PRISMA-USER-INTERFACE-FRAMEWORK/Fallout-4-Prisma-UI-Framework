from pathlib import Path

source = (Path(__file__).resolve().parent.parent / "src/PrismaUI/WebRuntimeInspector.cpp").read_text(encoding="utf-8")
apply_start = source.index("void ApplyInspectorBackendStateChanged(ViewId owner, bool visible, bool fallback)")
apply_visible_start = source.index("if (visible)", apply_start)
apply_visible_end = source.index("if (!InputMask::Acquire(InputMask::Owner::kInspector))", apply_visible_start)
apply_visible = source[apply_visible_start:apply_visible_end]

assert "const ViewId teardownOwner = st.inspectorTeardownPending.load" in apply_visible
assert "if (teardownOwner ||" in apply_visible
assert "st.inspectorTeardownPending.store(0" not in apply_visible
assert "st.inspectorTeardownRetryAt.store(0" not in apply_visible
conflict_start = apply_visible.index("if (teardownOwner ||")
conflict = apply_visible[conflict_start:]
assert "st.backend->SetInspectorVisibility(owner, false);" in conflict
assert "const ViewId intendedOwner = activeOwner ? activeOwner : pendingOwner;" in conflict
assert "if (!teardownOwner && intendedOwner && intendedOwner != owner)" in conflict
assert "st.backend->SetInspectorVisibility(intendedOwner, true);" in conflict
assert conflict.index("SetInspectorVisibility(owner, false)") < conflict.index("SetInspectorVisibility(intendedOwner, true)")

acquire_failure_start = apply_visible_end
acquire_failure_end = source.index("st.localInspectorOwner.store(owner", acquire_failure_start)
acquire_failure = source[acquire_failure_start:acquire_failure_end]
assert "st.backend->SetInspectorVisibility(owner, false);" in acquire_failure
assert "st.localInspectorPending.compare_exchange_strong" not in acquire_failure
assert "st.localInspectorPending.store" not in acquire_failure
assert "InputMask::Release" not in acquire_failure

reopen_guard_start = source.index("if (!fallback &&", acquire_failure_end)
reopen_guard_end = source.index("if (fallback && st.backend && st.backend->IsInspectorVisible(owner))", reopen_guard_start)
reopen_guard = source[reopen_guard_start:reopen_guard_end]
assert "st.localInspectorOwner.load(std::memory_order_acquire) == owner" in reopen_guard
assert "st.localInspectorPending.load(std::memory_order_acquire) == owner" in reopen_guard
assert "return;" in reopen_guard

fallback_start = reopen_guard_end
fallback_end = source.index("ViewId expectedOwner = owner", fallback_start)
fallback_visible = source[fallback_start:fallback_end]
assert "st.inspectorTeardownPending" not in fallback_visible
backend_retry = fallback_visible.index("st.backend->SetInspectorVisibility(owner, false);")
retry_anchor = fallback_visible.index("st.inspectorTeardownRetryAt.store(HealthNowMs()")
assert backend_retry < retry_anchor

release_start = source.index("(void)InputMask::Release(InputMask::Owner::kInspector);", fallback_end)
release_end = source.index("void RetryInspectorTeardown()", release_start)
release = source[release_start:release_end]
assert release.count("SyncVanillaCursorVisibility();") == 2
focused_capture = release.index("WebInput::SetCaptureActive(true, true);")
focused_sync = release.index("SyncVanillaCursorVisibility();", focused_capture)
assert focused_capture < focused_sync
passive_capture = release.rindex("WebInput::SetCaptureActive(")
passive_sync = release.rindex("SyncVanillaCursorVisibility();")
assert passive_capture < passive_sync

retry_start = source.index("void RetryInspectorTeardown()")
retry_end = source.index("void OnInspectorBackendStateChanged", retry_start)
retry = source[retry_start:retry_end]
for token in (
    "st.localInspectorOwner.load(std::memory_order_acquire)",
    "st.lastPresentMs.load(std::memory_order_acquire)",
    "st.recoveryStartMs.load(std::memory_order_acquire)",
    "now - reference <= 3000",
    "IsIconic(hwnd)",
    "SetInspectorVisibility(activeOwner, false);",
):
    assert token in retry, f"inspector Present-stall recovery lost required boundary: {token}"
assert retry.index("SetInspectorVisibility(activeOwner, false);") < retry.index("const int64_t retryAt"), (
    "Present-stall recovery must start a normal inspector close before entering teardown retry handling"
)
assert "InputMask::Release" not in retry, (
    "Present-stall recovery must not bypass backend-confirmed inspector teardown"
)

callback_start = source.index("void OnInspectorBackendStateChanged(ViewId owner, bool visible)")
callback_end = source.index("void CreateInspectorView(ViewId view)", callback_start)
callback = source[callback_start:callback_end]
assert "activeOwner != owner && pendingOwner != owner && teardownOwner != owner" in callback
assert "st.inspectorTeardownPending.compare_exchange_strong(expectedTeardown, owner" in callback
assert "st.inspectorTeardownPending.store(owner" not in callback
assert "st.inspectorTeardownPending.store(0" not in callback
callback_visible_start = callback.index("if (visible)")
active_duplicate_start = callback.index("if (activeOwner == owner)", callback_visible_start)
callback_conflict = callback[callback_visible_start:active_duplicate_start]
assert "const ViewId intendedOwner = activeOwner ? activeOwner : pendingOwner;" in callback_conflict
assert "if (!teardownOwner && intendedOwner && intendedOwner != owner)" in callback_conflict
assert "st.backend->SetInspectorVisibility(intendedOwner, true);" in callback_conflict
assert callback_conflict.index("SetInspectorVisibility(owner, false)") < callback_conflict.index("SetInspectorVisibility(intendedOwner, true)")

failure_hide = callback.index("if (st.backend) st.backend->SetInspectorVisibility(owner, false);", active_duplicate_start)
active_duplicate = callback[active_duplicate_start:failure_hide]
assert "st.localInspectorPending.compare_exchange_strong(expectedPending, 0" in active_duplicate
assert "return;" in active_duplicate
failure_end = callback.index("} else {", failure_hide)
dispatch_failure = callback[failure_hide:failure_end]
assert "st.backend->SetInspectorVisibility(owner, false);" in dispatch_failure
assert "st.localInspectorPending.compare_exchange_strong" not in dispatch_failure
assert "st.localInspectorPending.store" not in dispatch_failure

visibility_start = source.index("void SetInspectorVisibility(ViewId view, bool visible)")
visibility_end = source.index("bool IsInspectorVisible", visibility_start)
visibility = source[visibility_start:visibility_end]
teardown_gate = visibility.index("const ViewId teardown = st.inspectorTeardownPending.load")
pending_claim = visibility.index("st.localInspectorPending.compare_exchange_strong")
assert teardown_gate < pending_claim
assert "if (teardown)" in visibility

open_backend = visibility.index("st.backend->SetInspectorVisibility(view, true);")
close = visibility[open_backend:]
not_ready = close.index("if (!st.backend->IsReady())")
teardown_claim = close.index("st.inspectorTeardownPending.compare_exchange_strong(expectedTeardown, view")
backend_close = close.index("st.backend->SetInspectorVisibility(view, false);")
assert not_ready < teardown_claim < backend_close
assert "const bool tracked = owner == view || pending == view || teardown == view;" in close
assert "if (tracked) OnInspectorBackendStateChanged(view, false);" in close[:teardown_claim]
assert "OnInspectorBackendStateChanged(view, false);" not in close[teardown_claim:backend_close]
assert "st.inspectorTeardownRetryAt.store(0" in close[teardown_claim:backend_close]

print("inspector callback teardown ownership guard passed")
