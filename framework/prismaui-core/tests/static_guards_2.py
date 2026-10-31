from pathlib import Path
import sys

from static_guard_support import check, failures, function_body, read, read_web_runtime

@check("game-thread callbacks use a verified Fallout HWND boundary")
def _game_thread_dispatch():
    dispatcher = read("src/PrismaUI/GameThreadDispatcher.cpp")
    queue = read("src/PrismaUI/GameThreadDispatchQueue.h")
    input_source = read("src/PrismaUI/WebInput.cpp")
    wnd_proc_source = read("src/PrismaUI/WebInputWndProc.cpp")
    api = read("src/API/GameThreadAPI.cpp")
    api_header = read("src/API/API.h")
    public_v11 = read("src/PrismaUI_F4_API_V11.h")
    main = read("src/main.cpp")
    vr_main = read("src/VR/VRMain.cpp")
    bridge = read_web_runtime()
    xmake = read("xmake.lua")

    for token in (
        "RegisterWindowMessageW",
        "GetWindowThreadProcessId",
        "GetCurrentThreadId",
        "PostMessageW",
    ):
        assert token in dispatcher, f"verified dispatcher lost {token}"
    assert "ownerThread != currentThread" in dispatcher, (
        "the Fallout HWND owner thread must be compared with the callback thread"
    )
    assert "if (failed_) return {false, false, Rejection::Failed};" in queue and         "if (!ready_) return {false, false, Rejection::NotReady};" in queue, (
        "Dispatch must not retain work before the dispatcher is ready"
    )
    assert "wakePending_" in queue, "cross-thread wakes must coalesce onto one HWND message"
    assert "kMaxQueue" in queue, "the public dispatch API must have a backpressure ceiling"
    assert "takeNext" in queue, "a wake must keep view/generation metadata until each invoke"
    assert "std::vector<std::function<void()>> drain" not in queue, (
        "drain() discards metadata before invoke and lets Destroy miss already-drained callbacks"
    )
    assert "kMaxTasksPerWake" in queue, "one HWND wake must not run the entire 1024-cap queue"
    assert "cancelled_" in queue, "DropView must tombstone the view so late producers cannot enqueue"
    assert "beginDrain" in queue and "draining_" in queue, (
        "an active drain must suppress extra wakes and be finished before rearm"
    )
    assert "if (executing_) return std::nullopt;" in queue, (
        "takeNext must refuse a nested callback while one is already executing"
    )
    assert "DispatchSafety" in dispatcher, "emergency Unfocus needs a reserved dispatcher slot"
    assert "t_insideDrain" in dispatcher, "HWND drain must be non-reentrant"
    assert "drainThread{currentThread}" in dispatcher, (
        "DrainThreadScope must publish the drain thread id in its constructor, not in a later store"
    )
    assert "CompositeResult WebCompositor::Draw" in read("src/VR/VRUnportedStubs.cpp"), (
        "the VR compositor stub must match the flat Draw signature"
    )
    assert "return CompositeResult::NotInitialized" in read("src/VR/VRUnportedStubs.cpp")
    compositor_h = read("src/PrismaUI/WebCompositor.h")
    assert "inline const char* ToString(CompositeResult" in compositor_h, (
        "ToString(CompositeResult) must stay header-inline: VR compiles WebRuntime.cpp "
        "but not WebCompositor.cpp"
    )
    conflict = read("src/Utils/ConflictChecker.cpp")
    known_fg = conflict[conflict.index("kKnownFrameGenDlls[]"):conflict.index("bool EqualsIgnoreCase")]
    for dll in ("AAAFrameGeneration.dll", "amd_fidelityfx_dx12.dll", "nvngx_dlssg.dll", "dlssg_to_fsr3.dll"):
        assert dll in known_fg, f"IsFrameGenProxy lost {dll}; diagnostics and borrowed-backbuffer must share one list"
    assert "kFrameGenDlls" not in conflict, (
        "LogSystemSummary must not keep a second FrameGen DLL list that can drift from IsFrameGenProxy"
    )
    assert "g_installMutex" in input_source, "WebInput::Install/Uninstall must be serialized"
    assert "std::lock_guard lock(g_installMutex)" in function_body(input_source, "bool Install(HWND gameHwnd)")
    assert "std::lock_guard lock(g_installMutex)" in function_body(input_source, "void Uninstall()")
    assert "CaptureCurrentThread();" in main, "provider load must capture its diagnostic thread candidate"
    assert "TryInstallGameInput();" in main, (
        "HWND subclass/dispatch attach must be attempted from kGameDataReady, not only Present"
    )
    assert "void TryInstallGameInput()" in bridge
    install_now = function_body(bridge, "void DiscoverInputWindow()")
    install_try = function_body(bridge, "void TryInstallGameInput()")
    assert "WebInput::QueueInstall(h)" in install_now
    assert "WebInput::Install(h)" not in install_now
    assert "WebInput::QueueInstall(h)" in install_try or "DiscoverInputWindow" in install_try
    for token in ("st.inputInstallPosted.exchange(true)", "st.inputInstallPosted.store(false);"):
        assert token in install_try, f"input discovery lost its serialized task state: {token}"
    assert install_try.count("st.inputInstallPosted.exchange(true)") == 1
    assert install_try.count("st.inputInstallPosted.store(false);") == 2
    assert "task->AddTask([]" in install_try
    post = install_try.index("task->AddTask([]")
    assert install_try.index("DiscoverInputWindow();", post) > post
    assert "WebInput::Install" not in install_try, "F4SE AddTask must not be the HWND subclass execution boundary"
    assert "bool QueueInstall(HWND gameHwnd)" in input_source
    bootstrap = function_body(input_source, "bool QueueInstall(HWND gameHwnd)")
    bootstrap_source = read("src/PrismaUI/HwndThreadBootstrap.cpp")
    for token in (
        "GetWindowThreadProcessId",
        "GetCurrentThreadId",
        "SetWindowsHookExW(WH_CALLWNDPROC",
        "PostMessageW",
        "CWPSTRUCT",
        "CallNextHookEx",
        "UnhookWindowsHookEx",
    ):
        assert token in bootstrap_source, f"HWND-thread bootstrap lost {token}"
    assert "HwndThreadBootstrap::Queue(gameHwnd, &Install)" in bootstrap
    assert "owner == ::GetCurrentThreadId()" in bootstrap_source
    assert "return callback(hwnd);" in bootstrap_source
    assert "GetWindowThreadProcessId(gameHwnd" in function_body(input_source, "bool Install(HWND gameHwnd)")
    assert "GetCurrentThreadId()" in function_body(input_source, "bool Install(HWND gameHwnd)")
    assert "SetWindowSubclass(gameHwnd" in function_body(input_source, "bool Install(HWND gameHwnd)")
    assert "AttachWindow(gameHwnd)" in input_source
    assert "RemoveWindowSubclass(gameHwnd" in function_body(input_source, "bool Install(HWND gameHwnd)")
    assert "HwndThreadBootstrap::Cancel" in input_source
    assert "OnWindowDestroyed(HWND hwnd)" in input_source
    assert "OnWindowDestroyed(hwnd)" in function_body(wnd_proc_source, "LRESULT CALLBACK SubclassProc")
    assert "HandleWindowMessage(hwnd, uMsg)" in wnd_proc_source, (
        "the Fallout WndProc must be the execution boundary for queued callbacks"
    )
    dispatcher = read("src/PrismaUI/GameThreadDispatcher.cpp")
    attach = function_body(dispatcher, "bool AttachWindow(HWND hwnd)")
    assert "VerifyWindowThread(hwnd)" in attach
    assert "GetCurrentThreadId()" in function_body(dispatcher, "DWORD VerifyWindowThread(HWND hwnd)")
    assert "g_gameThreadId.store(ownerThread" in function_body(dispatcher, "bool Publish(HWND hwnd, DWORD ownerThread)")
    assert "ownerThread != expectedThread" not in attach
    assert "#ifndef PRISMAUI_FO4VR\n        if (!GameThreadDispatcher::AttachWindow(gameHwnd))" in input_source, (
        "AttachWindow must stay behind PRISMAUI_FO4VR in WebInput.cpp"
    )
    assert "#ifndef PRISMAUI_FO4VR\n    bool ReattachDispatcher(HWND hwnd)" in input_source, (
        "ReattachDispatcher must stay behind PRISMAUI_FO4VR in WebInput.cpp"
    )
    vr_marker = 'set_filename("PrismaUI_F4VR.dll")'
    assert vr_marker in xmake, "the VR shared-library target is missing"
    vr_chunk = xmake[xmake.index(vr_marker):xmake.index(vr_marker) + 2500]
    assert "src/PrismaUI/GameThreadDispatcher.cpp" not in vr_chunk, (
        "the VR target must not compile GameThreadDispatcher.cpp"
    )
    assert '"src/PrismaUI/WebCompositor.cpp"' not in vr_chunk, (
        "the VR target must not compile WebCompositor.cpp; ToString lives in the header"
    )
    assert "F4SE::GetTaskInterface" not in api, (
        "V11/verified BindUIEvent must never regress to treating F4SE AddTask as a window-thread marshal"
    )
    assert api.count("GameThreadDispatcher::IsReady()") >= 2, (
        "V11 BindGameThreadUIEvent and V4 BindUIEvent must both refuse to register when the "
        "dispatcher cannot run the callback"
    )
    assert 'not registering' in api, (
        "V4 BindUIEvent cannot return false, so it must log a real failure instead of claiming "
        "a verified window-thread listener was registered"
    )
    assert "DropView(view)" in bridge, (
        "DestroyView must drop already-queued UI-event jobs for that view"
    )
    assert "class VerifiedPrismaUIInterface final" in api_header
    assert "return PluginAPI::VerifiedPrismaUIInterface::GetSingleton();" in api_header, (
        "all requested interface versions must use the provider that overrides legacy BindUIEvent"
    )
    assert "DispatchToGameThread" in public_v11
    assert "BindGameThreadUIEvent" in public_v11
    assert "PrismaUIInterfaceV11" in main, "flat RequestPluginAPI must expose V11"
    vr_target = xmake[xmake.index(vr_marker):xmake.index('set_pcxxheader("src/PCH.h")', xmake.index(vr_marker))]
    assert 'remove_files("src/API/GameThreadAPI.cpp")' in vr_target, (
        "the VR target must drop the flat GameThreadAPI.cpp; src/VR/VRGameThreadAPI.cpp replaces it"
    )
    assert "PrismaUIInterface::BindUIEvent(view, functionName, callback);" in read("src/VR/VRGameThreadAPI.cpp"), (
        "VR must preserve the existing V1-V10 BindUIEvent path until it has its own verified boundary"
    )
    assert "PrismaUIInterfaceV11" not in vr_main, (
        "FO4VR must not expose V11 before its window-thread boundary is independently verified"
    )
    assert "[AE-DIAG]" not in bridge, (
        "AE cursor diagnostics belong on the unmerged diagnostic PR, not the production stack"
    )
    assert "kUnresponsiveGraceMs" in bridge, "unresponsive recovery must use elapsed time, not frame count"
    assert "kUnresponsiveGraceFrames" not in bridge
    assert "if (ScheduleFocusRelease(focused)) handledGen = gen;" in bridge, (
        "Present-independent recovery must not latch handledGen before scheduling succeeds"
    )
    assert "s_handled = ScheduleFocusRelease(focused);" in bridge, (
        "Present-driven recovery must not latch s_handled before scheduling succeeds"
    )

@check("view destruction unwinds before Chromium iframe teardown")
def _deferred_view_destroy():
    shell = read("shell/src/main.ts")
    assert 'if (iframe.dataset.prismaDestroying === "1") return;' in shell, (
        "a destroying iframe must not emit a late domReady for a dead native view"
    )
    assert 'views.delete(id);' in shell, "logical destruction must be immediate"
    assert 'iframe.dataset.prismaDestroying = "1";' in shell
    assert 'iframe.style.pointerEvents = "none";' in shell
    assert 'iframe.style.display = "none";' in shell
    assert 'setTimeout(() => iframe.remove(), 0);' in shell, (
        "physical iframe removal must be deferred out of the Host ExecuteJavaScript turn"
    )
    destroy = shell.index("destroyView(id)")
    deferred_remove = shell.index("setTimeout(() => iframe.remove(), 0);", destroy)
    direct_remove = shell.find("iframe.remove();", destroy, deferred_remove)
    assert direct_remove == -1, "destroyView must not synchronously remove the live iframe"

@check("VR compositor registers the submitted color target before depth acquire")
def _vr_scene_depth_registers_submitted_target():
    compositor = read("src/VR/VRCompositor.cpp")
    render = function_body(compositor, "void RenderSubmittedTexture(")
    acquire = render.find("AcquireForSubmittedTarget(")
    register = render.find("SetSubmittedTarget(")
    assert register != -1, (
        "RenderSubmittedTexture must call SceneDepthCapture::SetSubmittedTarget so "
        "KnownSubmittedTarget can match the engine color buffer"
    )
    assert acquire != -1, "RenderSubmittedTexture must still acquire the captured DSV"
    assert register < acquire, (
        "SetSubmittedTarget must run before AcquireForSubmittedTarget; otherwise "
        "KnownSubmittedTarget always fails and world quads with occlusion never draw"
    )
    assert "SceneDepthDrawPolicy.h" in compositor, (
        "world-quad depth fallback must use the tested SceneDepthDrawPolicy"
    )

@check("VR compositor AddRefs the view SRV while g_offscreenMutex is held")
def _vr_view_srv_owned_for_submit():
    compositor = read("src/VR/VRCompositor.cpp")
    bridge = read_web_runtime()
    frames = read("src/PrismaUI/OffscreenFrames.cpp")
    resolve = function_body(compositor, "bool ResolveTarget(")

    forwarder = function_body(bridge, "AcquireViewSRV(")
    assert "OffscreenFrames::AcquireSRV" in forwarder, (
        "WebRuntime::AcquireViewSRV must forward to OffscreenFrames::AcquireSRV, which is where the "
        "AddRef-under-lock happens"
    )
    acquire = function_body(frames, "AcquireSRV(")
    public_get = function_body(bridge, "void* GetViewSRV(")
    assert "AcquireViewSRV" in resolve, (
        "ResolveTarget must acquire via AcquireViewSRV so AddRef happens under g_offscreenMutex"
    )
    assert "GetViewSRV" not in resolve, (
        "ResolveTarget must not use the public borrowed GetViewSRV; that unlocks before AddRef"
    )
    assert "g_offscreenMutex" in acquire
    assert "return it->second.srv" in acquire or "return it->second.srv;" in acquire.replace(" ", ""), (
        "AcquireViewSRV must copy the ComPtr (AddRef) before the lock_guard destructor runs"
    )

    assert "OffscreenFrames::PeekSRV" in public_get, (
        "public GetViewSRV must forward to OffscreenFrames::PeekSRV"
    )
    peek = function_body(frames, "void* PeekSRV(")
    assert ".Get()" in peek, (
        "PeekSRV stays a borrowed pointer for the existing IVPrismaUI5 contract"
    )

    assert ".Attach(" not in resolve, (
        "ResolveTarget must not Attach() a borrowed GetViewSRV pointer"
    )

@check("VR reticle uses the same effective depth decision as the panel")
def _vr_reticle_uses_effective_depth():
    compositor = read("src/VR/VRCompositor.cpp")
    render = function_body(compositor, "void RenderSubmittedTexture(")
    assert "useDepth" in render
    draw_reticle = render.find("DrawReticle(")
    assert draw_reticle != -1, "RenderSubmittedTexture must still draw the pointer reticle"
    reticle_chunk = render[draw_reticle:draw_reticle + 280]
    assert "requestsDepth" not in reticle_chunk, (
        "DrawReticle must not receive the original request flag; a requested-but-missing "
        "snapshot would set useDepth=true and skip the reticle"
    )
    assert "useDepth" in reticle_chunk, (
        "DrawReticle must use the SceneDepthDrawPolicy decision already applied to the panel"
    )

@check("Ultralight inspector API stays in-game")
def _inspector_is_in_process():
    api = read("src/API/API.cpp") + read("src/API/ListenerInspectorAPI.cpp")
    mapping = read("src/API/ConsoleMessageMapping.h")
    assert '#include "API/ConsoleMessageMapping.h"' in api
    assert "MapUltralightLogSeverity" in mapping
    assert "DispatchConsoleMessage" in mapping
    assert "PrismaUI::API::DispatchConsoleMessage" in api
    assert "Dispatcher dispatch) noexcept" not in mapping
    assert "MapCefLogSeverity" not in mapping
    assert "case 0: return PRISMA_UI_API::ConsoleMessageLevel::Log;" in mapping
    assert "case 1: return PRISMA_UI_API::ConsoleMessageLevel::Warning;" in mapping
    assert "case 2: return PRISMA_UI_API::ConsoleMessageLevel::Error;" in mapping
    assert "case 3: return PRISMA_UI_API::ConsoleMessageLevel::Debug;" in mapping
    assert "case 4: return PRISMA_UI_API::ConsoleMessageLevel::Info;" in mapping
    assert "ShellExecuteW" not in api
    assert "PrismaUI::WebRuntime::CreateInspectorView" in api
    assert "PrismaUI::WebRuntime::SetInspectorVisibility" in api
    assert "PrismaUI::WebRuntime::SetInspectorBounds" in api

@check("bind entry points hold an owned view SRV across the MeshBinding call")
def _bind_entry_points_own_view_srv():
    api = read("src/API/API.cpp")
    for signature, bind in (
        ("bool PluginAPI::PrismaUIInterface::BindViewToGeometry(", "PrismaUI::MeshBinding::BindToGeometry("),
        ("bool PluginAPI::PrismaUIInterface::BindViewToScreenTexture(", "PrismaUI::MeshBinding::BindToScreenTexture("),
    ):
        body = function_body(api, signature)
        acquire = body.find("PrismaUI::WebRuntime::AcquireViewSRV(")
        call = body.find(bind)
        assert acquire != -1, f"{signature} must acquire the view SRV through AcquireViewSRV"
        assert "Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>" in body, (
            f"{signature} must hold the acquired SRV in a ComPtr"
        )
        assert "GetViewSRV" not in body, f"{signature} must not use the borrowed GetViewSRV pointer"
        assert call > acquire, (
            f"{signature} must keep the owned SRV until {bind} AddRefs it in MakeBinding"
        )

@check("FrameGen DLL list is owned only by ConflictChecker.cpp")
def _frame_gen_list_single_owner():
    diagnostics = read("src/Utils/ConflictDiagnostics.cpp")
    for dll in (
        "AAAFrameGeneration.dll",
        "amd_fidelityfx_dx12.dll",
        "nvngx_dlssg.dll",
        "dlssg_to_fsr3.dll",
        "sl.interposer.dll",
        "sl.dlss_g.dll",
    ):
        assert dll not in diagnostics, f"ConflictDiagnostics.cpp must not declare its own FrameGen list ({dll})"
    assert "kKnownFrameGenDlls" not in diagnostics, "ConflictDiagnostics.cpp must not copy kKnownFrameGenDlls"
    assert "LoadedKnownFrameGenModules()" in diagnostics, (
        "LogSystemSummary must read frame-gen modules through ConflictChecker::LoadedKnownFrameGenModules"
    )

@check("VerifiedOwnedProxy rule keeps its identity constants, same-module, TimeDateStamp and file-name checks")
def _verified_owned_proxy_rule():
    source = read("src/Utils/VerifiedOwnedProxy.cpp")
    body = " ".join(function_body(source, "VerifiedOwnedProxyMatch MatchVerifiedOwnedProxy(").split())
    assert '{L"Upscaling.dll", 0x6AC8FD7B, "Upscaling 108772"}' in source, (
        "kVerifiedOwnedProxyBuilds must keep the verified Upscaling 108772 name and TimeDateStamp"
    )
    assert "if (!SameModuleIdentity(present, getBuffer) || !SameModuleIdentity(getBuffer, resizeBuffers))" in body, (
        "MatchVerifiedOwnedProxy must require Present, GetBuffer and ResizeBuffers to belong to one module"
    )
    assert "if (build.timeDateStamp != stamp) continue;" in body, (
        "MatchVerifiedOwnedProxy must skip builds whose PE TimeDateStamp differs from kVerifiedOwnedProxyBuilds"
    )
    assert "if (::CompareStringOrdinal(fileName.c_str(), -1, build.fileName, -1, TRUE) != CSTR_EQUAL) continue;" in body, (
        "MatchVerifiedOwnedProxy must skip builds whose file name does not match, not the name alone"
    )

@check("chain install seq is bumped before the Active store and compared after the originals load")
def _chain_install_seq_orders_reads():
    hooks = read("src/Hooks/Hooks.cpp")
    lifecycle = read("src/Hooks/HooksLifecycle.inl")
    install = function_body(hooks, "bool InstallChainSlot(")
    bump = "s_chainInstallSeq[slot].fetch_add(1, std::memory_order_release);"
    assert bump in install, "InstallChainSlot must bump s_chainInstallSeq so a reinstall is visible to readers"
    assert install.index(bump) < install.index("s_chainState[slot].store(ChainRetirementState::Active"), (
        "the install seq must be bumped before the Active store"
    )
    originals = " ".join(function_body(lifecycle, "bool D3DHooks::OriginalSwapchainSlots(").split())
    seq_load = "s_chainInstallSeq[slot].load(std::memory_order_acquire)"
    assert originals.index(seq_load) < originals.index("ChainShadowVtable(slot).load("), (
        "OriginalSwapchainSlots must read the install seq before the shadow and previous-target loads"
    )
    last_load = originals.index("ChainPreviousResize(slot).load(")
    assert originals.find(f"{seq_load} != seq) continue;", last_load) > last_load, (
        "OriginalSwapchainSlots must compare the install seq after the shadow and previous-target loads"
    )

if __name__ == "__main__":
    if failures:
        print(f"{len(failures)} guard(s) failed")
        sys.exit(1)
