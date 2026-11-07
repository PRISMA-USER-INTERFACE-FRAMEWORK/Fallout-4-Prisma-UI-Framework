from pathlib import Path


def function_body(source: str, marker: str) -> str:
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated function: {marker}")


def check_reload_resets_document_ready(documents: str) -> None:
    begin = function_body(documents, "void UltralightDocumentCallbacks::OnBeginLoading(")
    assert "record->documentDomReady = false;" in begin, "a trusted reload must clear per-document readiness"


def check_calls_gate_on_document_ready(inputs: str) -> None:
    call = function_body(inputs, "void UltralightBackend::CallFunction(")
    assert "!record.documentDomReady" in call, "pre-DOM call queue must gate on the per-document flag"
    assert "domReadyFired" not in call, "the one-shot consumer flag must not gate per-document calls"


def check_dom_ready_hook_precedes_one_shot(documents: str) -> None:
    dom = function_body(documents, "void UltralightDocumentCallbacks::OnDomReady(")
    assert "if (record->domReadyFired) return;" not in dom, "a repeat DOM ready must not skip per-document init"
    assert "if (!record->domReadyFired)" in dom, "the consumer callback must stay one-shot"
    one_shot = function_body(dom, "if (!record->domReadyFired)")
    assert "record->domReadyFired = true;" in one_shot, "the one-shot section must mark the consumer callback delivered"
    assert dom.index("host_.OnTrustedDocumentReady(id);") < dom.index("if (!record->domReadyFired)"), (
        "per-document init must run before the one-shot consumer callback"
    )


def check_bridge_is_per_document(views: str, state: str, backend: str) -> None:
    registration = views.split("st.domReadyByToken[token] = DomReadyRegistration{", 1)[1].split(
        "ViewId id = st.backend->CreateView(", 1)[0]
    assert "InjectBridge" not in registration, "window.prisma must not depend on the one-shot registration"
    assert "onDomReady(id)" in registration, "the consumer onDomReady must stay in the one-shot registration"
    sink = function_body(state, "void DocumentReadySink(")
    assert "PapyrusBridge::InjectBridge(view);" in sink, "the per-document sink must install window.prisma"
    for helper in ("Invoke(view, apiInit", "Invoke(view, texInit", "__prismaUI_modelPreview", "__prismaUI_textureOverlay"):
        assert helper in sink, f"the per-document sink must reinstall {helper}"
    assert "SetDocumentReadyCallback(&DocumentReadySink);" in state, "the runtime must register the per-document sink"
    hook = function_body(backend, "void UltralightBackend::OnTrustedDocumentReady(")
    assert "documentReadyCallback_" in hook and "record->isInspector" in hook and "callback(id);" in hook, (
        "the backend must call the per-document sink for trusted non-inspector documents"
    )


def run() -> None:
    documents = Path("src/PrismaUI/UltralightDocumentCallbacks.cpp").read_text(encoding="utf-8")
    inputs = Path("src/PrismaUI/UltralightBackendInput.cpp").read_text(encoding="utf-8")
    views = Path("src/PrismaUI/WebRuntimeViews.cpp").read_text(encoding="utf-8")
    state = Path("src/PrismaUI/WebRuntimeState.cpp").read_text(encoding="utf-8")
    backend = Path("src/PrismaUI/UltralightBackendCallbacks.cpp").read_text(encoding="utf-8")
    check_reload_resets_document_ready(documents)
    check_calls_gate_on_document_ready(inputs)
    check_dom_ready_hook_precedes_one_shot(documents)
    check_bridge_is_per_document(views, state, backend)


if __name__ == "__main__":
    run()
    print("document reload init contract passed")
