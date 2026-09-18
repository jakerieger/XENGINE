"""
Compiles engine shaders located in Code/Shaders and outputs them to Engine/Shaders.
"""

import subprocess
import sys
from enum import StrEnum
from pathlib import Path


class ShaderType(StrEnum):
    GRAPHICS = "Graphics"
    COMPUTE = "Compute"


GRAPHICS_ENTRYPOINTS = ("VSMain", "PSMain")


def detect_shader_type(shader_file: Path) -> ShaderType:
    source = shader_file.read_text(encoding="utf-8", errors="replace")
    if any(entry in source for entry in GRAPHICS_ENTRYPOINTS):
        return ShaderType.GRAPHICS
    return ShaderType.COMPUTE


class ShaderCompiler:
    PROFILES = {
        "vertex": "vs_6_0",
        "pixel": "ps_6_0",
        "compute": "cs_6_0",
    }
    ENTRYPOINTS = {
        "vertex": "VSMain",
        "pixel": "PSMain",
        "compute": "CSMain",
    }
    SUFFIXES = {
        "vertex": ".vs",
        "pixel": ".ps",
        "compute": ".cs",
    }
    STAGES = {
        ShaderType.GRAPHICS: ("vertex", "pixel"),
        ShaderType.COMPUTE: ("compute",),
    }

    def __init__(self, root: Path | None = None):
        # Scripts/ lives one level below the repo root.
        root = root or Path(__file__).resolve().parent.parent
        self.shader_sources = root / "Code" / "Shaders"
        self.shader_output = root / "Engine" / "Shaders"

    def _invoke_dxc(self, shader_file: Path, stage: str) -> None:
        output = self.shader_output / ("xen.shader." + shader_file.stem.lower() + self.SUFFIXES[stage])
        cmd = [
            "dxc.exe",
            "-T", self.PROFILES[stage],
            "-E", self.ENTRYPOINTS[stage],
            "-Fo", str(output),
            str(shader_file),
        ]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True)
        except FileNotFoundError:
            raise RuntimeError(
                "dxc.exe was not found on PATH. It ships in the Windows SDK "
                r"(e.g. C:\Program Files (x86)\Windows Kits\10\bin\<ver>\x64)."
            ) from None

        if result.returncode != 0:
            sys.stderr.write(result.stdout)
            sys.stderr.write(result.stderr)
            raise ChildProcessError(
                f"Failed to compile {stage} shader: {shader_file.name}"
            )

    def _compile_shader(self, shader_file: Path, shader_type: ShaderType) -> None:
        for stage in self.STAGES[shader_type]:
            self._invoke_dxc(shader_file, stage)
        print(f"✔ Compiled '{shader_file.stem}' ({shader_type})")

    def compile_shaders(self) -> None:
        print("---[ compile_engine_shaders.py ]---")
        print(f"[Source] => {str(self.shader_sources)}")
        print(f"[Output] => {str(self.shader_output)}")

        if not self.shader_sources.is_dir():
            raise FileNotFoundError(f"No shader source directory: {self.shader_sources}")

        self.shader_output.mkdir(parents=True, exist_ok=True)

        for item in sorted(self.shader_sources.iterdir()):
            if item.is_file() and item.suffix.lower() == ".hlsl":
                self._compile_shader(item, detect_shader_type(item))


def main() -> int:
    try:
        ShaderCompiler().compile_shaders()
    except (ChildProcessError, FileNotFoundError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
