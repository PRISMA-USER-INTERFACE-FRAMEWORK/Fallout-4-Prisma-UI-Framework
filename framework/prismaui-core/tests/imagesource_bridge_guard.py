from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
BRIDGE = "src/PrismaUI/UltralightImageSources.cpp"
BACKEND = (SRC / "PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
BACKEND_HEADER = (SRC / "PrismaUI/UltralightBackend.h").read_text(encoding="utf-8")
C_API = re.compile(r"\bul(?:Create|Destroy)ImageSource\w*|\bulImageSource\w*")
CPP_API = re.compile(r"ImageSource::|ImageSourceProvider")


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start == -1:
        fail(f"missing {signature}")
    end = source.find("\n    }\n", start)
    if end == -1:
        fail(f"unterminated {signature}")
    return source[start:end]


for path in sorted(SRC.rglob("*")):
    if path.suffix not in {".cpp", ".h", ".inl"} or not path.is_file():
        continue
    rel = path.relative_to(ROOT).as_posix()
    text = path.read_text(encoding="utf-8", errors="ignore")
    if C_API.search(text):
        fail(f"the ImageSource C API must not be used: {rel}")
    if rel != BRIDGE and CPP_API.search(text):
        fail(f"ImageSource:: and ImageSourceProvider may appear only in {BRIDGE}: {rel}")
if not CPP_API.search((ROOT / BRIDGE).read_text(encoding="utf-8")):
    fail(f"{BRIDGE} must own the ImageSource calls")

owner = BACKEND.find("void UltralightBackend::OwnerMain(")
if owner == -1:
    fail("UltralightBackend::OwnerMain is missing")
set_registry = BACKEND.find("gpuDriver_->SetExternalTextureRegistry(&externalTextures_);", owner)
create_renderer = BACKEND.find("renderer_ = ultralight::Renderer::Create();", owner)
if not -1 < set_registry < create_renderer:
    fail("SetExternalTextureRegistry(&externalTextures_) must run before Renderer::Create")

accelerated_final = BACKEND.find("accelerated_ = accelerated_ && gpuDriver_->InitializePipeline();", owner)
ready_set = BACKEND.find("ready_ = success;", owner)
if not -1 < accelerated_final < ready_set:
    fail("OwnerMain must set ready_ after accelerated_ is final")

teardown = BACKEND.find('currentOp_.store("teardown", std::memory_order_release);')
if teardown == -1:
    fail("owner teardown is missing")
clear_records = BACKEND.find("viewManager_.ClearRecords();", teardown)
remove_all = BACKEND.find("imageSources_.RemoveAll();", teardown)
renderer_reset = BACKEND.find("renderer_ = nullptr;", teardown)
if not -1 < clear_records < remove_all < renderer_reset:
    fail("imageSources_.RemoveAll() must run between ClearRecords() and renderer_ = nullptr")
driver_reset = BACKEND.find("gpuDriver_.reset();", teardown)
registry_clear = BACKEND.find("externalTextures_.Clear();", teardown)
if not -1 < driver_reset < registry_clear:
    fail("externalTextures_.Clear() must run after the teardown gpuDriver_.reset()")

registry_declaration = BACKEND_HEADER.find("PrismaUI::GPU::ExternalTextureRegistry externalTextures_;")
driver_declaration = BACKEND_HEADER.find("std::unique_ptr<PrismaUI::GPU::FalloutGPUDriver> gpuDriver_;")
if not -1 < registry_declaration < driver_declaration:
    fail("externalTextures_ must be declared before gpuDriver_ in UltralightBackend.h")

external_ready = function_body(BACKEND, "bool UltralightBackend::ExternalImagesReady() const {")
if "std::lock_guard lock(stateMutex_);" not in external_ready or "return ready_ && accelerated_;" not in external_ready:
    fail("ExternalImagesReady must return ready_ && accelerated_ under stateMutex_")
for signature in (
    "std::string UltralightBackend::ReserveExternalImage(",
    "void UltralightBackend::PublishExternalImage(",
    "void UltralightBackend::RetractExternalImage(",
    "void UltralightBackend::ReleaseExternalImage(",
):
    body = function_body(BACKEND, signature)
    if not body[body.index("{") + 1 :].lstrip().startswith("if (!ExternalImagesReady()) return"):
        fail(f"{signature} must return unless ExternalImagesReady() comes first")

publish = function_body(BACKEND, "void UltralightBackend::PublishExternalImage(")
reserved = publish.find("if (!gpuDriver_ || !imageSources_.IsReserved(key)) return;")
next_id = publish.find("gpuDriver_->NextTextureId()")
if reserved == -1 or next_id == -1 or reserved > next_id:
    fail("PublishExternalImage must return before allocating an id unless the key is reserved")

release = function_body(BACKEND, "void UltralightBackend::ReleaseExternalImage(")
release_order = [
    release.find("imageSources_.Release(key);"),
    release.find("externalTextures_.Unbind(found->second);"),
    release.find("externalTextureIds_.erase(found);"),
    release.find("rejectedExternalKeys_.erase(key);"),
]
if -1 in release_order or release_order != sorted(release_order):
    fail("ReleaseExternalImage must release the token, unbind the texture, then forget the key")

if 'L"GPU", L"bAcceleratedRendering", 1,' not in BACKEND:
    fail("UltralightBackend.cpp must attempt accelerated rendering by default behind bAcceleratedRendering")
if "gpuContext_.Initialize(device_.Get(), immediateContext_.Get(), true)" not in BACKEND:
    fail("UltralightBackend.cpp must pass the controlled proxy allowance to the accelerated context")

print("imagesource bridge guard passed")
