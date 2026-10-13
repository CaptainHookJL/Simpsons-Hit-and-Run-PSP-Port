#!/usr/bin/env python3
"""Inspect uncompressed PC Pure3D meshes and convert a bounded scenery sample.

Binary layouts checked against Hampo/LuaP3DLib (see THIRD_PARTY.md).
Only static meshes are supported; skinning and scene transforms are not applied.
"""
import argparse
from collections import Counter
from dataclasses import dataclass
import json
import math
from pathlib import Path
import struct

ROOT_ID = 0xFF443350
MESH = 0x10000
PRIMITIVES = 0x10002
POSITIONS = 0x10005
COLOURS = 0x10008
INDICES = 0x1000A
STATIC_ENTITY = 0x03F00000
MAX_FILE = 64 * 1024 * 1024
MAX_VERTICES = 49152  # 768 KiB; triangle-list vertices, not original unique vertices


class FormatError(ValueError):
    pass


@dataclass
class Chunk:
    identifier: int
    payload: bytes
    children: list


def read_chunks(data):
    if len(data) > MAX_FILE:
        raise FormatError("P3D exceeds 64 MiB input limit")
    count = 0

    def read(start, end, depth):
        nonlocal count
        if depth > 64:
            raise FormatError("Chunk nesting exceeds 64 levels")
        chunks = []
        while start < end:
            if end - start < 12:
                raise FormatError("Truncated chunk header")
            ident, header, total = struct.unpack_from("<III", data, start)
            if not 12 <= header <= total <= end - start:
                raise FormatError("Invalid chunk sizes or child outside parent")
            count += 1
            if count > 100000:
                raise FormatError("Too many chunks")
            chunks.append(Chunk(ident, data[start+12:start+header],
                                read(start+header, start+total, depth+1)))
            start += total
        return chunks

    roots = read(0, len(data), 0)
    if len(roots) != 1 or roots[0].identifier != ROOT_ID:
        raise FormatError("Expected uncompressed little-endian PC P3D root")
    return roots[0]


def walk(chunk):
    yield chunk
    for child in chunk.children:
        yield from walk(child)


def pstring(data, offset):
    if offset >= len(data):
        raise FormatError("Missing P3D string length")
    size = data[offset]
    end = offset + 1 + size
    if end > len(data):
        raise FormatError("Truncated P3D string")
    return data[offset+1:end].rstrip(b"\0").decode("latin1"), end


def array(chunk, fmt):
    if len(chunk.payload) < 4:
        raise FormatError("Missing array count")
    count = struct.unpack_from("<I", chunk.payload)[0]
    stride = struct.calcsize(fmt)
    if count > 1000000 or len(chunk.payload) != 4 + count * stride:
        raise FormatError("Array count does not match payload")
    return list(struct.iter_unpack(fmt, chunk.payload[4:]))


def groups(mesh):
    name, _ = pstring(mesh.payload, 0)
    for group in mesh.children:
        if group.identifier != PRIMITIVES:
            continue
        shader, offset = pstring(group.payload, 4)
        if len(group.payload) != offset + 20:
            raise FormatError("Unexpected primitive-group header")
        primitive, flags, num_vertices, num_indices, num_matrices = struct.unpack_from("<5I", group.payload, offset)
        if num_matrices or flags & ((1 << 7) | (1 << 8)):
            raise FormatError(f"Skinned mesh requires a skeleton: {name}")
        children = {}
        for child in group.children:
            if child.identifier in (POSITIONS, INDICES, COLOURS):
                if child.identifier in children:
                    raise FormatError("Duplicate vertex/index/colour array")
                children[child.identifier] = child
        if POSITIONS not in children:
            raise FormatError("Mesh primitive has no positions")
        positions = array(children[POSITIONS], "<3f")
        if len(positions) != num_vertices:
            raise FormatError("Position count does not match primitive header")
        if any(not math.isfinite(v) or abs(v) > 100000 for p in positions for v in p):
            raise FormatError("Nonfinite or out-of-range vertex position")
        if INDICES in children:
            indices = [x[0] for x in array(children[INDICES], "<I")]
        elif num_indices == 0:
            indices = list(range(num_vertices))
        else:
            raise FormatError("Missing indices")
        if INDICES in children and len(indices) != num_indices:
            raise FormatError("Index count does not match primitive header")
        if any(i >= num_vertices for i in indices):
            raise FormatError("Vertex index out of bounds")
        colours = array(children[COLOURS], "<4B") if COLOURS in children else None
        if colours is not None and len(colours) != num_vertices:
            raise FormatError("Colour count does not match position count")
        if primitive == 0:
            if len(indices) % 3:
                raise FormatError("Incomplete triangle list")
            triangles = [tuple(indices[i:i+3]) for i in range(0,len(indices),3)]
        elif primitive == 1:
            triangles = [(indices[i],indices[i+1],indices[i+2]) if i % 2 == 0
                         else (indices[i+1],indices[i],indices[i+2])
                         for i in range(len(indices)-2)]
        else:
            raise FormatError(f"Unsupported primitive type {primitive}")
        yield name, shader, positions, colours, [t for t in triangles if len(set(t)) == 3]


def static_meshes(root):
    # StaticEntityLoader consumes geometry in world coordinates. Other wrappers
    # can require instance transforms or skeletons and are deliberately excluded.
    for chunk in walk(root):
        if chunk.identifier == ROOT_ID:
            for child in chunk.children:
                if child.identifier == MESH:
                    yield child
        if chunk.identifier == STATIC_ENTITY:
            for child in chunk.children:
                if child.identifier == MESH:
                    yield child


def convert(root, center, radius, max_triangles):
    if not 1 <= max_triangles <= MAX_VERTICES // 3:
        raise FormatError("Triangle budget must be between 1 and 16384")
    if not math.isfinite(radius) or radius <= 0 or radius > 500:
        raise FormatError("Radius must be between 0 and 500 metres")
    if len(center) != 3 or any(not math.isfinite(x) for x in center):
        raise FormatError("Invalid center")
    selected = []
    used = set()
    for mesh in static_meshes(root):
        for name, shader, positions, colours, triangles in groups(mesh):
            for tri in triangles:
                points = [positions[i] for i in tri]
                # Retain any triangle that intersects the XZ selection square;
                # do not cut individual triangles or silently truncate a budget.
                if (min(p[0] for p in points) > center[0]+radius or
                    max(p[0] for p in points) < center[0]-radius or
                    min(p[2] for p in points) > center[2]+radius or
                    max(p[2] for p in points) < center[2]-radius):
                    continue
                if len(selected) >= max_triangles:
                    raise FormatError("Selected scenery exceeds triangle budget; reduce radius")
                vertices = []
                for index in tri:
                    x,y,z = positions[index]
                    if colours:
                        b,g,r,_ = colours[index]
                    else:
                        r,g,b = 190,190,190
                    # Vertex lighting exists in some source meshes; materials
                    # and alpha blending are not yet implemented.
                    color = r | (g << 8) | (b << 16) | 0xFF000000
                    vertices.append((color,x-center[0],y-center[1],z-center[2]))
                selected.append(vertices)
                used.add(name)
    if not selected:
        raise FormatError("No static triangles selected")
    output = struct.pack("<4sIII",b"SHM1",1,len(selected)*3,0)
    output += b"".join(struct.pack("<I3f",*v) for tri in selected for v in tri)
    return output, {"triangles":len(selected),"vertices":len(selected)*3,
                    "mesh_count":len(used),"bytes":len(output),"center":center,"radius":radius}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input",type=Path,nargs="+")
    parser.add_argument("--output",type=Path)
    parser.add_argument("--center",type=float,nargs=3)
    parser.add_argument("--radius",type=float,default=30)
    parser.add_argument("--max-triangles",type=int,default=12000)
    args = parser.parse_args()
    try:
        roots = []
        for path in args.input:
            if path.stat().st_size > MAX_FILE:
                raise FormatError("P3D exceeds input limit")
            roots.append(read_chunks(path.read_bytes()))
        root = Chunk(ROOT_ID,b"",roots)
        if args.output:
            if args.center is None:
                parser.error("--output requires --center X Y Z")
            data,report = convert(root,args.center,args.radius,args.max_triangles)
            args.output.parent.mkdir(parents=True,exist_ok=True)
            args.output.write_bytes(data)
            print(json.dumps(report,indent=2))
        else:
            meshes = list(static_meshes(root))
            points = [p for mesh in meshes for _,_,positions,_,_ in groups(mesh) for p in positions]
            print(json.dumps({"static_meshes":len(meshes),"position_count":len(points),
                "bounds_min":[min(p[i] for p in points) for i in range(3)] if points else None,
                "bounds_max":[max(p[i] for p in points) for i in range(3)] if points else None,
                "chunks":{hex(k):v for k,v in Counter(c.identifier for c in walk(root)).items()}},indent=2))
    except (OSError,FormatError,struct.error) as error:
        parser.exit(1,f"P3D conversion failed: {error}\n")


if __name__ == "__main__":
    main()
