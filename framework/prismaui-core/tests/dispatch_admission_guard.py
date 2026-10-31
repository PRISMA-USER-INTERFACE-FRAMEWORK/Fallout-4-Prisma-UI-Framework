from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src/PrismaUI"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


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


queue_header = read(SRC / "UltralightDispatchQueue.h")
assert "[[nodiscard]] bool Dispatch(Task task);" in queue_header, (
    "UltralightDispatchQueue::Dispatch must report admission"
)
assert "[[nodiscard]] bool DispatchDeferred(Task task);" in queue_header, (
    "UltralightDispatchQueue::DispatchDeferred must stay [[nodiscard]]"
)
backend_header = read(SRC / "UltralightBackend.h")
assert "[[nodiscard]] bool DispatchDeferred(UltralightDispatchQueue::Task function);" in backend_header, (
    "UltralightBackend::DispatchDeferred must stay [[nodiscard]] so admission cannot be dropped"
)
assert "[[nodiscard]] bool Dispatch(UltralightDispatchQueue::Task function);" in backend_header, (
    "UltralightBackend::Dispatch must stay [[nodiscard]] so admission cannot be dropped"
)

admission = re.compile(r"^(return |if \(!?|const bool \w+ = |\(void\))(dispatchQueue_\.)?DispatchDeferred\(")
statement_dispatch = re.compile(r"^(return |if \(!?|const bool \w+ = |\(void\))(dispatchQueue_\.)?Dispatch\(")
for path in sorted(SRC.glob("UltralightBackend*.cpp")):
    for line in read(path).splitlines():
        stripped = line.strip()
        if "DispatchDeferred(" in stripped and "UltralightBackend::DispatchDeferred(" not in stripped:
            assert admission.match(stripped), (
                f"{path.name}: DispatchDeferred admission must be consumed or explicitly (void)-discarded: {stripped}"
            )
        if re.search(r"(?<![\w.])(dispatchQueue_\.)?Dispatch\(", stripped) and "::Dispatch(" not in stripped:
            assert statement_dispatch.match(stripped), (
                f"{path.name}: Dispatch admission must be consumed or explicitly (void)-discarded: {stripped}"
            )

views = read(SRC / "UltralightBackendViews.cpp")
create = function_body(views, "PrismaUI::Web::ViewId UltralightBackend::CreateView(")
assert "std::make_shared<OnceCompletion<PrismaUI::Web::ViewId>>(std::move(onReady), 0)" in create, (
    "CreateView must hold its DomReady callback in a OnceCompletion so an abandoned task still reports failure"
)
assert "auto domReady = done->Take();" in create, "CreateView's task must take ownership of the DomReady callback"
assert re.search(r"if \(!admitted\) \{\s*ErasePublishedView\(id\);\s*return 0;", create), (
    "CreateView must undo its reservation when the enqueue is refused"
)
destroy = function_body(views, "void UltralightBackend::DestroyView(")
assert "if (!DispatchDeferred([this, id] {" in destroy and "ErasePublishedView(id);" in destroy, (
    "DestroyView must erase the published view when the destroy task is refused"
)

inputs = read(SRC / "UltralightBackendInput.cpp")
gamepad = function_body(inputs, "bool UltralightBackend::SetNativeGamepad(")
assert re.search(r"if \(DispatchDeferred\(.*?\)\) return true;\s*CancelNativeGamepad\(id\);", gamepad, re.S), (
    "SetNativeGamepad must cancel the owner it claimed when the prepare task is refused"
)

documents = read(SRC / "UltralightDocumentCallbacks.cpp")
flush = function_body(documents, "void UltralightDocumentCallbacks::FlushPendingCalls(")
assert re.search(
    r"if \(!host_\.DispatchDocument\(.*?\)\) \{\s*record\.pendingCalls\.clear\(\);\s*record\.pendingCallBytes = 0;\s*"
    r"record\.replayingPendingCalls = false;",
    flush,
    re.S,
), "FlushPendingCalls must clear replay state when the next flush is refused"

inspector = read(SRC / "UltralightInspector.cpp")
visibility = function_body(inspector, "void UltralightInspector::SetVisibility(")
assert "std::make_shared<OnceCompletion<bool>>(" in visibility and (
    "[this, owner](bool state) { host_.NotifyInspectorState(owner, state); }" in visibility
), "SetVisibility must notify the inspector state through a OnceCompletion"
assert visibility.count("done->Complete(") == 4, (
    "every SetVisibility path must complete the inspector state notification exactly once through done"
)

print("dispatch admission guard passed")
