import io
from pathlib import Path
import struct
import sys
import unittest
from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import p3d_mesh as p
import p3d_texture as t


class TextureTests(unittest.TestCase):
    def png_chunk(self,width=16,height=8):
        image=Image.new("RGBA",(width,height),(255,0,0,255))
        image.putpixel((0,0),(0,255,0,0))
        buffer=io.BytesIO();image.save(buffer,format="PNG");data=buffer.getvalue()
        pixels=p.Chunk(t.IMAGE_DATA,struct.pack("<I",len(data))+data,[])
        header=b"\x04test"+struct.pack("<7I",0,width,height,32,0,1,1)
        return p.Chunk(t.TEXTURE,b"\x04test",[p.Chunk(t.IMAGE,header,[pixels])])

    def test_png_colour_and_cutout(self):
        width,height,data=t.decode_texture(self.png_chunk())
        self.assertEqual((width,height),(16,8))
        self.assertEqual(struct.unpack_from("<H",data,0)[0],31<<5) # green with zero alpha
        self.assertEqual(struct.unpack_from("<H",data,2)[0],0x801f) # opaque red

    def test_block_order(self):
        pixels=b"".join(struct.pack("<H",y*32+x) for y in range(16) for x in range(32))
        output=t.swizzle(pixels,32,16)
        self.assertEqual(len(output),len(pixels))
        self.assertEqual(struct.unpack_from("<H",output,16)[0],32) # next row, same block
        self.assertEqual(struct.unpack_from("<H",output,128)[0],8) # next horizontal block
        self.assertEqual(struct.unpack_from("<H",output,512)[0],256) # next block row

    def test_declared_image_size(self):
        chunk=self.png_chunk();header=bytearray(chunk.children[0].payload)
        struct.pack_into("<I",header,9,32) # width disagrees with embedded PNG
        chunk.children[0].payload=bytes(header)
        with self.assertRaises(p.FormatError):t.decode_texture(chunk)

    def test_image_length(self):
        chunk=self.png_chunk();chunk.children[0].children[0].payload=b"\xff\xff\xff\xffx"
        with self.assertRaises(p.FormatError):t.decode_texture(chunk)

    def test_bad_uv(self):
        group=p.Chunk(p.PRIMITIVES,b"",[p.Chunk(t.UV_LIST,struct.pack("<II2f",1,0,float("nan"),0),[])])
        with self.assertRaises(p.FormatError):t.uv_coordinates(group,1)

    def test_mesh_selection_mismatch(self):
        with self.assertRaises(p.FormatError):t.build(p.Chunk(p.ROOT_ID,b"",[]),b"wrong",(0,0,0),45)


if __name__=="__main__":unittest.main()
