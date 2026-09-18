"""
Creates a new Xen game project with a barebones implementation as a starting point.

Usage: create_project.py <name> <directory>
"""

import sys
import argparse
from pathlib import Path


class ProjectArgs:
    def __init__(self, name: str, directory: str):
        self.name = name
        self.directory = Path(directory)

    def __str__(self):
        return f"Project name: {self.name}, directory: {self.directory}"


def create_project_config(project_dir: Path):
    config_dir = project_dir / "Config"
    config_dir.mkdir()

    if not config_dir.exists():
        raise NotADirectoryError(f"Failed to create Config directory: {config_dir}")

    engine_config_file = config_dir / "EngineConfig.ini"
    with open(engine_config_file, "w") as f:
        content = """[Engine]
StartupScene = scenes/default.xscene
WindowMode = windowed
ResolutionX = 1280
ResolutionY = 720
        """
        f.write(content)

    audio_config_file = config_dir / "AudioConfig.ini"
    with open(audio_config_file, "w") as f:
        content = """[Audio]
MasterVolume = 1
MusicVolume = 1
EffectsVolume = 1
VoiceVolume = 1
UIVolume = 1
        """
        f.write(content)

    input_config_file = config_dir / "InputConfig.ini"
    with open(input_config_file, "w") as f:
        content = """[Actions]
        """
        f.write(content)


def create_content_dir(project_dir: Path):
    content_dir = project_dir / "Content"
    content_dir.mkdir()

    scenes_dir = content_dir / "scenes"
    scenes_dir.mkdir()

    default_scene = scenes_dir / "default.xscene"
    with open(default_scene, "w") as f:
        content = """{
  "Version": 2,
  "Name": "Default",
  "Actors": []
}"""
        f.write(content)


def create_project(project_args: ProjectArgs):
    project_dir = project_args.directory
    project_dir.mkdir(exist_ok=True, parents=True)

    if not project_dir.exists():
        raise NotADirectoryError(f"Failed to create project directory: {project_dir}")

    # Create config directory and files
    create_project_config(project_dir)

    # Create content directory
    create_content_dir(project_dir)

    # Create runtime source files

    # Create CMake config


def main():
    parser = argparse.ArgumentParser(
        description="Creates a new Xen game project with a barebones implementation as a starting point.")
    parser.add_argument("name", type=str, help="The name of the project.")
    parser.add_argument("directory", type=str, help="The directory where the project will be created.")
    args = parser.parse_args()

    project_args = ProjectArgs(args.name, args.directory)

    create_project(project_args)


if __name__ == "__main__":
    main()
    sys.exit(0)
