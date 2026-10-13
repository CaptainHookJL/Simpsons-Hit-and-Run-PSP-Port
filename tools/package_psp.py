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

instructions = """SHAR PSP - Driving Prototype

This is a replacement-engine driving test, not the full Simpsons game.

Requires a PSP configured to run unsigned homebrew (custom firmware).
It is not a signed retail PSP release. No firmware installer is included.

INSTALL
1. Extract this ZIP.
2. Copy the PSP folder to the root of your PSP Memory Stick, merging folders.
3. Confirm the path is PSP/GAME/SHARPSP/EBOOT.PBP.
4. Safely disconnect USB, then open Game > Memory Stick > SHAR PSP.
   On PSP Go, use the corresponding Game storage location.

CONTROLS
Analog stick or D-pad: steer
Cross: accelerate
Square: brake
Circle: reverse
Start: reset vehicle
Home/PS: quit

The screen shows speed and controls. Buildings are visual placeholders without
collision. The test-ground edge stops the vehicle. There are no missions,
original assets, sound, or on-foot gameplay.

The build and native driving tests passed. Hardware and PPSSPP execution
have not yet been verified. If launch fails, record your PSP model, firmware
version, and exact error code so the failure can be diagnosed.
"""
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("PSP/GAME/SHARPSP/EBOOT.PBP", binary)
    archive.writestr("INSTALL.txt", instructions)
with zipfile.ZipFile(args.output) as archive:
    if archive.testzip() is not None or archive.read("PSP/GAME/SHARPSP/EBOOT.PBP") != binary:
        parser.error("Package verification failed")
print(f"Packaged {args.output}: MIPS PRX, standard PSP memory, ZIP integrity verified")
