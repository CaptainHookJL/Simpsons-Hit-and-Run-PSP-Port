#!/usr/bin/env python3
"""Report whether the upstream game's middleware sources are available."""
import argparse
from pathlib import Path
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path, help="Root of the original source checkout")
args = parser.parse_args()
root = args.source.resolve()
required = ["radcore", "radmovie", "radsound", "radscript", "radmusic",
            "pure3d", "scrooby", "radmath", "sim", "poser", "choreo", "radcontent"]
if not (root / "game/code/main/game.cpp").is_file():
    parser.error("Not an original Hit & Run source root: game/code/main/game.cpp missing")
missing = []
for name in required:
    path = root / "game/libs" / name
    present = path.is_dir() and any(path.iterdir())
    print(f"{'PRESENT' if present else 'MISSING'} game/libs/{name}")
    if not present:
        missing.append(name)
print("This is a directory audit; it does not prove PSP compatibility or buildability.")
sys.exit(1 if missing else 0)
