#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parent.parent
sandbox = (root / "src/PrismaUI/NetworkSandbox.h").read_text(encoding="utf-8")
file_system = (root / "src/PrismaUI/PrismaFileSystem.cpp").read_text(encoding="utf-8")
callbacks = (root / "src/PrismaUI/UltralightDocumentCallbacks.cpp").read_text(encoding="utf-8")
views = (root / "src/PrismaUI/UltralightBackendViews.cpp").read_text(encoding="utf-8")
manager = (root / "src/PrismaUI/UltralightViewManager.cpp").read_text(encoding="utf-8")
dispatch_source = (root / "src/PrismaUI/UltralightBackendDispatch.cpp").read_text(encoding="utf-8")
runtime_state = (root / "src/PrismaUI/WebRuntimeState.cpp").read_text(encoding="utf-8")
runtime_lifecycle = (root / "src/PrismaUI/WebRuntimeInspector.cpp").read_text(encoding="utf-8")
runtime_views = (root / "src/PrismaUI/WebRuntimeViews.cpp").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


inspector_csp = function_body(sandbox, "inline std::string InjectInspectorContentSecurityPolicyIntoHtml")
for token in (
    "connect-src 'none'",
    "worker-src 'self' blob:",
    "frame-src 'self' data: blob:",
    "img-src 'self' data: blob:",
    "style-src 'self' 'unsafe-inline'",
    "script-src 'self' 'unsafe-inline' data:",
    "object-src 'none'",
    "base-uri 'none'",
    "form-action 'none'",
):
    assert token in inspector_csp, f"inspector CSP lost required boundary: {token}"
assert 'constexpr std::string_view head = "<head>";' in inspector_csp
assert "result.insert(position + head.size(), meta);" in inspector_csp

open_file = function_body(file_system, "ultralight::RefPtr<ultralight::Buffer> PrismaFileSystem::OpenFile")
assert "const auto logicalPath = UrlPath(request);" in open_file
assert 'logicalPath.rfind("inspector/", 0) == 0' in open_file
assert "InjectInspectorContentSecurityPolicyIntoHtml(payload)" in open_file
assert "InjectContentSecurityPolicyIntoHtml(payload)" in open_file

window_ready = function_body(callbacks, "void UltralightDocumentCallbacks::OnWindowObjectReady(")
assert "record->isInspector && record->view" in window_ready
assert "InstallInspectorHost(*record->view, id);" in window_ready
assert window_ready.index("InstallInspectorHost(*record->view, id);") < window_ready.index("InstallListeners(*record);")

is_valid = function_body(views, "bool UltralightBackend::IsViewValid(")
assert "viewManager_.IsValid(id)" in is_valid
assert "ViewReservationState::DestroyRequested" in manager
assert "ViewReservationState::Failed" in manager

dispatch = function_body(dispatch_source, "void UltralightBackend::DispatchForView(")
external = dispatch[:dispatch.index("const auto state = viewManager_.DispatchState(id);")]
assert "viewManager_.DispatchState(id)" in external
assert "ViewDispatchState::Rejected" in external
assert "ViewDispatchState::Unknown" not in external, (
    "missing public reservations must remain admissible until the owner thread resolves internal ids"
)
assert external.index("viewManager_.DispatchState(id)") < external.index("Dispatch(std::move(function));")
assert "ViewDispatchState::Unknown" in dispatch

view_error = function_body(runtime_state, "void ViewErrorSink(")
assert "view == st.backend->VisibleInspectorView()" in view_error
assert "const auto owner = activeOwner ? activeOwner : pendingOwner;" in view_error
assert "owner && st.backend->IsInspectorVisible(owner)" in view_error, (
    "a stale inspector failure must not close a replacement owner that is only pending and not backend-visible"
)
assert view_error.index("IsInspectorVisible(owner)") < view_error.index("SetInspectorVisibility(owner, false)")

apply_state = function_body(runtime_lifecycle, "void ApplyInspectorBackendStateChanged(")
acquire = apply_state.index("InputMask::Acquire(InputMask::Owner::kInspector)")
assert "GetViewHealth(" not in apply_state[:acquire], (
    "internal inspector health is not initialized at creation, so missing health must not be treated as LoadFailed before capture"
)
stale_hidden = apply_state[apply_state.index("if (!fallback &&"):apply_state.index("if (fallback &&")]
assert "st.localInspectorOwner.load(std::memory_order_acquire) == owner" in stale_hidden
assert "st.localInspectorPending.load(std::memory_order_acquire) == owner" in stale_hidden
assert "st.backend && st.backend->IsReady() && IsValid(owner)" in stale_hidden, (
    "an older hidden callback may be ignored for immediate reopen only while backend and owner remain healthy"
)

set_visibility = function_body(runtime_lifecycle, "void SetInspectorVisibility(ViewId view, bool visible)")
visible_branch = set_visibility[:set_visibility.index("const ViewId owner = st.localInspectorOwner", set_visibility.index("if (visible)"))]
assert "if (!IsValid(view)) return;" in visible_branch
reserve = set_visibility.index("st.localInspectorPending.compare_exchange_strong")
dispatch_show = set_visibility.index("st.backend->SetInspectorVisibility(view, true);")
post_dispatch = set_visibility.index("if (!st.backend->IsReady() || !IsValid(view)) OnInspectorBackendStateChanged(view, false);")
assert set_visibility.index("if (!IsValid(view)) return;") < reserve < dispatch_show < post_dispatch, (
    "inspector open must reject dead owners before pending reservation and clear pending if owner/backend loss races the show"
)

retry = function_body(runtime_lifecycle, "void RetryInspectorTeardown()")
assert "const ViewId pendingOwner = st.localInspectorPending.load" in retry
assert "!activeOwner && pendingOwner" in retry
assert "!st.backend || !st.backend->IsReady() || !IsValid(pendingOwner)" in retry
pending_recovery = retry.index("OnInspectorBackendStateChanged(pendingOwner, false);")
active_health = retry.index("activeOwner && (!st.backend || !st.backend->IsReady() || !IsValid(activeOwner))")
active_recovery = retry.index("OnInspectorBackendStateChanged(activeOwner, false);")
present_timing = retry.index("const int64_t now = HealthNowMs();")
assert pending_recovery < active_health < active_recovery < present_timing, (
    "backend/owner health loss must release active inspector ownership before the Present-silence recovery path"
)

background = function_body(runtime_views, "void SetViewOffscreenBackground(")
assert "!view || !st.backend || !IsValid(view)" in background
assert background.index("IsValid(view)") < background.index("SetViewClearColor(view, argb)")

print("inspector document and backend admission guard passed")
