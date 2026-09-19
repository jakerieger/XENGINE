"""
Generates a small synthetic equirectangular sky+sun HDRI (Radiance .hdr) for
testing the engine's image-based lighting pipeline without needing a real
environment capture. Not shipped content - regenerate it any time with:

    python Scripts/generate_test_hdri.py [output.hdr]

Direction -> pixel mapping matches PBR.hlsl's DirToEquirectUV exactly: image
center (u = 0.5) is the engine's forward direction (-Z, glTF's convention),
+X is to the right (u = 0.75), and the top row (v = 0) is straight up (+Y).
Keep this in sync with that function if either changes, or the sun below
won't land where this script says it does.

Written as flat (non-RLE) RGBE scanlines: stb_image's Radiance loader
(Code/Vendor/stb_image.h, stbi__hdr_load) accepts that, and it avoids
implementing run-length encoding for a file this small.
"""

import math
import sys
from pathlib import Path

WIDTH = 512
HEIGHT = 256

SUN_DIRECTION = (-0.35, 0.55, 0.75)  # behind/above-left of a camera looking down -Z
SUN_COLOR = (1.0, 0.95, 0.85)
SUN_CORE_COS = 0.9985  # ~3 degrees
SUN_HALO_COS = 0.985
SUN_CORE_INTENSITY = 120.0  # deliberately far above 1.0 - the point of an HDR image

SKY_ZENITH = (0.25, 0.45, 0.95)
SKY_HORIZON = (0.85, 0.85, 0.8)
GROUND_NEAR = (0.22, 0.17, 0.12)
GROUND_FAR = (0.08, 0.07, 0.06)


def normalize(v):
    length = math.sqrt(sum(c * c for c in v))
    return tuple(c / length for c in v)


def lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def to_rgbe(r, g, b):
    """Ward's shared-exponent RGBE encoding - see the Radiance file format spec."""
    v = max(r, g, b)
    if v < 1e-32:
        return (0, 0, 0, 0)
    mantissa, exponent = math.frexp(v)
    scale = mantissa * 256.0 / v
    return (
        min(255, int(r * scale)),
        min(255, int(g * scale)),
        min(255, int(b * scale)),
        exponent + 128,
    )


def shade(direction, sun_dir):
    x, y, z = direction

    if y >= 0.0:
        color = lerp(SKY_HORIZON, SKY_ZENITH, smoothstep(y))
    else:
        color = lerp(GROUND_NEAR, GROUND_FAR, smoothstep(-y * 2.0))
        # Longitude stripes on the ground so reflected orientation is easy to
        # verify by eye (a flat gradient looks identical from every angle).
        longitude = math.atan2(x, -z)
        stripe = 1.0 + 0.5 * math.copysign(1.0, math.sin(longitude * 6.0))
        color = tuple(c * stripe for c in color)

    cos_sun = x * sun_dir[0] + y * sun_dir[1] + z * sun_dir[2]
    if cos_sun > SUN_HALO_COS:
        t = (cos_sun - SUN_HALO_COS) / (SUN_CORE_COS - SUN_HALO_COS)
        intensity = 4.0 * smoothstep(t)
        if cos_sun > SUN_CORE_COS:
            intensity = SUN_CORE_INTENSITY
        color = tuple(c + intensity * s for c, s in zip(color, SUN_COLOR))

    return color


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("test_env.hdr")
    sun_dir = normalize(SUN_DIRECTION)

    with open(out_path, "wb") as f:
        f.write(b"#?RADIANCE\n")
        f.write(b"# Synthetic test HDRI - see Scripts/generate_test_hdri.py\n")
        f.write(b"FORMAT=32-bit_rle_rgbe\n\n")
        f.write(f"-Y {HEIGHT} +X {WIDTH}\n".encode("ascii"))

        for row in range(HEIGHT):
            v = (row + 0.5) / HEIGHT
            phi = (0.5 - v) * math.pi  # +pi/2 (up) at the top row
            y = math.sin(phi)
            radius = math.cos(phi)

            scanline = bytearray()
            for col in range(WIDTH):
                u = (col + 0.5) / WIDTH
                theta = (u - 0.5) * 2.0 * math.pi  # 0 = forward (-Z), +pi/2 = +X
                direction = (radius * math.sin(theta), y, -radius * math.cos(theta))
                scanline += bytes(to_rgbe(*shade(direction, sun_dir)))
            f.write(scanline)

    print(f"Wrote {out_path} ({WIDTH}x{HEIGHT})")


if __name__ == "__main__":
    main()
