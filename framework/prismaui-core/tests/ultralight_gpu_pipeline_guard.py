import re
from pathlib import Path

context = Path("src/GPU/FalloutD3DContext.cpp").read_text(encoding="utf-8")
layouts = Path("src/GPU/UltralightPipelineLayout.h").read_text(encoding="utf-8")
uniforms = Path("src/GPU/UltralightGPUUniforms.h").read_text(encoding="utf-8")
sdk = Path("external/ultralight-1.4.0/include/Ultralight/platform/GPUDriver.h").read_text(encoding="utf-8")


def section(source, start, end):
    begin = source.find(start)
    stop = source.find(end, begin) if begin != -1 else -1
    if begin == -1 or stop == -1:
        raise AssertionError(f"source section missing: {start!r} .. {end!r}")
    return source[begin:stop]


def fields(block):
    return dict(re.findall(r"\.(\w+) = (\w+);", block))


def expect(actual, expected, label):
    for name, value in expected.items():
        if actual.get(name) != value:
            raise AssertionError(f"{label}: {name} must be {value}, found {actual.get(name)}")


ensure = section(context, "bool FalloutD3DContext::EnsureStates()", "bool FalloutD3DContext::BindBlend(")
expect(
    fields(section(ensure, "D3D11_BLEND_DESC blend{};", "CreateBlendState(&blend, blendEnabled_")),
    {
        "BlendEnable": "TRUE",
        "SrcBlend": "D3D11_BLEND_ONE",
        "DestBlend": "D3D11_BLEND_INV_SRC_ALPHA",
        "BlendOp": "D3D11_BLEND_OP_ADD",
        "SrcBlendAlpha": "D3D11_BLEND_INV_DEST_ALPHA",
        "DestBlendAlpha": "D3D11_BLEND_ONE",
        "BlendOpAlpha": "D3D11_BLEND_OP_ADD",
        "RenderTargetWriteMask": "D3D11_COLOR_WRITE_ENABLE_ALL",
    },
    "enabled blend state",
)
expect(
    fields(section(ensure, "blend.RenderTarget[0].BlendEnable = FALSE;", "CreateBlendState(&blend, blendDisabled_")),
    {
        "BlendEnable": "FALSE",
        "SrcBlend": "D3D11_BLEND_ONE",
        "DestBlend": "D3D11_BLEND_ZERO",
        "SrcBlendAlpha": "D3D11_BLEND_ONE",
        "DestBlendAlpha": "D3D11_BLEND_ZERO",
    },
    "disabled blend state",
)
plain = fields(section(ensure, "D3D11_RASTERIZER_DESC raster{};", "CreateRasterizerState(&raster, rasterizer_"))
scissored = fields(section(ensure, "raster.ScissorEnable = TRUE;", "CreateRasterizerState(&raster, scissoredRasterizer_"))
expect(plain, {"DepthClipEnable": "FALSE", "ScissorEnable": "FALSE"}, "plain rasterizer state")
expect({**plain, **scissored}, {"DepthClipEnable": "FALSE", "ScissorEnable": "TRUE"}, "scissored rasterizer state")

SDK_SIZES = {"float": 4, "unsigned char": 1}
FORMAT_SIZES = {"R32G32_FLOAT": 8, "R8G8B8A8_UINT": 4, "R32G32B32A32_FLOAT": 16}
PATH_VERTEX = [("float", "pos", "2"), ("unsigned char", "color", "4"), ("float", "obj", "2")]
FILL_VERTEX = [
    ("float", "pos", "2"),
    ("unsigned char", "color", "4"),
    ("float", "tex", "2"),
    ("float", "obj", "2"),
] + [("float", f"data{index}", "4") for index in range(7)]
PATH_LAYOUT = [("POSITION", 0, "R32G32_FLOAT"), ("COLOR", 0, "R8G8B8A8_UINT"), ("TEXCOORD", 0, "R32G32_FLOAT")]
FILL_LAYOUT = [
    ("POSITION", 0, "R32G32_FLOAT"),
    ("COLOR", 0, "R8G8B8A8_UINT"),
    ("TEXCOORD", 0, "R32G32_FLOAT"),
    ("TEXCOORD", 1, "R32G32_FLOAT"),
] + [("COLOR", index, "R32G32B32A32_FLOAT") for index in range(1, 8)]

for struct, size, layout_name, members, elements in (
    ("Vertex_2f_4ub_2f", 20, "kFillPathLayout", PATH_VERTEX, PATH_LAYOUT),
    ("Vertex_2f_4ub_2f_2f_28f", 140, "kFillLayout", FILL_VERTEX, FILL_LAYOUT),
):
    body = section(sdk, f"struct {struct} {{", "};")
    actual_members = re.findall(r"^\s*([\w ]+?)\s+(\w+)\[(\d+)\];", body, re.M)
    if actual_members != members:
        raise AssertionError(f"{struct} member order or type changed: {actual_members}")
    if sum(SDK_SIZES[kind] * int(count) for kind, _, count in actual_members) != size:
        raise AssertionError(f"{struct} no longer totals {size} bytes")
    if f"sizeof(ultralight::{struct}) == {size}" not in uniforms:
        raise AssertionError(f"UltralightGPUUniforms.h must static_assert sizeof({struct}) == {size}")
    layout_body = section(layouts, f"D3D11_INPUT_ELEMENT_DESC {layout_name}[] = {{", "};")
    actual_elements = [
        (semantic, int(index), fmt)
        for semantic, index, fmt in re.findall(r'\{"(\w+)", (\d+), DXGI_FORMAT_(\w+),', layout_body)
    ]
    if actual_elements != elements:
        raise AssertionError(f"{layout_name} input elements changed: {actual_elements}")
    if sum(FORMAT_SIZES[fmt] for _, _, fmt in actual_elements) != size:
        raise AssertionError(f"{layout_name} byte stride must total {size}")

print("ultralight gpu pipeline guard passed")
