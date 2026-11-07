from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    executable = Path(directory) / "reads.exe"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20",
                    "-I" + str(ROOT / "tests"),
                    str(ROOT / "tests/modelpreview_read_failure_tests.cpp"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print("preview read failure recovery tests passed")
