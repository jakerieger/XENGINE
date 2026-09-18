"""
XEN bootstrapper
"""

import sys
import subprocess
from pathlib import Path


def main():
    # compile engine shaders
    root = Path(__file__).resolve().parent

    compile_shaders_script = root / "Scripts" / "compile_engine_shaders.py"
    result = subprocess.run([sys.executable, str(compile_shaders_script)])

    if result.returncode != 0:
        print(result.stderr)
        return result.returncode

    print(
        "==============================================================================================================")

    gen_meshes_script = root / "Scripts" / "generate_primitive_meshes.py"
    result = subprocess.run([sys.executable, str(gen_meshes_script)])

    if result.returncode != 0:
        print(result.stderr)
        return result.returncode

    return 0


if __name__ == "__main__":
    sys.exit(main())
