"""
Splits a glTF-style packed metallic-roughness texture (G = roughness,
B = metallic) into two separate single-channel maps - the format
Code/Shaders/Include/MaterialBindings.hlsli's XEN_ROUGHNESS_TEX_REGISTER and
XEN_METALLIC_TEX_REGISTER expect (each sampled from its own texture's .r -
see PBR.hlsl), now that the engine no longer reads a combined texture. Useful
for migrating an existing packed/ORM export, or any other tool's G=rough/
B=metal convention, into two maps PBRMaterialComponent can assign
independently.

Each output is written as a flat grayscale (mode "L") PNG - stb_image
(Code/Vendor/stb_image.h), which TextureCache decodes every texture through,
expands that to RGBA with R=G=B=the gray value, so it reads back correctly
through the engine's .r-based sampling.

Requires Pillow (not one of the engine's own dependencies - install with
`pip install Pillow` if this errors on import).

Usage:

    python Scripts/split_metallic_roughness.py combined.png [roughness.png] [metallic.png]

Outputs default to "<stem>_roughness.png" and "<stem>_metallic.png" next to
the input.
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("error: this script needs Pillow - install it with `pip install Pillow`", file=sys.stderr)
    sys.exit(1)


def split_metallic_roughness(combined_path: Path, roughness_path: Path, metallic_path: Path):
    combined = Image.open(combined_path).convert("RGBA")
    _, roughness, metallic, _ = combined.split()

    for path, channel in ((roughness_path, roughness), (metallic_path, metallic)):
        path.parent.mkdir(parents=True, exist_ok=True)
        channel.save(path)
        print(f"Wrote '{path}' ({channel.size[0]}x{channel.size[1]})")


def main():
    parser = argparse.ArgumentParser(description="Split a packed glTF metallic-roughness texture (G=roughness, "
                                                  "B=metallic) into two separate grayscale maps.")
    parser.add_argument("combined", type=Path, help="Packed metallic-roughness texture to split")
    parser.add_argument("roughness", type=Path, nargs="?", default=None,
                        help="Output path for the roughness map (default: '<combined stem>_roughness.png')")
    parser.add_argument("metallic", type=Path, nargs="?", default=None,
                        help="Output path for the metallic map (default: '<combined stem>_metallic.png')")
    args = parser.parse_args()

    print("---[ split_metallic_roughness.py ]---")

    if not args.combined.is_file():
        print(f"error: input not found: {args.combined}", file=sys.stderr)
        return 1

    roughness = args.roughness or args.combined.with_name(f"{args.combined.stem}_roughness.png")
    metallic = args.metallic or args.combined.with_name(f"{args.combined.stem}_metallic.png")
    split_metallic_roughness(args.combined, roughness, metallic)
    return 0


if __name__ == "__main__":
    sys.exit(main())
