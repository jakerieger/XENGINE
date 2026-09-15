import os
import platform
import sys
import subprocess
from enum import Enum
from pathlib import Path


class BuildConfig(str, Enum):
    Debug = "Debug"
    Release = "Release"

    def __str__(self) -> str:
        return self.value


def get_tools_path(config: BuildConfig) -> Path:
    cwd_path = Path.cwd()
    tools_path = cwd_path.joinpath("build", config.value, "bin", "Tools")

    if not (tools_path.exists()):
        raise NotADirectoryError(
            "Tools directory does not exist for config '{}' (missing: {})".format(config.value, tools_path))

    return tools_path


def get_paktool_path(config: BuildConfig) -> Path:
    tools_path = get_tools_path(config)
    paktool_path = tools_path.joinpath("PAKTool")

    if platform.system() == "Windows":
        paktool_path = tools_path.joinpath("PAKTool.exe")

    if not (paktool_path.exists()):
        raise FileNotFoundError("PAKTool executable not found (missing: {})".format(paktool_path))

    return paktool_path


def get_content_dir() -> Path:
    cwd_path = Path.cwd()
    content_dir = cwd_path.joinpath("Code", "XenPong", "Content")

    if not (content_dir.exists()):
        raise NotADirectoryError("Content directory not found (missing: {})".format(content_dir))

    return content_dir


def get_pak_filename(config: BuildConfig) -> Path:
    cwd_path = Path.cwd()
    pak_filename = cwd_path.joinpath("build", config.value, "bin", "XenPong", "Data1.xpak")

    if not (pak_filename.parent.exists()):
        raise NotADirectoryError("XenPong build output directory not found (missing: {})".format(pak_filename.parent))

    return pak_filename


def pack_demo_content():
    build_config = BuildConfig(sys.argv[1]) if len(sys.argv) > 1 else BuildConfig.Debug

    paktool = get_paktool_path(build_config)
    pak_filename = get_pak_filename(build_config)
    content_dir = get_content_dir()

    result = subprocess.run([str(paktool), "pack", str(content_dir), "-o", str(pak_filename)])

    if result.returncode != 0:
        print(f"fatal: {str(paktool.name)} failed with code {result.returncode}", file=sys.stderr)
        sys.exit(result.returncode)

    if build_config == BuildConfig.Release:
        # delete the .xmeta file PAKTool creates for dev purposes
        xmeta_file = pak_filename.with_suffix(".xmeta")
        if xmeta_file.exists():
            xmeta_file.unlink()


if __name__ == "__main__":
    pack_demo_content()
