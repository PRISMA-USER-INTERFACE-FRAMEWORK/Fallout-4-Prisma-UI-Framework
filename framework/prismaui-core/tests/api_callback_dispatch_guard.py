from pathlib import Path

SOURCE = (
    Path("src/API/API.cpp").read_text(encoding="utf-8")
    + Path("src/API/WebRuntimeAPI.cpp").read_text(encoding="utf-8")
    + Path("src/API/ListenerInspectorAPI.cpp").read_text(encoding="utf-8")
)

def require(needle: str, message: str) -> None:
    if needle not in SOURCE:
        raise AssertionError(message)

def forbid(needle: str, message: str) -> None:
    if needle in SOURCE:
        raise AssertionError(message)

require('#include "PrismaUI/GameThreadDispatcher.h"', "API callbacks must reuse the framework game-thread dispatcher")
require("bool DispatchApiCallback(std::function<void()> task, PrismaView view)", "API callback dispatch helper is missing")
require("if (PrismaUI::GameThreadDispatcher::Dispatch(task, view)) return true;", "API callback dispatch must prefer the view-scoped game-thread dispatcher")
require("if (const auto* taskInterface = F4SE::GetTaskInterface())", "API callback dispatch must null-check the F4SE fallback")
require("if (view != 0)", "view-scoped F4SE fallback must keep a lifetime guard")
require("taskInterface->AddTask([task = std::move(task), view]() mutable {", "view-scoped F4SE fallback must defer through a guarded wrapper")
if SOURCE.count("if (PrismaUI::WebRuntime::IsValid(view)) task();") != 1:
    raise AssertionError("view-scoped F4SE fallback must re-check view lifetime exactly once at execution")
require("taskInterface->AddTask(std::move(task));", "unscoped API callback dispatch must preserve the F4SE task fallback")
require("PrismaUI API callback dropped because no game-thread dispatcher is available", "API callback dispatch must fail closed when no safe route exists")
for needle in (
    "CreateViewShared(htmlPath, owner, onDomReady, DispatchApiCallback);",
    "CreateViewShared(htmlPath, owner, task, DispatchApiCallback);",
    "DispatchApiCallback([callback, result]",
    "DispatchApiCallback([targetCallback = callback",
    "PrismaUI::API::DispatchConsoleMessage(view, levelValue, std::move(msg), callback, mapLevel",
    "DispatchApiCallback([callback, view, fnName, arg]",
):
    require(needle, f"public API callback path bypasses guarded dispatch: {needle}")
forbid("F4SE::GetTaskInterface()->", "public API must never dereference a nullable F4SE task interface directly")
if SOURCE.count("(void)DispatchApiCallback([callback, result]() { callback(result.c_str()); });") != 2:
    raise AssertionError("Invoke completion callbacks must remain unscoped so invalid/destroyed views still receive their one-shot result")
forbid("DispatchApiCallback([callback, result]() { callback(result.c_str()); }, view)", "Invoke completion must not be dropped with view-scoped event callbacks")
print("api callback dispatch guard passed")
