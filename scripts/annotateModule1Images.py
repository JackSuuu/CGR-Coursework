"""Create separate report diagrams from the four Module 1 rendered PNGs.

Optional presentation utility: Python + Pillow; never used by the C++ renderer.
Coordinates refer to the current scenes, with a top-left image origin.
Original PNGs/logs are preserved. Re-run after rendering the reported scenes.
Run: python3 scripts/annotateModule1Images.py [--output-dir output] [--font FONT]
"""
import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
COLOURS = ("#38bdf8", "#fbbf24", "#e879f9", "#4ade80",
           "#fb923c", "#fb7185", "#a78bfa")

# ID, feature label, label origin, ellipse bounds, arrow polyline.
FIGURES = {
    "WSRT_simple": ((900, 560), [
        ("A", "Mirror reflection", (28, 28), (207, 229, 324, 351), [(152, 65), (238, 232)]),
        ("B", "Refraction", (330, 28), (377, 221, 518, 364), [(423, 65), (445, 224)]),
        ("C", "Blinn-Phong highlights\n2 point lights", (565, 28), (680, 282, 756, 331), [(718, 90), (718, 284)]),
        ("D", "Sphere UV", (254, 414), (263, 268, 385, 390), [(321, 414), (321, 388)]),
        ("E", "Hard shadow", (625, 441), (746, 333, 857, 372), [(736, 441), (801, 370)]),
        ("F", "Triangle geometry", (16, 395), (73, 262, 249, 378), [(92, 395), (112, 367)]),
        ("G", "Plane UV", (382, 510), (426, 421, 570, 495), [(469, 510), (490, 494)]),
    ], ""),
    "WSRT_creative": ((960, 640), [
        ("A", "Mirror reflection", (28, 40), (164, 379, 336, 484), [(75, 82), (75, 428), (165, 428)]),
        ("B", "Glass refraction", (28, 132), (207, 216, 256, 317), [(141, 172), (217, 231)]),
        ("C", "Screen UV texture", (620, 38), (500, 126, 854, 354), [(734, 80), (742, 139)]),
        ("D", "Hard shadow", (695, 535), (909, 366, 959, 466), [(857, 554), (931, 462)]),
    ], ""),
    "DRT_simple": ((600, 400), [
        ("A", "OBJ: UVs + normals", (12, 12), (131, 112, 301, 291), [(111, 39), (185, 117)]),
        ("B", "Mesh UV / defocus", (242, 12), (346, 126, 402, 250), [(332, 39), (369, 127)]),
        ("C", "Phong highlight", (420, 55), (391, 145, 461, 201), [(490, 82), (444, 149)]),
        ("D", "Soft shadow", (383, 343), (407, 280, 533, 324), [(455, 343), (473, 321)]),
        ("E", "Defocus blur", (182, 344), (69, 265, 174, 370), [(197, 354), (164, 335)]),
    ], "Reinhard: per-channel, W=8   |   spp=128, A=0.18, D=9"),
    "DRT_creative": ((480, 320), [
        ("A", "Mirror reflection", (12, 10), (81, 188, 166, 242), [(43, 36), (43, 211), (82, 211)]),
        ("B", "Soft shadow", (329, 267), (451, 186, 479, 243), [(413, 267), (463, 240)]),
        ("C", "Mesh UV textures", (269, 10), (230, 171, 401, 216), [(439, 35), (443, 166), (398, 185)]),
        ("D", "Defocus blur", (12, 70), (180, 77, 235, 133), [(122, 83), (184, 105)]),
    ], "Reinhard: per-channel, W=8   |   spp=96, A=0.14, D=10"),
}


def font(size, override=None):
    candidates = ([override] if override else []) + [
        "DejaVuSans.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
    ]
    for candidate in candidates:
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            pass
    raise RuntimeError("No scalable font found; provide --font /path/to/font.ttf")


def annotate(name, spec, output, font_path):
    expected_size, marks, footer = spec
    source = output / f"{name}.png"
    with Image.open(source) as original:
        if original.size != expected_size:
            raise ValueError(f"{source}: expected {expected_size}, got {original.size}; update annotation coordinates")
        scale = 1440 / original.width
        # Nearest-neighbour display enlargement preserves the original rendered
        # pixel blocks, including existing noise/blur/aliasing. It adds no detail.
        image = original.convert("RGB").resize(
            (1440, round(original.height * scale)), Image.Resampling.NEAREST)
    if footer:
        canvas = Image.new("RGB", (image.width, image.height + 62), "#172033")
        canvas.paste(image)
        image = canvas
    draw = ImageDraw.Draw(image)
    face = font(36, font_path)

    def point(p):
        return tuple(round(c * scale) for c in p)

    def arrow(points, colour):
        pts = [point(p) for p in points]
        draw.line(pts, fill="#101827", width=8, joint="curve")
        draw.line(pts, fill=colour, width=4, joint="curve")
        dx, dy = pts[-1][0] - pts[-2][0], pts[-1][1] - pts[-2][1]
        length = math.hypot(dx, dy)
        ux, uy = dx / length, dy / length
        tip = pts[-1]
        base = (tip[0] - 20 * ux, tip[1] - 20 * uy)
        draw.polygon([tip, (base[0] - 9 * uy, base[1] + 9 * ux),
                      (base[0] + 9 * uy, base[1] - 9 * ux)], fill=colour)

    # Thin unfilled region circles and routed arrows; labels never replace the
    # selected evidence pixels. Short labels identify features, not conclusions.
    for index, (identifier, text, origin, bounds, route) in enumerate(marks):
        colour = COLOURS[index]
        region = tuple(round(c * scale) for c in bounds)
        draw.ellipse(region, outline="#101827", width=7)
        draw.ellipse(region, outline=colour, width=3)
        arrow(route, colour)
        x, y = point(origin)
        label = f"{identifier}  {text}"
        bbox = draw.multiline_textbbox((0, 0), label, font=face, spacing=5)
        w, h = bbox[2] - bbox[0] + 26, bbox[3] - bbox[1] + 22
        if x + w > image.width or y + h > image.height:
            raise ValueError(f"{name} label {identifier} is outside the canvas")
        draw.rounded_rectangle((x, y, x + w, y + h), radius=10,
                               fill="#172033", outline=colour, width=3)
        draw.multiline_text((x + 13, y + 11 - bbox[1]), label, font=face,
                            fill="white", spacing=5)
    if footer:
        draw.text((24, image.height - 48), footer, font=font(32, font_path), fill="white")
    target = output / f"{name}_annotated.png"
    image.save(target)
    print(f"Annotated {name}: {target.relative_to(output)} ({image.width}x{image.height})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "output")
    parser.add_argument("--font")
    args = parser.parse_args()
    for name, spec in FIGURES.items():
        annotate(name, spec, args.output_dir, args.font)


if __name__ == "__main__":
    main()
