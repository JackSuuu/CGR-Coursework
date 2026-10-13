"""Build the photo-inspired cafe scene and procedural OBJ/PPM assets.

Reference: scenes/photograph/cgr_ref_img.jpg (user-provided, not generated).
The renderer still uses only C++ standard-library OBJ/PPM loading.
Run: python3 scripts/generateCafeCreative.py [--preview]
Preview writes additional low-resolution scenes, leaving final scenes intact.
"""
import argparse
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENES = ROOT / "scenes"


def unit(v):
    length = math.sqrt(sum(c * c for c in v))
    return tuple(c / length for c in v)


def lathe(name, profile, segments=12, caps=True):
    """Closed rotational surface with explicit normals/UVs; local axis is +y."""
    vertices, normals, uvs, faces = [], [], [], []
    count = segments + 1
    for j, (height, radius) in enumerate(profile):
        j0, j1 = max(0, j - 1), min(len(profile) - 1, j + 1)
        dh = profile[j1][0] - profile[j0][0]
        dr = profile[j1][1] - profile[j0][1]
        for i in range(count):
            angle = 2 * math.pi * i / segments
            ca, sa = math.cos(angle), math.sin(angle)
            vertices.append((radius * ca, height, radius * sa))
            normals.append(unit((dh * ca, -dr, dh * sa)))
            uvs.append((i / segments, j / (len(profile) - 1)))
    for j in range(len(profile) - 1):
        for i in range(segments):
            a, b = j * count + i, j * count + i + 1
            c, d = (j + 1) * count + i, (j + 1) * count + i + 1
            faces.extend([(a, c, b), (b, c, d)])
    if caps:
        for top in (False, True):
            ring = (len(profile) - 1) * count if top else 0
            center = len(vertices)
            vertices.append((0, profile[-1 if top else 0][0], 0))
            normals.append((0, 1 if top else -1, 0))
            uvs.append((.5, 1 if top else 0))
            # Separate cap vertices retain flat normals at the rim.
            start = len(vertices)
            for i in range(count):
                vertices.append(vertices[ring + i])
                normals.append((0, 1 if top else -1, 0))
                uvs.append((.5, 1 if top else 0))
            for i in range(segments):
                faces.append((center, start + i + 1, start + i) if top
                             else (center, start + i, start + i + 1))
    write_obj(name, vertices, normals, uvs, faces)


def rack_ring():
    vertices, normals, uvs, faces = [], [], [], []
    around, sides = 20, 4
    rx, rz, tube = .9, .58, .025
    for i in range(around + 1):
        angle = 2 * math.pi * i / around
        ca, sa = math.cos(angle), math.sin(angle)
        outward = unit((ca / rx, 0, sa / rz))
        for j in range(sides + 1):
            a = 2 * math.pi * j / sides
            cp, sp = math.cos(a), math.sin(a)
            n = (outward[0] * cp, sp, outward[2] * cp)
            vertices.append((rx * ca + tube * n[0], tube * n[1], rz * sa + tube * n[2]))
            normals.append(n)
            uvs.append((i / around, j / sides))
    stride = sides + 1
    for i in range(around):
        for j in range(sides):
            a = i * stride + j
            b, c, d = a + stride, a + 1, a + stride + 1
            faces.extend([(a, c, b), (b, c, d)])
    write_obj("cafe_rack_ring.obj", vertices, normals, uvs, faces)


def write_obj(name, vertices, normals, uvs, faces):
    lines = ["# Procedural cafe asset: local vertices, explicit normals and UVs."]
    lines.extend("v %.8f %.8f %.8f" % v for v in vertices)
    lines.extend("vt %.8f %.8f" % uv for uv in uvs)
    lines.extend("vn %.8f %.8f %.8f" % n for n in normals)
    lines.extend("f " + " ".join(f"{i+1}/{i+1}/{i+1}" for i in face) for face in faces)
    (SCENES / "meshes" / name).write_text("\n".join(lines) + "\n")


def texture(name, width, height, colour):
    lines = ["P3", f"{width} {height}", "255"]
    for y in range(height):
        lines.append(" ".join(" ".join(str(max(0, min(255, round(c)))) for c in colour(x, y))
                              for x in range(width)))
    (SCENES / "textures" / name).write_text("\n".join(lines) + "\n")


def screen_pixel(x, y):
    if y < 5:
        return (35, 55, 100)
    if x < 28:
        if 12 <= y <= 55 and y % 8 in (1, 2):
            return (150, 162, 183)
        return (199, 207, 219)
    if y < 10:
        return (227, 230, 236)
    if y % 9 in (1, 2) and 38 <= x <= 112 - (y % 3) * 10:
        return (86, 97, 117)
    if 38 <= x <= 104 and 37 <= y <= 46:
        return (197, 210, 230)
    return (237, 239, 241)


def keyboard_pixel(x, y):
    if y >= 51:
        return (27, 28, 32) if 24 < x < 101 and 54 < y < 62 else (72, 65, 57)
    cellx, celly = x % 9, y % 10
    if 1 <= cellx <= 7 and 1 <= celly <= 8:
        return (190, 195, 201) if cellx in (3, 4) and celly in (3, 4) else (24, 25, 29)
    return (73, 65, 57)


def assets():
    lathe("cafe_bottle.obj", [(0, .23), (.10, .26), (.95, .26),
                             (1.15, .21), (1.25, .095), (1.45, .095)])
    lathe("cafe_rod.obj", [(0, 1), (1, 1)], segments=8)
    lathe("cafe_label_band.obj", [(0, 1), (1, 1)], caps=False)
    rack_ring()
    texture("cafe_marble.ppm", 128, 128,
            lambda x, y: tuple(base - 25 * math.exp(-((math.sin(2 * math.pi *
                              (x / 128 + .13 * math.sin(2 * math.pi * y / 128)))) / .11) ** 2)
                              for base in (229, 210, 173)))
    texture("laptop_screen.ppm", 128, 80, screen_pixel)
    texture("laptop_keys.ppm", 128, 64, keyboard_pixel)
    texture("cafe_label.ppm", 48, 32,
            lambda x, y: (35, 61, 100) if 8 < y < 18 else
            ((150, 37, 28) if 24 < y < 29 else (229, 218, 186)))


def scene(distributed=False, preview=False):
    lines = ["# Photo-inspired creative cafe scene.",
             "# Reference: photograph/cgr_ref_img.jpg (user-provided original).",
             "# Laptop, marble inset, chrome condiment rack, capped bottles and green chairs.",
             "# Geometry/labels are stylised; screen texture is procedural, not a photo crop.",
             "# Broad polished tray is an artistic addition to make bottle reflections visible.",
             "# WSRT uses point lights; DRT uses area lights and independent lens/light samples."]
    integrator = "distributed" if distributed else "whitted"
    width, height = ((480, 320) if distributed else (960, 640))
    spp = 96
    if preview:
        width, height, spp = 240, 160, 4
    lines += [f'Integrator "{integrator}" {{ "integer maxdepth" {5 if distributed else 10}' +
              (f' "integer spp" {spp} "float whitepoint" 8' if distributed else '') + ' }',
              f'Film "cafe" {{ "integer xresolution" {width} "integer yresolution" {height} }}',
              'Camera "cam" { "lookat" 4.4 4.2 8.2  -0.1 0.9 -0.05  0 1 0 "float fov" 46' +
              (' "float apertureradius" 0.14 "float focusdistance" 10.0' if distributed else '') + ' }']

    def material(name, colour, specular=(.1, .1, .1), exponent=50, tex=None,
                 kind=None, emission=None):
        shader = kind or ("phong" if distributed else "blinnphong")
        fields = [f'"{shader}"', '"reflectance" [ ' + ' '.join(map(str, colour)) + ' ]']
        if shader not in ("mirror", "dielectric"):
            fields += ['"specularreflectance" [ ' + ' '.join(map(str, specular)) + ' ]',
                       f'"{("float phong" if distributed else "shininess")}" {exponent}']
        if shader == "dielectric":
            fields.append('"float eta" 1.5')
        if tex:
            fields.append(f'"texture" "diffuse" "textures/{tex}"')
        if emission:
            fields.append('"emission" [ ' + ' '.join(map(str, emission)) + ' ]')
        lines.append(f'Material "{name}" {{ ' + ' '.join(fields) + ' }')

    material("marble", (.95, .9, .8), (.05, .05, .05), 60, "cafe_marble.ppm")
    material("walnut", (.6, .38, .22), (.08, .08, .08), 35, "wood.ppm")
    material("aluminium", (.54, .44, .33), (.3, .3, .3), 100)
    material("bezel", (.025, .028, .035), (.12, .12, .12), 90)
    material("display", (1, 1, 1), (.025, .025, .025), 30, "laptop_screen.ppm", emission=(.12, .15, .2))
    material("keyboard", (.9, .9, .9), (.08, .08, .08), 50, "laptop_keys.ppm")
    material("chrome", (.91, .92, .93), kind="mirror")
    material("amberGlass", (.99, .96, .88), kind="dielectric")
    material("brownSauce", (.28, .045, .022), (.15, .15, .15), 70)
    material("mustard", (.7, .43, .06), (.12, .12, .12), 60)
    material("cap", (.96, .9, .72), (.12, .12, .12), 45)
    material("label", (.9, .9, .9), (.015, .015, .015), 20, "cafe_label.ppm")
    material("upholstery", (.22, .31, .075), (.04, .04, .04), 20)
    material("wall", (.3, .17, .085), (.01, .01, .01), 10)

    def mesh(name, file, mat, transform):
        lines.append(f'Shape "{name}" {{ "trianglemesh" "meshes/{file}" {transform} "material" "{mat}" }}')

    def box(name, mat, pos, scale, rotation=""):
        mesh(name, "display_box.obj", mat, '"translate" ' + ' '.join(map(str, pos)) +
             (' ' + rotation if rotation else '') + ' "scale" ' + ' '.join(map(str, scale)))

    def quad(name, mat, transform, width, height):
        points = [(-width / 2, 0, 0), (width / 2, 0, 0),
                  (width / 2, height, 0), (-width / 2, height, 0)]
        uv = [(0, 1), (1, 1), (1, 0), (0, 0)]
        for suffix, face in (("A", (0, 1, 2)), ("B", (0, 2, 3))):
            p = ' '.join(str(c) for i in face for c in points[i])
            t = ' '.join(str(c) for i in face for c in uv[i])
            lines.append(f'Shape "{name}{suffix}" {{ "triangle" [ {p} ] {transform} "uv" [ {t} ] "material" "{mat}" }}')

    box("tableFrame", "walnut", (0, -.18, 0), (9.0, .16, 6.0))
    box("marbleInset", "marble", (0, -.15, 0), (8.5, .15, 5.5))
    lines += ['Shape "floor" { "plane" [0 0 0] "translate" 0 -1.2 0 "rotate" 1 0 0 -90 "material" "walnut" }',
              'Shape "wall" { "plane" [0 0 0] "translate" 0 0 -7 "material" "wall" }']

    box("laptopBase", "aluminium", (1.1, .05, .2), (3.75, .14, 2.45))
    box("screenFrame", "bezel", (1.1, .24, -.95), (3.75, 2.5, .10), '"rotate" 1 0 0 -12')
    quad("screen", "display", '"translate" 1.1 0.45 -0.875 "rotate" 1 0 0 -12', 3.35, 2.12)
    quad("keys", "keyboard", '"translate" 1.1 0.195 0.20 "rotate" 1 0 0 -90', 3.2, 1.04)
    box("trackpadBorder", "bezel", (1.1, .191, .775), (1.46, .005, .73))
    quad("trackpad", "aluminium", '"translate" 1.1 0.198 1.10 "rotate" 1 0 0 -90', 1.40, .65)

    # Unlike the thin rack wires, this broad horizontal mirror shows recognisable
    # bottle/label reflections. Its front/right extent accommodates the reflected
    # silhouettes from the existing camera, without overlapping the laptop.
    box("trayBase", "bezel", (-2.65, .015, .80), (3.28, .09, 2.92))
    quad("polishedTray", "chrome", '"translate" -2.65 0.115 2.22 "rotate" 1 0 0 -90', 3.20, 2.84)
    mesh("rackLower", "cafe_rack_ring.obj", "chrome", '"translate" -2.6 0.14 0.25')
    mesh("rackUpper", "cafe_rack_ring.obj", "chrome", '"translate" -2.6 0.70 0.25')
    for i, (x, z) in enumerate(((-3.35, .10), (-1.85, .10), (-3.0, .70), (-2.2, .70))):
        mesh(f"rackPost{i}", "cafe_rod.obj", "chrome",
             f'"translate" {x} 0.10 {z} "scale" 0.023 0.84 0.023')
    # Small rods evoke the raised handle in the photograph.
    for i, x in enumerate((-3.42, -1.78)):
        mesh(f"handlePost{i}", "cafe_rod.obj", "chrome",
             f'"translate" {x} 0.15 -0.12 "scale" 0.024 1.35 0.024')
    mesh("rackHandle", "cafe_rod.obj", "chrome", '"translate" -3.42 1.50 -0.12 "rotate" 0 0 1 -90 "scale" 0.024 1.64 0.024')

    for name, mat, x, z, scale, sy in (
            ("amberBottle", "amberGlass", -2.88, -.02, .85, 1.0),
            ("sauceBottle", "brownSauce", -2.26, .05, .9, .82),
            ("mustardBottle", "mustard", -2.58, .57, .8, .67)):
        mesh(name, "cafe_bottle.obj", mat, f'"translate" {x} 0.15 {z} "scale" {scale} {sy} {scale}')
        mesh(name + "Cap", "cafe_rod.obj", "cap",
             f'"translate" {x} {0.15 + 1.40 * sy} {z} "scale" {0.112*scale} {0.19*sy} {0.112*scale}')
        mesh(name + "Label", "cafe_label_band.obj", "label",
             f'"translate" {x} {0.15 + .36*sy} {z} "scale" {.267*scale} {.43*sy} {.267*scale}')

    for name, x, z in (("leftChair", -2.8, -3.8), ("rightChair", 2.8, -4.1)):
        lines.append(f'Shape "{name}Back" {{ "sphere" [1] "translate" {x} 0.70 {z} "scale" 1.05 0.72 0.16 "material" "upholstery" }}')
        lines.append(f'Shape "{name}Seat" {{ "sphere" [1] "translate" {x} -0.45 {z+.4} "scale" 1.05 0.12 0.7 "material" "upholstery" }}')

    if distributed:
        lines += ['Light "warmCeiling" { "rectangle" [-4.5 6 -1  2.5 6 -1  2.5 6 3.2  -4.5 6 3.2] "radiance" [10 8.3 5.7] }',
                  'Light "coolFill" { "rectangle" [5 2 -1  5 5 -1  5 5 2  5 2 2] "radiance" [3 4 6] }']
    else:
        lines += ['Light "warmCeiling" { "point" [-3 7 4] "intensity" [180 145 105] }',
                  'Light "coolFill" { "point" [5 4 1] "intensity" [45 58 80] }']
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preview", action="store_true")
    args = parser.parse_args()
    assets()
    for distributed, name in ((False, "WSRT_creative"), (True, "DRT_creative")):
        suffix = "_preview" if args.preview else ""
        (SCENES / f"{name}{suffix}.pbrt").write_text(scene(distributed, args.preview))
    print("Generated cafe creative scenes inspired by photograph/cgr_ref_img.jpg.")


if __name__ == "__main__":
    main()
