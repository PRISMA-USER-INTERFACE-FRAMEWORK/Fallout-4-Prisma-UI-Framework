from pathlib import Path
import sys

from static_guard_support import check, failures, function_body, read, read_web_runtime

@check("recovery has an owner independent of Present")
def _independent_watchdog():

    source = read_web_runtime()
    assert "StartRecoveryWatchdog" in source, "the Present-independent recovery watchdog is missing"
    watchdog = function_body(source, "void PresentIndependentWatchdog()")
    assert "TickPauseMaintenance" not in watchdog, (
        "the watchdog must not run frame advance; it only schedules recovery"
    )
    assert "ScheduleFocusRelease" in watchdog, "the watchdog must release focus when Present goes silent"
    assert "AddTask([focused] { Unfocus" not in watchdog, (
        "Unfocus must not treat F4SE AddTask as the verified engine-mutation boundary"
    )
    assert "TryInstallGameInput();" in watchdog, (
        "the watchdog must reuse the serialized input-install request path"
    )
    assert "st.inputInstallPosted.exchange(true)" not in watchdog, (
        "the watchdog must not duplicate the input-install scheduler"
    )
    release = function_body(source, "bool ScheduleFocusRelease(ViewId focused)")
    assert "DispatchSafety" in release, (
        "flat Unfocus recovery must use the reserved safety slot so a full consumer queue cannot starve it"
    )
    assert "AddTask([focused] { Unfocus" not in release, (
        "Unfocus recovery must fail closed after the verified dispatcher rejects it"
    )
    assert "if (ScheduleFocusRelease(focused)) handledGen = gen;" in watchdog, (
        "the watchdog must not mark an episode handled until Unfocus was actually queued"
    )
    assert "s_handled = ScheduleFocusRelease(focused);" in source, (
        "Present-driven recovery must not latch handled across a failed schedule"
    )
    focus = function_body(source, "bool Internal::FocusImpl(ViewId view, bool pauseGame, bool disableFocusMenu,")
    assert "GameThreadDispatcher::IsReady()" in focus, (
        "flat Focus must refuse pause/input ownership without a usable safe-release dispatcher"
    )

@check("DLL loading stays scoped")
def _scoped_dll():
    hits = [
        f"{path}:{n}"
        for path in sorted(Path("src").rglob("*"))
        if path.is_file() and path.suffix in {".cpp", ".h"}
        for n, line in enumerate(path.read_text(encoding="utf-8", errors="ignore").splitlines(), 1)
        if "SetDllDirectoryW" in line
    ]
    assert not hits, (
        f"SetDllDirectoryW is process-global and never restored; it must not come back: {hits}"
    )
    bridge = read_web_runtime()
    assert "LoadLibraryW" not in bridge
    assert "WebRuntimeUltralight::Backend()" in bridge

@check("frame advancement has exactly one owner")
def _frame_owner():
    bridge = read_web_runtime()
    assert "FrameAdvanceGuard AdvanceFrame(IDXGISwapChain* swapChain)" in bridge
    assert "auto frame = AdvanceFrame(swapChain);" in bridge
    assert "auto frame = WebRuntime::AdvanceFrame();" in read("src/VR/VRCompositor.cpp"), (
        "VR must advance the frame through the shared owner, not its own pump"
    )
    stragglers = [
        str(path)
        for path in sorted(Path("src").rglob("*"))
        if path.is_file() and path.suffix in {".cpp", ".h"}
        and "PumpOffscreenFrames" in path.read_text(encoding="utf-8", errors="ignore")
    ]
    assert not stragglers, f"the retired renderer-specific pump must stay gone: {stragglers}"

@check("ordinary Win32 messages keep chaining to DefSubclassProc")
def _subclass_chaining():
    source = read("src/PrismaUI/WebInputWndProc.cpp")
    assert "if (forwarded && g_swallow.load())" not in source, (
        "ordinary forwarded Win32 messages must continue through DefSubclassProc, or the game "
        "stops seeing input the UI did not consume"
    )
    assert "return DefSubclassProc(hwnd, uMsg, wParam, lParam);" in source

@check("compositor drops overlay SRV-size cache on RTV invalidation")
def _compositor_overlay_cache_cleared_on_invalidate():
    source = read("src/PrismaUI/WebCompositor.cpp")
    invalidate = function_body(source, "void WebCompositor::InvalidateRenderTarget()")
    assert "m_overlaySourceSizes.clear();" in invalidate, (
        "InvalidateRenderTarget must drop cached overlay SRV dimensions; a swapchain/device "
        "change can recycle those pointers"
    )
    assert "m_rtv.Reset();" in invalidate, "InvalidateRenderTarget must still release the backbuffer RTV"
    assert "m_ownership = Backbuffer::Ownership::Unclassified" in invalidate, (
        "InvalidateRenderTarget must forget the last ownership verdict so a resize re-asks"
    )

@check("WebInput keeps mouse-only work off the keyboard path")
def _cefinput_hotpath_gate():
    source = read("src/PrismaUI/WebInputWndProc.cpp")
    subclass = function_body(source, "LRESULT CALLBACK SubclassProc(")
    mouse_gate = subclass.index("if (mouseMessage) {")
    dispatch_switch = subclass.index("switch (uMsg)")
    mouse_path = subclass[mouse_gate:dispatch_switch]

    for token in (
        "MapToFocusedOffscreenPage(hwnd, x, y)",
        "WebRuntime::MapClientPointToBrowser(routeX, routeY)",
        "WebRuntime::ShouldRouteFocusedMouse(routeX, routeY)",
        "MouseModifiers(wParam)",
    ):
        assert subclass.count(token) == 1, f"{token} must have exactly one SubclassProc call site"
        assert token in mouse_path, f"{token} escaped the mouse-message gate"

    assert "if (!gestureCaptured)" in mouse_path, (
        "captured drags must skip repeated region-policy evaluation until release"
    )
    assert mouse_path.index("if (!gestureCaptured)") < mouse_path.index("ShouldRouteFocusedMouse"), (
        "ShouldRouteFocusedMouse must remain behind the captured-gesture fast path"
    )

    mouse_modifiers = function_body(source, "uint32_t MouseModifiers(WPARAM wParam)")
    for token in ("MK_SHIFT", "MK_CONTROL", "MK_LBUTTON", "MK_MBUTTON", "MK_RBUTTON"):
        assert token in mouse_modifiers, f"MouseModifiers must derive {token} from message wParam"
    assert "GetKeyState(VK_MENU)" in mouse_modifiers, (
        "Alt is not carried by mouse wParam and must still be queried explicitly"
    )
    for key in ("VK_SHIFT", "VK_CONTROL", "VK_LBUTTON", "VK_MBUTTON", "VK_RBUTTON"):
        assert f"GetKeyState({key})" not in mouse_modifiers, (
            f"mouse hot path regressed to a redundant GetKeyState({key}) call"
        )

if __name__ == "__main__":
    if failures:
        print(f"{len(failures)} guard(s) failed")
        sys.exit(1)
