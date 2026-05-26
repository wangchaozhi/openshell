#!/usr/bin/env python3
"""Generate native OpenShell app icons without external dependencies."""

from __future__ import annotations

import argparse
import binascii
from functools import lru_cache
import math
import struct
import zlib
from pathlib import Path


BG = (15, 23, 42, 255)
BORDER = (30, 41, 59, 255)
ACCENT = (56, 189, 248, 255)


def blend(dst: tuple[int, int, int, int], src: tuple[int, int, int, int], coverage: float) -> tuple[int, int, int, int]:
    alpha = (src[3] / 255.0) * max(0.0, min(1.0, coverage))
    inv = 1.0 - alpha
    return (
        round(src[0] * alpha + dst[0] * inv),
        round(src[1] * alpha + dst[1] * inv),
        round(src[2] * alpha + dst[2] * inv),
        round(255 * (alpha + (dst[3] / 255.0) * inv)),
    )


def rounded_rect_contains(x: float, y: float, left: float, top: float, right: float, bottom: float, radius: float) -> bool:
    if x < left or x > right or y < top or y > bottom:
        return False
    cx = min(max(x, left + radius), right - radius)
    cy = min(max(y, top + radius), bottom - radius)
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius


def distance_to_segment(px: float, py: float, ax: float, ay: float, bx: float, by: float) -> float:
    vx = bx - ax
    vy = by - ay
    wx = px - ax
    wy = py - ay
    length_sq = vx * vx + vy * vy
    if length_sq <= 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, (wx * vx + wy * vy) / length_sq))
    return math.hypot(px - (ax + t * vx), py - (ay + t * vy))


@lru_cache(maxsize=None)
def render_icon(size: int) -> bytes:
    scale = 4 if size <= 256 else 2 if size <= 512 else 1
    canvas = size * scale
    pixels = [(0, 0, 0, 0)] * (canvas * canvas)
    unit = canvas / 64.0

    rect = (2 * unit, 2 * unit, 62 * unit, 62 * unit, 12 * unit)
    inner = (4 * unit, 4 * unit, 60 * unit, 60 * unit, 10 * unit)
    stroke_half = 1.5 * unit
    line_half = 1.5 * unit
    segments = [
        ((14 * unit, 22 * unit), (24 * unit, 32 * unit)),
        ((24 * unit, 32 * unit), (14 * unit, 42 * unit)),
        ((28 * unit, 44 * unit), (48 * unit, 44 * unit)),
    ]

    for y in range(canvas):
        fy = y + 0.5
        for x in range(canvas):
            fx = x + 0.5
            color = (0, 0, 0, 0)
            if rounded_rect_contains(fx, fy, *rect):
                color = blend(color, BG, 1.0)
            if rounded_rect_contains(fx, fy, *rect) and not rounded_rect_contains(fx, fy, *inner):
                color = blend(color, BORDER, 1.0)
            for (a, b) in segments:
                d = distance_to_segment(fx, fy, a[0], a[1], b[0], b[1])
                coverage = line_half + 0.75 - d
                if coverage > 0:
                    color = blend(color, ACCENT, coverage)
            pixels[y * canvas + x] = color

    if scale == 1:
        return write_png(size, size, pixels)

    downsampled: list[tuple[int, int, int, int]] = []
    samples = scale * scale
    for y in range(size):
        for x in range(size):
            totals = [0, 0, 0, 0]
            for sy in range(scale):
                for sx in range(scale):
                    p = pixels[(y * scale + sy) * canvas + (x * scale + sx)]
                    totals[0] += p[0]
                    totals[1] += p[1]
                    totals[2] += p[2]
                    totals[3] += p[3]
            downsampled.append(tuple(round(v / samples) for v in totals))  # type: ignore[arg-type]
    return write_png(size, size, downsampled)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF)


def write_png(width: int, height: int, pixels: list[tuple[int, int, int, int]]) -> bytes:
    rows = []
    for y in range(height):
        row = bytearray([0])
        for pixel in pixels[y * width : (y + 1) * width]:
            row.extend(pixel)
        rows.append(bytes(row))
    return b"".join(
        [
            b"\x89PNG\r\n\x1a\n",
            png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)),
            png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9)),
            png_chunk(b"IEND", b""),
        ]
    )


def write_ico(path: Path, sizes: list[int]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    images = [render_icon(size) for size in sizes]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = []
    for size, data in zip(sizes, images):
        dim = 0 if size == 256 else size
        entries.append(struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    path.write_bytes(header + b"".join(entries) + b"".join(images))


def write_icns(path: Path, sizes: list[tuple[str, int]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    chunks = []
    for code, size in sizes:
        png = render_icon(size)
        chunks.append(code.encode("ascii") + struct.pack(">I", len(png) + 8) + png)
    body = b"".join(chunks)
    path.write_bytes(b"icns" + struct.pack(">I", len(body) + 8) + body)


def write_file(path: Path, data: bytes | str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(data, str):
        path.write_text(data, encoding="utf-8")
    else:
        path.write_bytes(data)


def android_manifest() -> str:
    return """<?xml version="1.0"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.wangchaozhi.openshell"
    android:installLocation="auto"
    android:versionCode="1"
    android:versionName="1.0">
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
    <supports-screens
        android:anyDensity="true"
        android:largeScreens="true"
        android:normalScreens="true"
        android:smallScreens="true" />
    <application
        android:name="org.qtproject.qt.android.bindings.QtApplication"
        android:hardwareAccelerated="true"
        android:label="OpenShell"
        android:icon="@mipmap/ic_launcher"
        android:roundIcon="@mipmap/ic_launcher_round"
        android:enableOnBackInvokedCallback="false"
        android:allowBackup="true"
        android:fullBackupOnly="false">
        <activity
            android:name="org.qtproject.qt.android.bindings.QtActivity"
            android:configChanges="orientation|uiMode|screenLayout|screenSize|smallestScreenSize|layoutDirection|locale|fontScale|keyboard|keyboardHidden|navigation|mcc|mnc|density"
            android:label="OpenShell"
            android:launchMode="singleTop"
            android:screenOrientation="unspecified"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
            <meta-data android:name="android.app.lib_name" android:value="OpenShell" />
            <meta-data android:name="android.app.arguments" android:value="" />
        </activity>
        <provider
            android:name="androidx.core.content.FileProvider"
            android:authorities="${applicationId}.qtprovider"
            android:exported="false"
            android:grantUriPermissions="true">
            <meta-data
                android:name="android.support.FILE_PROVIDER_PATHS"
                android:resource="@xml/qtprovider_paths" />
        </provider>
    </application>
</manifest>
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="assets/platform", help="output directory")
    args = parser.parse_args()

    root = Path(args.output)
    write_ico(root / "windows" / "OpenShell.ico", [16, 24, 32, 48, 64, 128, 256])

    write_icns(
        root / "macos" / "OpenShell.icns",
        [("icp4", 16), ("icp5", 32), ("icp6", 64), ("ic07", 128), ("ic08", 256), ("ic09", 512), ("ic10", 1024)],
    )

    android_sizes = {
        "mipmap-mdpi": 48,
        "mipmap-hdpi": 72,
        "mipmap-xhdpi": 96,
        "mipmap-xxhdpi": 144,
        "mipmap-xxxhdpi": 192,
    }
    for density, size in android_sizes.items():
        png = render_icon(size)
        write_file(root / "android" / "res" / density / "ic_launcher.png", png)
        write_file(root / "android" / "res" / density / "ic_launcher_round.png", png)
    write_file(root / "android" / "AndroidManifest.xml", android_manifest())

    ios_sizes = {
        "AppIcon20x20@2x.png": 40,
        "AppIcon20x20@3x.png": 60,
        "AppIcon29x29@2x.png": 58,
        "AppIcon29x29@3x.png": 87,
        "AppIcon40x40@2x.png": 80,
        "AppIcon40x40@3x.png": 120,
        "AppIcon60x60@2x.png": 120,
        "AppIcon60x60@3x.png": 180,
        "AppIcon76x76.png": 76,
        "AppIcon76x76@2x.png": 152,
        "AppIcon83.5x83.5@2x.png": 167,
        "AppIcon1024x1024.png": 1024,
    }
    for name, size in ios_sizes.items():
        write_file(root / "ios" / name, render_icon(size))

    print(f"Generated platform icons in {root}")


if __name__ == "__main__":
    main()
