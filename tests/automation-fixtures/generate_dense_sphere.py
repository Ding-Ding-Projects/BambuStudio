"""Write the 1,035,360-triangle, 20 mm STL used by the native import regression.

Requires NumPy. Pass an unused destination path. The fixture is generated locally.
"""
import argparse
import hashlib
import struct
from pathlib import Path

import numpy as np


def write_sphere(destination):
    divisions = 720
    theta = np.arange(divisions) * 2 * np.pi / divisions

    def ring(latitude):
        angle = np.pi * latitude / divisions
        return np.stack((10 * np.sin(angle) * np.cos(theta), 10 * np.sin(angle) * np.sin(theta),
                         np.full(divisions, 10 * np.cos(angle))), axis=-1).astype("<f4")

    record = np.dtype([("normal", "<f4", (3,)), ("vertices", "<f4", (3, 3)), ("attribute", "<u2")])
    triangles = 2 * divisions * (divisions - 1)
    with destination.open("xb") as output:
        output.write(b"Synthetic 20 mm sphere for native import verification".ljust(80, b" "))
        output.write(struct.pack("<I", triangles))
        for index in range(divisions):
            upper, lower = ring(index), ring(index + 1)
            next_upper, next_lower = np.roll(upper, -1, axis=0), np.roll(lower, -1, axis=0)
            bands = []
            if index < divisions - 1:
                bands.append(np.stack((upper, lower, next_lower), axis=1))
            if index > 0:
                bands.append(np.stack((upper, next_lower, next_upper), axis=1))
            vertices = np.concatenate(bands)
            records = np.zeros(len(vertices), dtype=record)
            normals = np.cross(vertices[:, 1] - vertices[:, 0], vertices[:, 2] - vertices[:, 0])
            records["vertices"] = vertices
            records["normal"] = normals / np.linalg.norm(normals, axis=1)[:, None]
            output.write(records.tobytes())
    if destination.stat().st_size != 84 + 50 * triangles:
        raise RuntimeError("Incomplete binary STL")
    with destination.open("rb") as output:
        digest = hashlib.sha256()
        for chunk in iter(lambda: output.read(1024 * 1024), b""):
            digest.update(chunk)
    print(f"{triangles} triangles, {destination.stat().st_size} bytes, SHA-256 {digest.hexdigest()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    write_sphere(parser.parse_args().destination)
