from pathlib import Path

SOURCE = Path("src/PrismaUI/UltralightInspector.cpp").read_text(encoding="utf-8")


def require(source: str, needle: str, message: str) -> None:
    if needle not in source:
        raise AssertionError(message)


def forbid(source: str, needle: str, message: str) -> None:
    if needle in source:
        raise AssertionError(message)


require(
    SOURCE,
    "constexpr uint32_t kInspectorMinimumDimension = 32;",
    "inspector geometry must keep the normal 32px minimum",
)

resize_viewport_start = SOURCE.find("void UltralightInspector::SetViewport(")
resize_viewport_end = SOURCE.find("void UltralightInspector::SetVisibility(", resize_viewport_start)
if resize_viewport_start < 0 or resize_viewport_end < 0:
    raise AssertionError("could not locate viewport Resize")
resize_viewport = SOURCE[resize_viewport_start:resize_viewport_end]
for needle in (
    "const uint32_t minimumWidth = std::min(kInspectorMinimumDimension, width);",
    "const uint32_t minimumHeight = std::min(kInspectorMinimumDimension, height);",
    "state.width = std::clamp(state.width, minimumWidth, width);",
    "state.height = std::clamp(state.height, minimumHeight, height);",
):
    require(resize_viewport, needle, f"viewport Resize lost inspector minimum restoration: {needle}")
for needle in (
    "state.width = std::min(state.width, width_);",
    "state.height = std::min(state.height, height_);",
):
    forbid(resize_viewport, needle, f"viewport Resize restored shrink-only inspector geometry: {needle}")

bounds_start = SOURCE.find("void UltralightInspector::SetBounds(")
bounds_end = SOURCE.find("void UltralightInspector::BeginMove(", bounds_start)
if bounds_start < 0 or bounds_end < 0:
    raise AssertionError("could not locate SetInspectorBounds")
bounds = SOURCE[bounds_start:bounds_end]

for needle in (
    "const uint32_t minimumWidth = std::min(kInspectorMinimumDimension, viewportWidth);",
    "const uint32_t minimumHeight = std::min(kInspectorMinimumDimension, viewportHeight);",
    "const uint32_t nextWidth = std::min(width, viewportWidth);",
    "const uint32_t nextHeight = std::min(height, viewportHeight);",
    "if (nextWidth < minimumWidth || nextHeight < minimumHeight) return;",
    "state.width = nextWidth;",
    "state.height = nextHeight;",
):
    require(bounds, needle, f"SetInspectorBounds lost viewport-safe geometry rule: {needle}")

validation = bounds.find("if (nextWidth < minimumWidth || nextHeight < minimumHeight) return;")
width_mutation = bounds.find("state.width = nextWidth;")
height_mutation = bounds.find("state.height = nextHeight;")
if not validation < width_mutation < height_mutation:
    raise AssertionError("SetInspectorBounds must validate candidate dimensions before mutating stored geometry")

forbidden_bounds = (
    "state.width = std::min(width, viewportWidth);",
    "state.height = std::min(height, viewportHeight);",
    "state.width < 32 || state.height < 32",
)
for needle in forbidden_bounds:
    forbid(bounds, needle, f"SetInspectorBounds restored stale pre-validation mutation: {needle}")

update_start = SOURCE.find("UltralightInspector::UpdateGesture(")
update_end = SOURCE.find("UltralightInspector::GestureTargetAt(", update_start)
resize_start = SOURCE.find("void UltralightInspector::ResizeBy(")
resize_end = SOURCE.find("bool UltralightInspector::GetFrame(", resize_start)
if min(update_start, update_end, resize_start, resize_end) < 0:
    raise AssertionError("could not locate inspector resize paths")

for name, body in (
    ("UpdateGesture", SOURCE[update_start:update_end]),
    ("ResizeBy", SOURCE[resize_start:resize_end]),
):
    for needle in (
        "const int minimumWidth = static_cast<int>(std::min(kInspectorMinimumDimension, viewportWidth));",
        "const int minimumHeight = static_cast<int>(std::min(kInspectorMinimumDimension, viewportHeight));",
        "static_cast<int>(state.width) + dx, minimumWidth, static_cast<int>(viewportWidth)",
        "static_cast<int>(state.height) + dy, minimumHeight, static_cast<int>(viewportHeight)",
    ):
        require(body, needle, f"{name} lost viewport-safe inspector clamp: {needle}")

for needle in (
    "static_cast<int>(state.width) + dx, 32, static_cast<int>(viewportWidth)",
    "static_cast<int>(state.height) + dy, 32, static_cast<int>(viewportHeight)",
):
    forbid(SOURCE, needle, f"fixed inspector clamp can invert below 32px viewport: {needle}")

if SOURCE.count("std::min(kInspectorMinimumDimension, viewportWidth)") < 3:
    raise AssertionError("all inspector width boundaries must derive their minimum from the viewport")
if SOURCE.count("std::min(kInspectorMinimumDimension, viewportHeight)") < 3:
    raise AssertionError("all inspector height boundaries must derive their minimum from the viewport")

print("inspector geometry guard passed")
