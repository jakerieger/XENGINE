"""
XEN bootstrapper
"""

import sys
import subprocess
from pathlib import Path


def pull_git_submodules() -> bool:
    imgui_path = Path(__file__).parent / "Code" / "Vendor" / "imgui"
    if not any(imgui_path.iterdir()):
        print("ImGui submodule not found. Pulling it...")

        # Clone submodule
        result = subprocess.run(["git", "submodule", "update", "--init", "--recursive"], capture_output=True)
        if result.returncode != 0:
            print(result.stderr)
            return False

        # Checkout 'docking' branch (used for the editor)
        result = subprocess.run(["git", "checkout", "docking"], capture_output=True, cwd=imgui_path)
        if result.returncode != 0:
            print(result.stderr)
            return False

        # Sparse checkout only necessary files (CMakeLists.txt glob's imgui sources so if the entire repo is checked
        # out, it'll try to compile code for unsupported platforms, i.e. Android)
        result = subprocess.run(["git", "sparse-checkout", "init", "--no-cone"], capture_output=True, cwd=imgui_path)
        if result.returncode != 0:
            print(result.stderr)
            return False
        result = subprocess.run(
            ["git", "sparse-checkout", "set", "'/*.cpp'", "'/*.h'", "'/backends/imgui_impl_dx12.*'",
             "'/backends/imgui_impl_win32.*'"],
            capture_output=True, cwd=imgui_path)
        if result.returncode != 0:
            print(result.stderr)
            return False

    return True


def main():
    # Get submodules if missing
    if not pull_git_submodules():
        print("Failed to pull git submodules")
        return 1

    # compile engine shaders
    root = Path(__file__).resolve().parent

    compile_shaders_script = root / "Scripts" / "compile_engine_shaders.py"
    result = subprocess.run([sys.executable, str(compile_shaders_script)])

    if result.returncode != 0:
        print(result.stderr)
        return result.returncode

    # generate primitive meshes for demos
    gen_meshes_script = root / "Scripts" / "generate_primitive_meshes.py"
    result = subprocess.run([sys.executable, str(gen_meshes_script)])

    if result.returncode != 0:
        print(result.stderr)
        return result.returncode

    return 0


if __name__ == "__main__":
    sys.exit(main())
