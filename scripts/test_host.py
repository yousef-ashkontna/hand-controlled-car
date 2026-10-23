"""Compile and run hardware-independent regression tests (requires GCC or Clang)."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang")
if not compiler:
    raise SystemExit("Install GCC or Clang, or set CC to a C compiler")
with tempfile.TemporaryDirectory(prefix="car-tests-") as folder:
    for name, sources, includes in (
        ("control", ["tests/test_control.c", "src/common/car_control.c"], ["include"]),
        ("pairing", ["tests/test_pairing.c"], ["include"]),
        ("mpu6050", ["tests/test_mpu6050.c"], ["tests/stubs", "components/mpu6050"]),
    ):
        binary = str(Path(folder) / (name + (".exe" if os.name == "nt" else "")))
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror"]
        command += ["-I" + str(root / directory) for directory in includes]
        command += [str(root / source) for source in sources] + ["-o", binary]
        subprocess.run(command, check=True)
        subprocess.run([binary], check=True)
