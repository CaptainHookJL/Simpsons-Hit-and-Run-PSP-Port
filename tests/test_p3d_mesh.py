import math
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import p3d_mesh as p


def chunk(identifier,payload=b"",children=()):
    children = b"".join(children)
    return struct.pack("<III",identifier,12+len(payload),12+len(payload)+len(children))+payload+children


def fixture(indices=(0,1,2),primitive=0,positions=((0,0,0),(1,0,0),(0,0,1)),count=None):
    position_data = chunk(p.POSITIONS,struct.pack("<I",len(positions))+b"".join(struct.pack("<3f",*x) for x in positions))
    index_data = chunk(p.INDICES,struct.pack("<I",len(indices))+struct.pack("<"+"I"*len(indices),*indices))
    # One-byte padded P3D strings and the version/primitive fields.
    group = chunk(p.PRIMITIVES,struct.pack("<I",0)+b"\x04test"+struct.pack("<5I",primitive,0,len(positions) if count is None else count,len(indices),0),[position_data,index_data])
    mesh = chunk(p.MESH,b"\x04test"+struct.pack("<II",0,1),[group])
    wrapper = chunk(p.STATIC_ENTITY,b"",[mesh])
    return chunk(p.ROOT_ID,children=[wrapper])


class ImportTests(unittest.TestCase):
    def test_triangle_conversion_and_origin(self):
        result,info = p.convert(p.read_chunks(fixture()),(1,0,0),10,1)
        self.assertEqual(info["triangles"],1)
        self.assertEqual(struct.unpack_from("<I3f",result,16),(0xFFBEBEBE,-1,0,0))

    def test_strip_winding_and_degenerate(self):
        root=p.read_chunks(fixture((0,1,2,2,3,4),1,tuple((i,0,i%2) for i in range(5))))
        triangles=list(p.groups(next(p.static_meshes(root))))[0][-1]
        self.assertEqual(triangles,[(0,1,2),(3,2,4)])

    def test_truncated_or_invalid_headers(self):
        for data in (b"",fixture()[:-1],struct.pack("<III",p.ROOT_ID,0,12),struct.pack("<III",p.ROOT_ID,12,0)):
            with self.subTest(data=data[:12]),self.assertRaises(p.FormatError):
                p.read_chunks(data)

    def test_count_mismatch(self):
        with self.assertRaises(p.FormatError):
            p.convert(p.read_chunks(fixture(count=400)),(0,0,0),10,1)

    def test_out_of_range_index(self):
        with self.assertRaises(p.FormatError):
            p.convert(p.read_chunks(fixture((0,1,7))),(0,0,0),10,1)

    def test_nonfinite_position(self):
        with self.assertRaises(p.FormatError):
            p.convert(p.read_chunks(fixture(positions=((math.nan,0,0),(1,0,0),(0,0,1)))),(0,0,0),10,1)

    def test_no_silent_budget_truncation(self):
        with self.assertRaises(p.FormatError):
            p.convert(p.read_chunks(fixture((0,1,2,0,2,1))),(0,0,0),10,1)

    def test_empty_selection(self):
        with self.assertRaises(p.FormatError):
            p.convert(p.read_chunks(fixture()),(100,0,100),1,1)


if __name__=="__main__":
    unittest.main()
