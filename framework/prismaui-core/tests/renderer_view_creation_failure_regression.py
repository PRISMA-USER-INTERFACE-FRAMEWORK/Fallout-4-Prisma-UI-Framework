from pathlib import Path

SOURCE = Path("src/PrismaUI/UltralightBackendViews.cpp").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing {signature}")
    opening = source.find("{", start)
    if opening < 0:
        raise AssertionError(f"missing body for {signature}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed body for {signature}")


def renderer_null_completion(callback):
    record = {"domReady": callback}
    renderer_view = None

    if not renderer_view:
        failed_callback = record.pop("domReady")
        if failed_callback:
            failed_callback(0)


def main() -> None:
    body = function_body(SOURCE, "PrismaUI::Web::ViewId UltralightBackend::CreateView(")
    create = body.index("record->view = renderer_->CreateView")
    failure = body.index("if (!record->view)", create)
    failure_end = body.index("return;", failure)
    failure_branch = body[failure:failure_end]

    assert "record->domReady = std::move(domReady);" in body
    assert "FailViewCreation(id, std::move(record->domReady)," in failure_branch, (
        "a null renderer view must complete the callback owned by the record"
    )
    assert "FailViewCreation(id, std::move(domReady)," not in failure_branch, (
        "the moved-from local callback cannot complete renderer failure"
    )
    manager = Path("src/PrismaUI/UltralightViewManager.cpp").read_text(encoding="utf-8")
    fail = function_body(manager, "bool UltralightViewManager::FailCreation(")
    assert "onReady(0)" in fail, "FailCreation must complete the failed callback"
    completions = []
    renderer_null_completion(completions.append)
    assert completions == [0], "renderer failure must complete the callback exactly once"
    print("renderer-null callback fault injection passed")


if __name__ == "__main__":
    main()
