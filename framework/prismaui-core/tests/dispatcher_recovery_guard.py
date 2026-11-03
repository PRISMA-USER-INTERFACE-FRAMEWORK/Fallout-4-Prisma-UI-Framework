import re
from pathlib import Path

SRC = Path(__file__).resolve().parents[1] / "src" / "PrismaUI"


def read(name: str) -> str:
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[brace:i + 1]
    raise AssertionError(f"unterminated {signature}")


dispatcher = read("GameThreadDispatcher.cpp")
attach = body(dispatcher, "bool AttachWindow(HWND hwnd) noexcept")
assert re.search(r"if\s*\(\s*!hwnd\s*\|\|\s*g_failed", attach), \
    "ordinary AttachWindow must refuse while the dispatcher is failed closed"
assert "recoverAfterFailure" not in attach and "g_failed.store" not in attach, \
    "ordinary AttachWindow must never clear fail-closed state"
recover_fn = body(dispatcher, "bool RecoverWindow(HWND hwnd) noexcept")
verify = recover_fn.index("VerifyWindowThread(hwnd)")
recover = recover_fn.index("g_tasks.recoverAfterFailure()")
assert verify < recover, "failure may only be cleared after the Fallout window thread is re-verified"
assert dispatcher.count("recoverAfterFailure()") == 1, "RecoverWindow is the only place that clears failure"
all_src = "".join(f.read_text(encoding="utf-8", errors="replace") for f in SRC.rglob("*.cpp"))
assert all_src.count("GameThreadDispatcher::RecoverWindow(") == 1, \
    "only the HwndThreadBootstrap re-attach path may call RecoverWindow"

for name in ("bool Dispatch(std::function<void()> task, uint64_t view)", "bool DispatchSafety(std::function<void()> task)"):
    fn = body(dispatcher, name)
    assert "NoteRejected(" in fn, f"{name} must report why a task was rejected"
assert "wake-failure" in dispatcher, "PostWake failure must be reported as a wake-failure rejection"

lifecycle = read("WebRuntimeLifecycle.cpp")
install = body(lifecycle, "void TryInstallGameInput()")
installed = install.index("WebInput::IsInstalled()")
assert "GameThreadDispatcher::IsReady()" in install[installed:installed + 300], \
    "an installed WebInput must not stop recovery while the dispatcher is not ready"
assert "WebInput::QueueDispatcherReattach()" in install, "recovery must re-verify the installed HWND"

webinput = read("WebInput.cpp")
reattach = body(webinput, "bool ReattachDispatcher(HWND hwnd)")
assert "g_window.Get()" in reattach and "GameThreadDispatcher::RecoverWindow(hwnd)" in reattach, \
    "re-attach must target only the already-installed Fallout HWND"
queue = body(webinput, "bool QueueDispatcherReattach()")
assert "HwndThreadBootstrap::Queue(hwnd, &ReattachDispatcher)" in queue, \
    "re-attach must run on the window thread, never through an arbitrary F4SE task"
assert "GetTaskInterface" not in queue and "AddTask" not in queue
assert "nowMs - last < 1000" in queue, "re-attach requests from the Present path must be rate-limited"

print("dispatcher recovery guard passed")
