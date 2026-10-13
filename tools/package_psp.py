#!/usr/bin/env python3
"""Validate and package the built EBOOT for a PSP Memory Stick."""
import argparse
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path, help="Destination ZIP file")
args = parser.parse_args()
binary = (ROOT / "EBOOT.PBP").read_bytes()
if len(binary) < 40:
    parser.error("EBOOT.PBP is truncated")
magic, version, *offsets = struct.unpack_from("<10I", binary)
if magic != 0x50425000 or offsets[0] < 40 or offsets != sorted(offsets) or offsets[-1] > len(binary):
    parser.error("Invalid PBP header")
elf = binary[offsets[6]:offsets[7]]
if len(elf) < 52 or elf[:6] != b"\x7fELF\x01\x01":
    parser.error("Executable is not little-endian ELF32")
elf_type, machine = struct.unpack_from("<HH", elf, 16)
if machine != 8 or elf_type != 0xFFA0:
    parser.error("Expected a relocatable PSP PRX executable")
sfo = binary[offsets[0]:offsets[1]]
if len(sfo) < 20 or sfo[:4] != b"\0PSF":
    parser.error("Missing PSP metadata")
_, _, key_start, data_start, entry_count = struct.unpack_from("<5I", sfo)
memsize = None
for i in range(entry_count):
    key_offset, fmt, size, capacity, value_offset = struct.unpack_from("<HHIII", sfo, 20 + i * 16)
    key = sfo[key_start + key_offset:].split(b"\0", 1)[0]
    if key == b"MEMSIZE":
        memsize = struct.unpack_from("<I", sfo, data_start + value_offset)[0]
if memsize != 0:
    parser.error("Expected standard PSP memory mode (MEMSIZE=0)")

instructions = """SHAR PSP - Driving and On-Foot Prototype

This is a replacement-engine prototype, not the full Simpsons game.
It includes an adapted original Hit & Run steering controller. See SOURCE_REUSE.md.

Requires a PSP configured to run unsigned homebrew (custom firmware).
It is not a signed retail PSP release. No firmware installer is included.

INSTALL
1. Extract this ZIP.
2. Copy the PSP folder to the root of your PSP Memory Stick, merging folders.
3. Confirm the path is PSP/GAME/SHARPSP/EBOOT.PBP.
4. Safely disconnect USB, then open Game > Memory Stick > SHAR PSP.
   On PSP Go, use the corresponding Game storage location.

CONTROLS
Driving: stick/D-pad steers, Cross accelerates, Square brakes, Circle reverses
On foot: stick/D-pad moves, hold Cross to run, L/R turns the camera
Triangle: enter/exit CompactA (stop before exiting; approach the car to enter)
Start: reset to driving at spawn
Home/PS: quit

The screen shows speed, controls, and scenery load status. This package includes
textured original Springfield geometry near the Simpsons house and the original
CompactA car mesh/textures, reused from a parked-car asset. Ground following and
approximate scenery collision are enabled
when the grid loads; the screen shows Collision ON. The car stops at walls and
the edge of imported ground. Walking/running, wall sliding and clear-path car
entry/exit are included. The car stays parked and solid while on foot.
The character is an animated procedural placeholder; original Homer assets
are missing from the GitHub checkout. See CHARACTER.md for details.
Missions, NPCs and sound are absent.
If scenery is missing or invalid, the screen identifies the test-ground fallback.

Optimized clipping build 716b316 was reported at locked 60 FPS on PSP-3000 with
ARK 5. The user confirmed the later CompactA vehicle build e45c6e0 works perfectly.
This build includes on-foot exploration and animates all four original tyres.
The front pair steers. Rotation
follows actual movement after collision, including reverse. The existing steering
and collision footprint stay in use. Original suspension/damage are absent.
Check Vehicle: CompactA and Wheel animation ON in the HUD. Stop and press Triangle
to exit, check ON FOOT, walk/run, rotate the camera, test walls/the parked car,
then return and press Triangle to enter. Check Start reset, driving/wheel behavior,
road continuity and FPS in both modes. Parked-car camera-plane crossings use
software clipping; fully visible views retain direct drawing.
This on-foot build has not yet been tested on a physical PSP.
The sample still has fixed map boundaries; additional map geometry is not included.
Collision approximates the car with two spheres and uses rendered geometry,
not original collision volumes. The camera can still clip through scenery.
If launch fails, record your PSP model, firmware and exact error code.
"""
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("PSP/GAME/SHARPSP/EBOOT.PBP", binary)
    archive.writestr("INSTALL.txt", instructions)
    archive.writestr("SOURCE_REUSE.md", (ROOT / "SOURCE_REUSE.md").read_bytes())
    archive.writestr("THIRD_PARTY.md", (ROOT / "THIRD_PARTY.md").read_bytes())
    archive.writestr("ASSETS.md", (ROOT / "assets/README.md").read_bytes())
    archive.writestr("PSP/GAME/SHARPSP/assets/springfield.shm", (ROOT / "assets/springfield.shm").read_bytes())
    archive.writestr("PSP/GAME/SHARPSP/assets/springfield.stx", (ROOT / "assets/springfield.stx").read_bytes())
    archive.writestr("PSP/GAME/SHARPSP/assets/compact.shm", (ROOT / "assets/compact.shm").read_bytes())
    archive.writestr("PSP/GAME/SHARPSP/assets/compact.stx", (ROOT / "assets/compact.stx").read_bytes())
    archive.writestr("PSP/GAME/SHARPSP/assets/compact.swh", (ROOT / "assets/compact.swh").read_bytes())
    archive.writestr("PSP/GAME/SHARPSP/assets/compact.json", (ROOT / "assets/compact.json").read_bytes())
    archive.writestr("VEHICLE.md", (ROOT / "assets/VEHICLE.md").read_bytes())
    archive.writestr("CHARACTER.md", (ROOT / "assets/CHARACTER.md").read_bytes())
with zipfile.ZipFile(args.output) as archive:
    if (archive.testzip() is not None or archive.read("PSP/GAME/SHARPSP/EBOOT.PBP") != binary
        or archive.read("PSP/GAME/SHARPSP/assets/springfield.shm") != (ROOT / "assets/springfield.shm").read_bytes()
        or archive.read("PSP/GAME/SHARPSP/assets/springfield.stx") != (ROOT / "assets/springfield.stx").read_bytes()
        or archive.read("PSP/GAME/SHARPSP/assets/compact.shm") != (ROOT / "assets/compact.shm").read_bytes()
        or archive.read("PSP/GAME/SHARPSP/assets/compact.stx") != (ROOT / "assets/compact.stx").read_bytes()
        or archive.read("PSP/GAME/SHARPSP/assets/compact.swh") != (ROOT / "assets/compact.swh").read_bytes()):
        parser.error("Package verification failed")
print(f"Packaged {args.output}: MIPS PRX, standard PSP memory, ZIP integrity verified")
