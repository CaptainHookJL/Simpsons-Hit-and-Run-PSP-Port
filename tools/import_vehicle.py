#!/usr/bin/env python3
"""Convert an original rigid PC P3D vehicle mesh into the prototype's formats.

This imports the visual model only. --split-wheels identifies CompactA's four
disconnected tyres for runtime animation. Skeletons, original tuning and damage
are not imported. Dimensions fit the existing collision footprint.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import zlib
import p3d_mesh as p
import p3d_texture as t


def convert(root,width=1.8,length=3.6):
    if any(not math.isfinite(v) or v<=0 or v>10 for v in (width,length)):
        raise p.FormatError("Target vehicle dimensions must be between zero and ten metres")
    meshes=list(p.static_meshes(root))
    if not meshes:
        raise p.FormatError("No rigid root/static vehicle meshes; skinned/hierarchical vehicles need another importer")
    points=[v for mesh in meshes for _,_,positions,_,_ in p.groups(mesh) for v in positions]
    if not points:
        raise p.FormatError("Vehicle has no positions")
    low=[min(v[i] for v in points) for i in range(3)]
    high=[max(v[i] for v in points) for i in range(3)]
    size=[high[i]-low[i] for i in range(3)]
    if any(v<0.01 or v>100 for v in size):
        raise p.FormatError("Vehicle bounds are degenerate or exceed the import limit")
    center=[(low[0]+high[0])*0.5,low[1],(low[2]+high[2])*0.5]
    radius=max(size[0],size[2])+1
    mesh,report=p.convert(root,center,radius,2048)
    texture,texture_report=t.build(root,mesh,center,radius)
    scale=min(width/size[0],length/size[2])
    if not 0.01<=scale<=10:
        raise p.FormatError("Vehicle normalization scale is out of range")
    vertices=list(struct.iter_unpack("<I3f",mesh[16:]))
    mesh=mesh[:16]+b"".join(struct.pack("<I3f",color,x*scale,y*scale,z*scale)
                            for color,x,y,z in vertices)
    # Bind the same UV/material records to the newly normalized geometry.
    texture=texture[:8]+struct.pack("<I",zlib.crc32(mesh[16:]))+texture[12:]
    report.update({"source_center":center,"uniform_scale":scale,
                   "normalized_dimensions":[v*scale for v in size],
                   "textures":texture_report["textures"],"texture_bytes":texture_report["texture_bytes"],
                   "texture_names":texture_report["names"],"animated_wheels":False,
                   "mesh_sha256":hashlib.sha256(mesh).hexdigest(),
                   "texture_sha256":hashlib.sha256(texture).hexdigest()})
    return mesh,texture,report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input",type=Path)
    parser.add_argument("--output-dir",type=Path,required=True)
    parser.add_argument("--prefix",default="compact")
    parser.add_argument("--split-wheels",action="store_true",help="Create CompactA tyre pivots and triangle membership")
    args=parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+",args.prefix):
        parser.error("--prefix must be a plain file name")
    try:
        if args.input.stat().st_size>p.MAX_FILE:
            raise p.FormatError("P3D exceeds input limit")
        data=args.input.read_bytes()
        mesh,texture,report=convert(p.read_chunks(data))
        report["source_file"]=args.input.name
        report["source_sha256"]=hashlib.sha256(data).hexdigest()
        wheel_data=None
        if args.split_wheels:
            from split_wheels import split
            wheel_data,wheel_report=split(mesh)
            report["animated_wheels"]=True
            report["wheel_rig"]=wheel_report
        args.output_dir.mkdir(parents=True,exist_ok=True)
        (args.output_dir/(args.prefix+".shm")).write_bytes(mesh)
        (args.output_dir/(args.prefix+".stx")).write_bytes(texture)
        if wheel_data is not None:
            (args.output_dir/(args.prefix+".swh")).write_bytes(wheel_data)
        (args.output_dir/(args.prefix+".json")).write_text(json.dumps(report,indent=2)+"\n")
        print(json.dumps(report,indent=2))
    except (OSError,p.FormatError,struct.error) as error:
        parser.exit(1,f"Vehicle conversion failed: {error}\n")


if __name__=="__main__":
    main()
