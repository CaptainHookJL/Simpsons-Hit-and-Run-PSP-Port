from pathlib import Path
import math
import struct
import sys
import unittest
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import p3d_mesh as p
import split_wheels as w


class WheelSplitTests(unittest.TestCase):
    def test_original_model_membership_and_binding(self):
        mesh=(ROOT/"assets/compact.shm").read_bytes()
        data,report=w.split(mesh)
        self.assertEqual(data,(ROOT/"assets/compact.swh").read_bytes())
        self.assertEqual(data,w.split(mesh)[0])
        self.assertEqual(struct.unpack_from("<4s4I",data),
                         (b"SWH1",1,zlib.crc32(mesh[16:]),930,4))
        self.assertEqual(report["body_triangles"],166)
        self.assertEqual(report["wheel_triangles"],144)
        ids=[]
        for wheel in report["wheels"]:
            self.assertEqual(len(wheel["triangles"]),36)
            self.assertAlmostEqual(wheel["radius"],0.3344,places=3)
            ids.extend(wheel["triangles"])
        self.assertEqual(len(set(ids)),144)
        self.assertTrue(all(0<=i<310 for i in ids))
        self.assertEqual([x["front"] for x in report["wheels"]],[True,True,False,False])

    def test_malformed_mesh_rejected(self):
        valid=(ROOT/"assets/compact.shm").read_bytes()
        for mesh in (b"",valid[:15],valid[:-1],valid+b"x",
                     b"BAD!"+valid[4:],valid[:12]+struct.pack("<I",1)+valid[16:]):
            with self.subTest(size=len(mesh)),self.assertRaises(p.FormatError):w.split(mesh)
        for value in (math.inf,math.nan):
            invalid=bytearray(valid)
            struct.pack_into("<f",invalid,20,value)
            with self.assertRaises(p.FormatError):w.split(invalid)

    def test_missing_or_joined_tyres_rejected(self):
        mesh=(ROOT/"assets/compact.shm").read_bytes()
        vertices=list(struct.iter_unpack("<I3f",mesh[16:]))
        _,report=w.split(mesh)
        first=report["wheels"][0]["triangles"]
        removed=set(first)
        remaining=[v for i,v in enumerate(vertices) if i//3 not in removed]
        invalid=struct.pack("<4s3I",b"SHM1",1,len(remaining),0)+b"".join(struct.pack("<I3f",*v) for v in remaining)
        with self.assertRaises(p.FormatError):w.split(invalid)
        # A bridging triangle joins two tyres, so neither remains a valid part.
        vertices.extend((vertices[first[0]*3],vertices[report["wheels"][1]["triangles"][0]*3],vertices[first[0]*3+1]))
        invalid=struct.pack("<4s3I",b"SHM1",1,len(vertices),0)+b"".join(struct.pack("<I3f",*v) for v in vertices)
        with self.assertRaises(p.FormatError):w.split(invalid)


if __name__=="__main__":unittest.main()
