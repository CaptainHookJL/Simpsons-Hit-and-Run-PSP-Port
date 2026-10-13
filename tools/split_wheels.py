#!/usr/bin/env python3
"""Identify the four disconnected tyre meshes in the imported CompactA model."""
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib
import p3d_mesh as p


def split(mesh):
    if len(mesh)<16:
        raise p.FormatError("Truncated mesh")
    magic,version,count,reserved=struct.unpack_from("<4s3I",mesh)
    if magic!=b"SHM1" or version!=1 or reserved or not count or count%3 or count>p.MAX_VERTICES or len(mesh)!=16+count*16:
        raise p.FormatError("Invalid mesh header")
    vertices=list(struct.iter_unpack("<I3f",mesh[16:]))
    if any(not math.isfinite(x) for v in vertices for x in v[1:]):
        raise p.FormatError("Nonfinite geometry")
    parents={}
    def key(vertex):return tuple(round(x,5) for x in vertex[1:])
    def find(k):
        parents.setdefault(k,k)
        while parents[k]!=k:
            parents[k]=parents[parents[k]]
            k=parents[k]
        return k
    def union(a,b):parents[find(a)]=find(b)
    for i in range(0,count,3):
        a,b,c=[key(v) for v in vertices[i:i+3]]
        union(a,b);union(a,c)
    components=defaultdict(list)
    for i in range(0,count,3):components[find(key(vertices[i]))].append(i//3)
    wheels=[]
    for triangles in components.values():
        points=[v[1:] for i in triangles for v in vertices[i*3:i*3+3]]
        low=[min(v[i] for v in points) for i in range(3)]
        high=[max(v[i] for v in points) for i in range(3)]
        extent=[high[i]-low[i] for i in range(3)]
        center=[(low[i]+high[i])*0.5 for i in range(3)]
        # Conservative CompactA-specific identification: disconnected pieces
        # near ground, at outer X and front/rear Z, with a thin axle width.
        if (len(triangles)>=8 and abs(center[0])>0.5 and abs(center[2])>0.5 and
            low[1]<0.05 and 0.2<extent[1]<1 and 0.2<extent[2]<1 and extent[0]<0.4):
            radius=max(math.hypot(v[1]-center[1],v[2]-center[2]) for v in points)
            wheels.append({"pivot":center,"radius":radius,"front":center[2]>0,
                           "triangles":triangles})
    if len(wheels)!=4 or len({(w["pivot"][0]>0,w["front"]) for w in wheels})!=4:
        raise p.FormatError("Expected one disconnected tyre at each CompactA wheel position")
    wheels.sort(key=lambda w:(not w["front"],w["pivot"][0]))
    output=struct.pack("<4s4I",b"SWH1",1,zlib.crc32(mesh[16:]),count,4)
    for wheel in wheels:
        ids=wheel["triangles"]
        output+=struct.pack("<4f2I",*wheel["pivot"],wheel["radius"],wheel["front"],len(ids))
        output+=struct.pack("<"+"I"*len(ids),*ids)
    used=sum(len(w["triangles"]) for w in wheels)
    return output,{"wheel_count":4,"wheel_triangles":used,"body_triangles":count//3-used,
                   "wheels":wheels,"sha256":hashlib.sha256(output).hexdigest()}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mesh",type=Path)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    try:
        if args.mesh.stat().st_size>16+p.MAX_VERTICES*16:
            raise p.FormatError("Mesh exceeds input limit")
        data,report=split(args.mesh.read_bytes())
        args.output.write_bytes(data)
        print(json.dumps(report,indent=2))
    except (OSError,p.FormatError,struct.error) as error:
        parser.exit(1,f"Wheel split failed: {error}\n")


if __name__=="__main__":main()
