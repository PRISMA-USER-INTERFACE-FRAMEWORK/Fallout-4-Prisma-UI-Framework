from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


for relative in (
    "system/boot.html",
    "system/dock.html",
    "system/dockManifest.js",
    "logo.png",
    "dockimage.webp",
):
    shipped = ROOT / "assets/views" / relative
    tested = ROOT / "shell/public" / relative
    assert shipped.read_bytes() == tested.read_bytes(), f"shipping asset drift: {relative}"

print("system asset parity passed")
