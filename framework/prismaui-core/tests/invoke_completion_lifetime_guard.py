from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CALLBACKS = (ROOT / "src/PrismaUI/WebRuntimeCallbacks.cpp").read_text(encoding="utf-8")
STATE = (ROOT / "src/PrismaUI/WebRuntime.cpp").read_text(encoding="utf-8")


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


invoke = function_body(CALLBACKS, "void Invoke(ViewId view")
registration = invoke.find("st.resultByToken[token] = ResultRegistration{0, std::move(onResult)};")
evaluate = invoke.find("st.backend->EvaluateScript(view, script")
post_validity = invoke.find("if (hasCb && !IsValid(view))", evaluate)
post_complete = invoke.find(
    'ResultTramp(PrismaUI::InvokeResult::Error("Invoke target view is not live"), token);',
    post_validity,
)
assert -1 not in (registration, evaluate, post_validity, post_complete), (
    "Invoke must keep one-shot result registration unscoped and close the dispatch-vs-destroy race"
)
assert registration < evaluate < post_validity < post_complete, (
    "Invoke completion lifetime checks must bracket backend dispatch in the required order"
)
assert "ResultRegistration{view, std::move(onResult)}" not in invoke, (
    "Invoke result callbacks must not be view-scoped or ForgetViewShell can delete them before completion"
)

forget = function_body(STATE, "void Internal::ForgetViewShell(ViewId view)")
assert "st.resultByToken.clear()" not in forget, (
    "view teardown must not globally discard pending one-shot Invoke completions"
)
assert "if (it->second.view == view)" in forget, (
    "view teardown should continue removing genuinely view-scoped callback registrations"
)

result_tramp = function_body(STATE, "void Internal::ResultTramp(std::string json, uint64_t token)")
erase = result_tramp.find("st.resultByToken.erase(it);")
deliver = result_tramp.find("if (fn) fn(std::move(json));")
assert -1 not in (erase, deliver) and erase < deliver, (
    "Invoke result delivery must erase its token before invoking the consumer so duplicate completions are ignored"
)

backend = (ROOT / "src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
owner = function_body(backend, "void UltralightBackend::OwnerMain(")
assert owner.count("dispatchQueue_.DrainAbandoned();") == 2, (
    "OwnerMain must drain abandoned tasks on both the init-failure and loop exits"
)
script_input = (ROOT / "src/PrismaUI/UltralightBackendInput.cpp").read_text(encoding="utf-8")
for signature in ("void UltralightBackend::EvaluateScript(", "bool UltralightBackend::EvaluateScriptDeferred("):
    body = function_body(script_input, signature)
    assert "std::make_shared<ScriptCompletion>(std::move(callback), ScriptStopped())" in body and "callback(" not in body, (
        f"{signature} must route its result through ScriptCompletion so dropped tasks still complete"
    )
assert "using ScriptCompletion = OnceCompletion<std::string>;" in script_input, (
    "ScriptCompletion must stay the OnceCompletion<std::string> alias that carries the stopped-backend error"
)


print("invoke completion lifetime guard passed")
