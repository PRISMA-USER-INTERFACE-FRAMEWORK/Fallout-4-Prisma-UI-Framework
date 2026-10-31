#!/usr/bin/env python3
"""Pin the public Focus pause/menu semantics at the source boundary."""
from pathlib import Path
import sys


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


def main() -> int:
    source = Path("src/PrismaUI/WebRuntimeFocus.cpp").read_text(encoding="utf-8")
    focus = function_body(
        source,
        "bool Internal::FocusImpl(ViewId view, bool pauseGame, bool disableFocusMenu,",
    )
    assert "GetViewHealth(view)" in focus, "FocusImpl must verify backend health before arming input capture"
    assert "ViewHealth::DomReady" in focus and "ViewHealth::Live" in focus, (
        "FocusImpl must only focus DOM-ready or live views"
    )

    pause = focus.find("PauseHold::Set(pauseGame);")
    menu_gate = focus.find("if (disableFocusMenu)")
    assert pause != -1, "FocusImpl no longer applies pauseGame through PauseHold"
    assert menu_gate != -1, "FocusImpl no longer handles disableFocusMenu explicitly"
    assert pause < menu_gate, (
        "pauseGame must be applied independently of disableFocusMenu; "
        "Focus(view, true, true) is required to pause"
    )

    gate = focus[menu_gate:]
    close = gate.find("FocusMenu::Close();")
    open_menu = gate.find("FocusMenu::Open(pauseGame);")
    assert close != -1, (
        "disableFocusMenu=true must close a FocusMenu inherited from the previous focused view"
    )
    assert open_menu != -1, "disableFocusMenu=false must still open FocusMenu"
    assert close < open_menu, "FocusMenu close/open branches are no longer in the expected order"

    menu_source = Path("src/Menus/FocusMenu/FocusMenu.cpp").read_text(encoding="utf-8")
    frame_source = Path("src/PrismaUI/WebRuntimeFrame.cpp").read_text(encoding="utf-8")
    creator = function_body(menu_source, "RE::IMenu* FocusMenu::Creator")
    process_message = function_body(menu_source, "RE::UI_MESSAGE_RESULTS FocusMenu::ProcessMessage")
    on_removed = function_body(menu_source, "void FocusMenu::OnRemovedFromMenuStack()")
    menu_open = function_body(menu_source, "void FocusMenu::Open(bool pauseGame)")
    menu_close = function_body(menu_source, "void FocusMenu::Close()")
    menu_tick = function_body(menu_source, "void FocusMenu::Tick()")

    assert "g_openRequested.load" in creator, "FocusMenu Creator must reject stale queued kShow requests"
    assert "return nullptr;" in creator, "FocusMenu Creator must be able to reject a stale kShow"
    assert "RE::IMenu::ProcessMessage(a_message)" in process_message, (
        "FocusMenu must preserve CommonLib IMenu message semantics for show/hide, Scaleform, and controller updates"
    )
    assert "kPausesGame" not in menu_source, "FocusMenu must not duplicate PauseHold pause ownership"
    assert "g_pauseNext" not in menu_source, "FocusMenu must not retain a second pause-policy state"

    assert "RE::IMenu::OnRemovedFromMenuStack()" in on_removed, (
        "FocusMenu removal reconciliation must preserve CommonLib IMenu stack bookkeeping"
    )
    assert "g_openRequested.load" in on_removed, (
        "FocusMenu must reconcile a stale hide against the latest desired open state"
    )
    assert "g_reconcileRequested.store(true" in on_removed, (
        "FocusMenu removal must retain a pending reopen when immediate reconciliation is unavailable"
    )
    assert "closingAllMenus" in on_removed, (
        "FocusMenu must not reopen while Fallout is intentionally closing all menus"
    )
    assert "AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow)" in on_removed, (
        "FocusMenu must reopen after an actual stale removal instead of assuming queue ordering"
    )

    register = menu_open.find("RegisterMenu(MENU_NAME.data(), Creator)")
    show = menu_open.find("AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow)")
    assert register != -1, "FocusMenu must register its Creator with Fallout UI before opening"
    assert show != -1, "FocusMenu::Open must still enqueue kShow"
    assert register < show, "FocusMenu registration must happen before the kShow message"
    assert "closingAllMenus" in menu_open and "g_reconcileRequested.store(true" in menu_open, (
        "FocusMenu::Open must defer its show while Fallout is closing all menus"
    )

    task_lookup = menu_open.find("F4SE::GetTaskInterface()")
    null_check = menu_open.find("if (!tasks)")
    open_state = menu_open.find("g_openRequested.store(true")
    generation = menu_open.find("g_requestGeneration.fetch_add")
    assert task_lookup != -1 and null_check != -1, "FocusMenu::Open must guard the F4SE task interface"
    assert open_state != -1, "FocusMenu::Open must publish the latest desired open state"
    assert generation != -1, "FocusMenu::Open must sequence competing requests"
    assert task_lookup < null_check < open_state < generation, (
        "FocusMenu::Open must not publish requested-open state when UI dispatch is unavailable"
    )
    assert "g_requestGeneration.load" in menu_open, "FocusMenu::Open must reject superseded UI tasks"

    close_state = menu_close.find("g_openRequested.store(false")
    close_generation = menu_close.find("g_requestGeneration.fetch_add")
    task_lookup = menu_close.find("F4SE::GetTaskInterface()")
    assert close_state != -1, "FocusMenu::Close must publish closed state synchronously"
    assert close_generation != -1, "FocusMenu::Close must invalidate older queued Open tasks"
    assert close_state < close_generation < task_lookup, (
        "Close must reject stale kShow construction even when the F4SE task interface is unavailable"
    )
    assert "g_requestGeneration.load" in menu_close, "FocusMenu::Close must reject superseded UI tasks"
    assert "g_showPending" not in menu_source, "FocusMenu must not rely on unverified kShow/kHide FIFO ordering"

    assert "g_reconcileRequested.load" in menu_tick, "FocusMenu::Tick must consume deferred reconciliation"
    assert "closingAllMenus" in menu_tick, "FocusMenu::Tick must wait until Fallout finishes closing menus"
    assert "AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow)" in menu_tick, (
        "FocusMenu::Tick must restore a still-requested menu after a global purge"
    )
    assert "FocusMenu::Tick();" in frame_source, (
        "the per-frame runtime path must service deferred FocusMenu reconciliation"
    )

    pause_source = Path("src/Menus/PauseHold/PauseHold.cpp").read_text(encoding="utf-8")
    pause_creator = function_body(pause_source, "RE::IMenu* PauseHold::Creator")
    pause_process = function_body(pause_source, "RE::UI_MESSAGE_RESULTS PauseHold::ProcessMessage")
    pause_removed = function_body(pause_source, "void PauseHold::OnRemovedFromMenuStack()")
    pause_set = function_body(pause_source, "void PauseHold::Set(bool paused)")
    pause_tick = function_body(pause_source, "void PauseHold::Tick()")
    assert "kPausesGame" in pause_source, "PauseHold must remain the single engine pause owner"
    assert "g_policy.TargetPaused()" in pause_creator and "return nullptr;" in pause_creator, (
        "PauseHold Creator must reject stale queued kShow requests when pause is no longer requested"
    )
    assert "RE::IMenu::ProcessMessage(a_message)" in pause_process, (
        "PauseHold must preserve CommonLib IMenu message semantics"
    )
    assert "RE::IMenu::OnRemovedFromMenuStack()" in pause_removed, (
        "PauseHold removal reconciliation must preserve CommonLib IMenu stack bookkeeping"
    )
    assert "g_policy.TargetPaused()" in pause_removed, (
        "PauseHold must reconcile a stale hide against the latest pause target"
    )
    assert "g_policy.InvalidateMessageState()" in pause_removed, (
        "PauseHold removal must invalidate sent state so a deferred reopen remains retryable"
    )
    assert "closingAllMenus" in pause_removed, (
        "PauseHold must not reopen while Fallout is intentionally closing all menus"
    )
    assert "AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow)" in pause_removed, (
        "PauseHold must reopen after an actual stale removal instead of assuming queue ordering"
    )
    assert pause_set.find("g_policy.SetTarget(paused)") < pause_set.find("F4SE::GetTaskInterface()"), (
        "PauseHold::Set must publish the requested pause target even if task dispatch is unavailable"
    )
    assert "closingAllMenus" in pause_set and "g_policy.InvalidateMessageState()" in pause_set, (
        "PauseHold::Set must defer menu messages while Fallout is closing all menus"
    )
    assert "g_policy.ShouldSendMessage" in pause_tick, (
        "PauseHold::Tick must retry a desired state whose engine message was invalidated"
    )
    assert "g_policy.InvalidateMessageState()" in pause_tick, (
        "PauseHold verification mismatch must become retryable instead of only being logged"
    )
    for name, body in (("Set", pause_set), ("Tick", pause_tick)):
        task_lookup = body.find("F4SE::GetTaskInterface()")
        null_check = body.find("if (!tasks)")
        dispatch = body.find("tasks->AddUITask")
        assert task_lookup != -1 and null_check != -1 and dispatch != -1, (
            f"PauseHold::{name} must guard the F4SE task interface before dispatch"
        )
        assert task_lookup < null_check < dispatch, (
            f"PauseHold::{name} task-interface guard must precede AddUITask"
        )

    input_source = Path("src/PrismaUI/WebRuntimeInput.cpp").read_text(encoding="utf-8")
    focus_source = Path("src/PrismaUI/WebRuntimeFocus.cpp").read_text(encoding="utf-8")
    unfocus = function_body(focus_source, "void Unfocus(ViewId view)")
    inspector_gate = unfocus.find(
        "if (st.focusedView.load() == 0 && st.localInspectorOwner.load(std::memory_order_acquire))"
    )
    inspector_return = unfocus.find("return;", inspector_gate)
    inspector_branch = unfocus[inspector_gate:inspector_return + len("return;")]
    focus_release = unfocus.find("if (st.focusedView.load() == 0 && st.focusSession.exchange(false))")
    inspector_tokens = (
        "ClearFocusedInputMode(view)",
        "EndInspectorGesture();",
        "SetInputTargetView(0)",
        "WebInput::SetCaptureActive(true, true)",
        "SyncVanillaCursorVisibility();",
    )
    inspector_positions = [inspector_branch.find(token) for token in inspector_tokens]
    owner = unfocus.find("InputOwnershipPolicy::AfterFocusedViewRelease(st.dockVisible.load())")
    target = unfocus.find("SetInputTargetView(owner == InputOwnershipPolicy::CaptureOwner::kPassiveDock")
    dock = unfocus.find("st.dockView.load(std::memory_order_acquire)")
    capture = unfocus.find("WebInput::SetCaptureActive(owner != InputOwnershipPolicy::CaptureOwner::kNone, false)")
    assert inspector_gate != -1 and inspector_return != -1 and -1 not in inspector_positions, (
        "Unfocus must preserve active inspector input ownership"
    )
    assert inspector_return < focus_release < owner, (
        "Inspector ownership must take priority over Dock/none restoration during Unfocus"
    )
    assert -1 not in (owner, target, dock, capture), (
        "Unfocus must restore the Dock input target when passive Dock capture resumes"
    )
    assert owner < target <= dock < capture, (
        "Unfocus must choose the post-focus owner, restore its target, then enable capture"
    )

    menu_release = unfocus.find("FocusMenu::Close()")
    pause_release = unfocus.find("PauseHold::Set(false)")
    mask_release = unfocus.find("InputMask::Release(InputMask::Owner::kFocus)")
    cursor_release = unfocus.find("menuCursor->UnregisterCursor()")
    assert -1 not in (menu_release, pause_release, mask_release, cursor_release), (
        "Unfocus must release menu, pause, input-mask, and cursor ownership"
    )
    assert menu_release < mask_release and pause_release < mask_release < cursor_release, (
        "Unfocus must publish menu and pause release before dropping refcounted input and cursor owners"
    )

    show_dock = function_body(input_source, "void ShowDock()")
    hide_dock = function_body(input_source, "void HideDock()")
    assert "st.localInspectorOwner.load(std::memory_order_acquire)" in show_dock, (
        "ShowDock must observe active inspector ownership before changing capture"
    )
    assert "focused == 0 && inspectorOwner == 0" in show_dock, (
        "ShowDock must not replace active inspector capture with passive Dock capture"
    )
    assert "st.focusedView.load() == 0 && st.localInspectorOwner.load(std::memory_order_acquire) == 0" in hide_dock, (
        "HideDock must not disable capture while the inspector still owns input"
    )

    print("focus semantics guard passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, ValueError) as exc:
        print(f"focus semantics guard failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
