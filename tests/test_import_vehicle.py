from pathlib import Path
import math
import struct
import sys
import unittest
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import p3d_mesh as p
import import_vehicle as v
from test_p3d_mesh import fixture


class VehicleImportTests(unittest.TestCase):
    def root(self):
        return p.read_chunks(fixture(positions=((-1,-0.5,-2),(1,-0.5,2),(0,0.5,0))))

    def test_grounded_uniform_fit_and_texture_binding(self):
        mesh,texture,report=v.convert(self.root())
        vertices=list(struct.iter_unpack("<I3f",mesh[16:]))
        self.assertAlmostEqual(min(x[2] for x in vertices),0)
        self.assertAlmostEqual(max(x[1] for x in vertices)-min(x[1] for x in vertices),1.8,places=5)
        self.assertAlmostEqual(max(x[3] for x in vertices)-min(x[3] for x in vertices),3.6,places=5)
        self.assertAlmostEqual(report["uniform_scale"],0.9)
        self.assertEqual(struct.unpack_from("<I",texture,8)[0],zlib.crc32(mesh[16:]))
        self.assertEqual(struct.unpack_from("<I",texture,12)[0],len(vertices))

    def test_degenerate_model_rejected(self):
        root=p.read_chunks(fixture()) # flat plane cannot be a vehicle body
        with self.assertRaises(p.FormatError):v.convert(root)

    def test_invalid_dimensions_rejected(self):
        for width in (0,-1,math.nan,math.inf,100):
            with self.subTest(width=width),self.assertRaises(p.FormatError):v.convert(self.root(),width=width)

    def test_missing_mesh_rejected(self):
        with self.assertRaises(p.FormatError):v.convert(p.Chunk(p.ROOT_ID,b"",[]))


if __name__=="__main__":unittest.main()
