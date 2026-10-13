#!/usr/bin/env python3
"""Build a bounded PSP texture/UV sidecar for an existing converted mesh."""
import argparse
import io
import json
import math
from pathlib import Path
import struct
import zlib
from PIL import Image
import p3d_mesh as p

TEXTURE=0x19000
IMAGE=0x19001
IMAGE_DATA=0x19002
SHADER=0x11000
TEX_PARAMETER=0x11002
UV_LIST=0x10007
MAX_TEXTURES=128
MAX_TEXTURE_BYTES=2*1024*1024


def swizzle(pixels,width,height):
    """PSP 16-byte-wide, eight-row texture blocks (RGB5551 is two bytes/pixel)."""
    stride=width*2
    if width<16 or width&(width-1) or height<8 or height&(height-1) or len(pixels)!=stride*height:
        raise p.FormatError("Invalid texture block dimensions")
    return b"".join(pixels[(y+row)*stride+x:(y+row)*stride+x+16]
                    for y in range(0,height,8) for x in range(0,stride,16) for row in range(8))


def decode_texture(texture):
    name,_=p.pstring(texture.payload,0)
    images=[c for c in texture.children if c.identifier==IMAGE]
    if not images:
        raise p.FormatError(f"Texture has no image: {name}")
    image=images[0]  # use base mip; subsequent mips are not used yet
    _,offset=p.pstring(image.payload,0)
    if len(image.payload)!=offset+28:
        raise p.FormatError("Invalid image header")
    _,width,height,_,_,_,fmt=struct.unpack_from("<7I",image.payload,offset)
    if not 1<=width<=1024 or not 1<=height<=1024 or fmt!=1:
        raise p.FormatError("Only bounded embedded PNG images are supported")
    chunks=[c for c in image.children if c.identifier==IMAGE_DATA]
    if len(chunks)!=1 or len(chunks[0].payload)<4:
        raise p.FormatError("Invalid image-data chunk")
    payload=chunks[0].payload
    size=struct.unpack_from("<I",payload)[0]
    if size!=len(payload)-4 or size>4*1024*1024:
        raise p.FormatError("Image length mismatch")
    try:
        with Image.open(io.BytesIO(payload[4:])) as decoded:
            if decoded.format!="PNG" or decoded.size!=(width,height):
                raise p.FormatError("PNG metadata does not match P3D")
            rgba=decoded.convert("RGBA")
    except (OSError,Image.DecompressionBombError) as error:
        raise p.FormatError(f"Invalid PNG: {error}") from error
    # PSP dimensions must be powers of two. Cap at 128 to keep the scene small.
    w=max(16,min(128,1<<(width-1).bit_length()))  # block-aligned PSP texture stride
    h=max(8,min(128,1<<(height-1).bit_length()))
    if rgba.size!=(w,h):
        rgba=rgba.resize((w,h),Image.Resampling.LANCZOS)
    pixels=b"".join(struct.pack("<H",(r>>3)|((g>>3)<<5)|((b>>3)<<10)|((a>=128)<<15))
                    for r,g,b,a in struct.iter_unpack("4B",rgba.tobytes()))
    return w,h,swizzle(pixels,w,h)


def uv_coordinates(group,count):
    selected=None
    for child in group.children:
        if child.identifier!=UV_LIST:
            continue
        if len(child.payload)<8:
            raise p.FormatError("Missing UV-list header")
        num,channel=struct.unpack_from("<II",child.payload)
        if num!=count or len(child.payload)!=8+num*8:
            raise p.FormatError("UV count mismatch")
        if channel==0:
            if selected is not None:
                raise p.FormatError("Duplicate UV channel zero")
            selected=list(struct.iter_unpack("<2f",child.payload[8:]))
            if any(not math.isfinite(v) or abs(v)>10000 for uv in selected for v in uv):
                raise p.FormatError("Invalid UV coordinate")
    return selected


def build(root,mesh_bytes,center,radius):
    expected,_=p.convert(root,center,radius,12000)
    if mesh_bytes!=expected:
        raise p.FormatError("Mesh and texture conversion selection do not match")
    textures={}
    shaders={}
    for c in p.walk(root):
        if c.identifier==TEXTURE:
            name,_=p.pstring(c.payload,0)
            textures.setdefault(name,c)
        if c.identifier==SHADER:
            name,_=p.pstring(c.payload,0)
            for parameter in c.children:
                if parameter.identifier==TEX_PARAMETER and parameter.payload[:4]==b"TEX\0":
                    texture,_=p.pstring(parameter.payload,4)
                    if name in shaders and shaders[name]!=texture:
                        raise p.FormatError("Conflicting shader texture binding")
                    shaders[name]=texture
    # Texture zero is an opaque white fallback for untextured material groups.
    images=[(16,8,b"\xff\xff"*128)]
    image_ids={}
    uvs=[]
    batches=[]
    for mesh in p.static_meshes(root):
        primitive_chunks=[c for c in mesh.children if c.identifier==p.PRIMITIVES]
        for group,(_,shader,positions,_,triangles) in zip(primitive_chunks,p.groups(mesh)):
            group_uvs=uv_coordinates(group,len(positions))
            selected=[]
            for tri in triangles:
                points=[positions[i] for i in tri]
                if (min(v[0] for v in points)>center[0]+radius or max(v[0] for v in points)<center[0]-radius or
                    min(v[2] for v in points)>center[2]+radius or max(v[2] for v in points)<center[2]-radius):
                    continue
                selected.extend(group_uvs[i] if group_uvs else (0,0) for i in tri)
            if not selected:
                continue
            texture_id=0
            if group_uvs and shader in shaders:
                texture_name=shaders[shader]
                if texture_name not in textures:
                    raise p.FormatError(f"Missing source texture: {texture_name}")
                if texture_name not in image_ids:
                    if len(images)>=MAX_TEXTURES:
                        raise p.FormatError("Texture count exceeds runtime budget")
                    image_ids[texture_name]=len(images)
                    images.append(decode_texture(textures[texture_name]))
                    if sum(len(x[2]) for x in images)>MAX_TEXTURE_BYTES:
                        raise p.FormatError("Texture pixels exceed 2 MiB runtime budget")
                texture_id=image_ids[texture_name]
            batches.append((len(uvs),len(selected),texture_id))
            uvs.extend(selected)
    count=struct.unpack_from("<I",mesh_bytes,8)[0]
    if len(uvs)!=count or len(batches)>1024:
        raise p.FormatError("Texture vertex/batch count mismatch")
    output=struct.pack("<4s5I",b"STX1",1,zlib.crc32(mesh_bytes[16:]),count,len(images),len(batches))
    output+=b"".join(struct.pack("<2f",*uv) for uv in uvs)
    # Sorting changes draw order only; every batch still refers to the exact
    # original mesh vertices, preserving collision and source correspondence.
    output+=b"".join(struct.pack("<3I",*b) for b in sorted(batches,key=lambda b:b[2]))
    for width,height,pixels in images:
        output+=struct.pack("<3I",width,height,len(pixels))+pixels
    return output,{"textures":len(images)-1,"batches":len(batches),"vertices":count,
                   "texture_bytes":sum(len(x[2]) for x in images),"file_bytes":len(output),
                   "names":list(image_ids)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input",type=Path,nargs="+")
    parser.add_argument("--mesh",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--center",type=float,nargs=3,required=True)
    parser.add_argument("--radius",type=float,required=True)
    args=parser.parse_args()
    try:
        roots=[]
        for path in args.input:
            if path.stat().st_size>p.MAX_FILE:
                raise p.FormatError("P3D exceeds input limit")
            roots.append(p.read_chunks(path.read_bytes()))
        root=p.Chunk(p.ROOT_ID,b"",roots)
        data,report=build(root,args.mesh.read_bytes(),args.center,args.radius)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_bytes(data)
        print(json.dumps(report,indent=2))
    except (OSError,p.FormatError,struct.error) as error:
        parser.exit(1,f"Texture conversion failed: {error}\n")


if __name__=="__main__":
    main()
