from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/PrismaUI/WebRuntimeInput.cpp").read_text(encoding="utf-8")
start = source.index("    void QueueActivationRestore(")
end = source.index("    void MapClientPointToBrowser(", start)
with tempfile.TemporaryDirectory() as directory:
    (Path(directory) / "activation_under_test.inc").write_text(source[start:end], encoding="utf-8")
    executable = Path(directory) / "activation.exe"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-I" + str(ROOT / "src"),
                    "-I" + directory, str(ROOT / "tests/activation_restore_tests.cpp"),
                    str(ROOT / "src/PrismaUI/InputRegionPolicy.cpp"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print("activation restore behavior tests passed")
