from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
renderer = (root / "assets" / "views" / "prisma_controller_glyphs.js").read_text(encoding="utf-8")
styles = (root / "assets" / "views" / "prisma_controller_glyphs.css").read_text(encoding="utf-8")

assert 'width="22" height="22"' in renderer
assert "width:22px;height:22px;object-fit:contain;" in styles

subprocess.run(["node", str(root / "tests" / "controller_glyph_renderer_test.js")], check=True)

print("controller glyph intrinsic-size guard passed")
