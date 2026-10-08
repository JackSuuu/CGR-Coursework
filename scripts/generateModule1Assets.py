"""Generate reproducible OBJ/PPM scene assets (Python standard library only).

These are procedural assets, not the student's required inspiration photograph.
Run from any directory: python3 scripts/generateModule1Assets.py
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def normalise(v):
    length = math.sqrt(sum(x * x for x in v))
    return tuple(x / length for x in v)


def write_ppm(name, size, colour):
    lines = ["P3", f"{size} {size}", "255"]
    for y in range(size):
        lines.append(" ".join(" ".join(str(max(0, min(255, int(c)))) for c in colour(x, y))
                              for x in range(size)))
    (ROOT / "scenes" / "textures" / name).write_text("\n".join(lines) + "\n")


def geodesic_mesh():
    golden = (1 + math.sqrt(5)) / 2
    vertices = [normalise(v) for v in [
        (-1, golden, 0), (1, golden, 0), (-1, -golden, 0), (1, -golden, 0),
        (0, -1, golden), (0, 1, golden), (0, -1, -golden), (0, 1, -golden),
        (golden, 0, -1), (golden, 0, 1), (-golden, 0, -1), (-golden, 0, 1)]]
    faces = [(0,11,5),(0,5,1),(0,1,7),(0,7,10),(0,10,11),(1,5,9),(5,11,4),
             (11,10,2),(10,7,6),(7,1,8),(3,9,4),(3,4,2),(3,2,6),(3,6,8),
             (3,8,9),(4,9,5),(2,4,11),(6,2,10),(8,6,7),(9,8,1)]
    for _ in range(2):
        cache = {}

        def midpoint(a, b):
            key = tuple(sorted((a, b)))
            if key not in cache:
                cache[key] = len(vertices)
                vertices.append(normalise(tuple((vertices[a][i] + vertices[b][i]) / 2
                                                 for i in range(3))))
            return cache[key]

        refined = []
        for a, b, c in faces:
            ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
            refined.extend([(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)])
        faces = refined

    lines = ["# UV geodesic sphere: 320 non-overlapping faces; explicit smooth normals."]
    lines.extend("v %.9f %.9f %.9f" % v for v in vertices)
    lines.extend("vn %.9f %.9f %.9f" % v for v in vertices)
    face_records = []
    uv_index = 1
    for face in faces:
        uvs = []
        for i in face:
            x, y, z = vertices[i]
            uvs.append([(math.atan2(y, x) % (2 * math.pi)) / (2 * math.pi),
                        1 - math.acos(max(-1, min(1, z))) / math.pi])
        # Interpolate across the longitude seam locally, not across the globe.
        if max(uv[0] for uv in uvs) - min(uv[0] for uv in uvs) > 0.5:
            for uv in uvs:
                if uv[0] < 0.5:
                    uv[0] += 1
        for uv in uvs:
            lines.append("vt %.9f %.9f" % tuple(uv))
        face_records.append("f " + " ".join(f"{i+1}/{uv_index+j}/{i+1}"
                                            for j, i in enumerate(face)))
        uv_index += 3
    lines.extend(face_records)
    (ROOT / "scenes" / "meshes" / "uv_geodesic.obj").write_text("\n".join(lines) + "\n")


def box_mesh():
    faces = [
        [(-.5,0,.5),(.5,0,.5),(.5,1,.5),(-.5,1,.5)],
        [(.5,0,-.5),(-.5,0,-.5),(-.5,1,-.5),(.5,1,-.5)],
        [(.5,0,.5),(.5,0,-.5),(.5,1,-.5),(.5,1,.5)],
        [(-.5,0,-.5),(-.5,0,.5),(-.5,1,.5),(-.5,1,-.5)],
        [(-.5,1,.5),(.5,1,.5),(.5,1,-.5),(-.5,1,-.5)],
        [(-.5,0,-.5),(.5,0,-.5),(.5,0,.5),(-.5,0,.5)]]
    normals = [(0,0,1),(0,0,-1),(1,0,0),(-1,0,0),(0,1,0),(0,-1,0)]
    lines = ["# Unit box: x,z in [-.5,.5], y in [0,1]; flat face normals and UVs."]
    for face in faces:
        lines.extend("v %g %g %g" % p for p in face)
    lines.extend("vt %g %g" % uv for uv in [(0,0),(1,0),(1,1),(0,1)])
    lines.extend("vn %g %g %g" % n for n in normals)
    for f in range(6):
        corners = [f"{4*f+i+1}/{i+1}/{f+1}" for i in range(4)]
        lines.extend(["f " + " ".join(corners[:3]),
                      "f " + " ".join([corners[0], corners[2], corners[3]])])
    (ROOT / "scenes" / "meshes" / "display_box.obj").write_text("\n".join(lines) + "\n")


def main():
    geodesic_mesh()
    box_mesh()
    write_ppm("broad_checker.ppm", 32,
              lambda x, y: (225, 215, 185) if (x // 8 + y // 8) % 2 == 0 else (45, 75, 125))
    write_ppm("wood.ppm", 64,
              lambda x, y: tuple(base + 8 * math.sin(2 * math.pi * (x / 16 +
                               .12 * math.sin(2 * math.pi * y / 64)))
                                 for base in (155, 112, 72)))
    write_ppm("paper.ppm", 32,
              lambda x, y: (195, 185, 162) if y % 4 == 0 else (236, 225, 202))
    print("Generated UV geodesic/box OBJ meshes and three P3 textures.")


if __name__ == "__main__":
    main()
