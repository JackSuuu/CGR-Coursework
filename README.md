# CGR26_renderer

A physically based ray tracer written in C++17 from scratch, using only the C++
Standard Library. No external rendering framework, linear algebra package,
parser toolkit or image loading library is used: vectors and matrices, the
PBRT-style scene parser, the PNG/PPM writers and the OBJ/PPM readers are all
implemented in this repository.

The same executable serves both ray tracing approaches required by the
coursework. The `Integrator` directive of the scene file selects which one runs:

| `Integrator` name        | Renderer                          |
|--------------------------|-----------------------------------|
| `whitted`                | Whitted-style ray tracer (WSRT)   |
| `distributed` (`dr`, `drt`) | Distributed ray tracer (DRT)   |

## Development environment

| | |
|---|---|
| Operating system | macOS 27.0.1 |
| Hardware | Apple M2 Pro (arm64) |
| Compiler | Apple clang 21.0.0 (`clang-2100.1.1.101`) |
| Build system | GNU `make` |
| Language standard | C++17 |
| External dependencies | none (C++ Standard Library only) |

## Building

```sh
make                # builds output/CGR26_renderer
make clean          # removes build/ and the executable
make scenes         # builds, then renders every reported image
```

The default flags are:

```
-std=c++17 -O3 -Wall -Wextra -Isource -fno-strict-aliasing
```

All includes are relative to `source/`, so source files include each other as
`"core/vec.h"`, `"scene/shape.h"`, and so on. No `-I` flag other than
`-Isource` is needed.

Alternative compiler or flags:

```sh
make CXX=g++ CXXFLAGS="-O2 -std=c++20 -Wall -Isource"
```

The build produces `output/CGR26_renderer`.

## Running the renderer

```sh
./output/CGR26_renderer <scene.pbrt> [options]
```

The only required argument is the scene file. Every run writes the image and a
matching `<name>.log` into the output directory.

```sh
./output/CGR26_renderer scenes/WSRT_simple.pbrt
./output/CGR26_renderer scenes/DRT_simple.pbrt -o output -n my_render
./output/CGR26_renderer scenes/WSRT_simple.pbrt -f ppm -q
```

Options:

```
-o, --output <dir>     directory for the image and log (default: output)
-n, --name <stem>      output file stem (default: the scene stem)
-f, --format <fmt>     ppm | png (default: png)
-q, --quiet            do not echo the log to stdout
-v, --version          print the version and exit
-h, --help             print this message and exit
```

Output is 8-bit, gamma corrected by default. The `Gamma` setting is controlled
by the scene file, not by a command line switch.

### The log file

Each render writes `output/<name>.log` next to `output/<name>.png`. It records:

* the command line and the scene file being parsed;
* every parse warning and parse error with its line number, followed by a
  `parse warnings=N parse errors=N` summary;
* the scene contents (shapes, lights, materials, integrator) and the resolved
  camera parameters;
* the per-frame render time and the completed `render` profiling stage;
* image statistics: `mean`, `min`, `max` and `stddev` per channel.

A scene that contains a syntax error or an unsupported feature is reported in
the log; the renderer keeps going with whatever it could parse and still writes
an image, rather than crashing or failing silently.

## Rendering all reported images

```sh
./renderReportedImages.sh
```

This builds the project and then renders every image shown in the report into
`output/`, together with its log.

## Scenes

| Scene | Integrator | What it demonstrates |
|---|---|---|
| `scenes/WSRT_simple.pbrt` | `whitted` | mirror reflection, refraction, Blinn-Phong, point lights and hard shadows, PPM textures on spheres, planes and triangles |
| `scenes/WSRT_creative.pbrt` | `whitted` | provisional tabletop optics composition with books, mirrors and glass; real inspiration photograph pending |
| `scenes/DRT_simple.pbrt` | `distributed` | OBJ triangle mesh with smooth normals and UV textures, area lights and soft shadows, Phong BRDF, thin-lens depth of field, Reinhard tone mapping |
| `scenes/DRT_creative.pbrt` | `distributed` | provisional tabletop composition with textured meshes, books, reflective/glass objects and depth of field; real photograph pending |

Textures live in `scenes/textures/`, meshes in `scenes/meshes/`. Relative paths
inside a scene file are resolved against the directory of that scene file, so
the scenes can be launched from any working directory.

### Scene resolutions

`WSRT_*` renders at 900x560 / 960x600. `DRT_simple` renders at 600x400 with
32 lens/light samples per pixel; `DRT_creative` at 480x320 with 24 samples.
This checkpoint intersects every primitive
with a linear scan (the bounding volume hierarchy arrives in Module 3), and the
mesh plus the area-light sampling dominate the run time at higher resolutions.

### Procedural scene assets

```sh
python3 scripts/generateModule1Assets.py
```

This optional regeneration command uses only Python's standard library; the
C++ renderer itself has no Python dependency. The generated OBJ and PPM files
are already included in `scenes/`. `uv_geodesic.obj` has 320 non-overlapping
triangle faces, explicit smooth normals and seam-adjusted UVs. `display_box.obj`
has 12 triangles with face normals and UVs. Broad checker, wood and paper
textures are procedural P3 PPMs. They are not real inspiration photographs.

## Running the student test suite

The student test suite (`studentTests/runAllTests.sh`) is part of the Module 3
checkpoint and is not present in this checkpoint.

Module 1 has small analytic regression checks, separate from that later suite:

```sh
make check-module1
```

These check reflection/refraction directions, Fresnel and TIR, scaled
intersections, inverse-transpose normals, spherical/barycentric UVs, OBJ
normals/transforms, material parsing, focal-plane convergence, Phong energy,
direct illumination/shadows and Reinhard values against analytic expectations.

### Module 1 scope and conventions

* WSRT uses one pixel-centre camera ray, Blinn-Phong direct lighting, and
  recursive ideal reflection/refraction. Recursion termination returns zero.
* DRT uses fixed pixel centres and independent uniform lens/light samples.
  `spp` controls camera/lens samples; an area light's `"integer samples"`
  controls light samples per camera sample (default 1). Both are averaged.
* DRT evaluates direct illumination and ideal mirror/glass recursion. Diffuse
  indirect lighting, BF studies and BRDF importance sampling are later work.
  The legacy BF field is parsed for compatibility but is unused here.
* Directions `wi`/`wo` point away from the hit; `ray.d` points towards it.
  `Reflect(wo,n)` therefore receives `-ray.d` for a reflected ray.
* DRT uses a normalised Phong lobe with specular coefficient `ks` clamped to
  [0,1] and diffuse coefficient `clamp(texturedReflectance)*(1-ks)` per channel.
  The cosine factor is applied once by the integrator. No artificial ambient
  term is added to DRT; WSRT retains its required ambient component.
* Dielectric rays assume an object in air, using the outward geometric normal
  to select entry/exit. Nested media require a medium stack and are unsupported.
* Lights are two-sided. Rectangles use four explicit corners, or a legacy
  two-point square construction; the latter does not uniquely specify a
  rectangle. Non-uniformly scaled spherical emitters are unsupported.
* Reinhard defaults to per-channel extended Reinhard:
  `c*(1+c/W^2)/(1+c)`. At `W=1` this is exactly identity; values beyond the
  white point may exceed 1 and are clipped for display. Luminance mode is
  selectable in the integrator block. sRGB encoding follows tone mapping.
* The parser supports both the supplied named, brace-delimited format and a
  standard PBRT statement subset: Film, Sampler, Integrator, LookAt, Camera,
  WorldBegin/End, Material/MakeNamedMaterial/NamedMaterial, PPM spectrum
  imagemap Texture, point LightSource, diffuse AreaLightSource,
  sphere/inline trianglemesh Shape, Translate/Scale/Rotate, Identity and
  AttributeBegin/End. `tests/fixtures/standard.pbrt` exercises standard syntax.
  Camera placement in this subset uses LookAt; it does not implement PBRT's
  full transform stack for cameras, animation, Includes or general instances.
  Standard PBRT FOV is applied to the smaller axis; the legacy block scenes
  retain their larger-axis FOV convention. Unsupported directives are logged.
  A standard `path` integrator is explicitly warned about and approximated by
  direct-light DRT, not claimed to implement PBRT's complete path integrator.
  Standard diffuse area sources support spherical or four-corner mesh emitters;
  non-rectangular trianglemesh emitters are visible but not explicitly sampled.

The submission still needs the user's real inspiration photographs, image
annotations and `reports/Module1.pdf`. Report writing and AI-use disclosure
are the student's responsibility under the coursework rules.

The technical output index, measured log values and student report checklist
are in [`docs/module1-verification.md`](docs/module1-verification.md). This is
a verification record, not the coursework report.

## Known limitations at this checkpoint

* **No acceleration structure yet.** Every ray is tested against every
  primitive, so scenes with several thousand primitives render slowly.
* **No antialiasing yet.** The WSRT renders exactly one ray through the centre
  of each pixel and shows the usual staircase edges. Supersampling arrives with
  the Module 2 antialiasing task.
* **One sampling strategy.** The DRT draws uniform samples for the
  light surface and lens; the regular grid and
  Halton strategies are added in Module 2.
* **Only `-o`, `-n`, `-f`, `-q`, `-h` and `-v` on the command line.**
  Resolution, sample counts, recursion depth, branching factor, tone mapping
  and the camera are configured in the scene file; command line overrides for
  them are added in Module 2.
* Textures must be 8-bit PPM (`P3` or `P6`) with 3 colour components.
* The OBJ reader handles `v`, `vn`, `vt` and `f`, including `v/vt/vn` corner
  triples and negative (relative) indices. Meshes without `vn` records get
  area-weighted vertex normals recomputed from the faces. Other records (`g`,
  `o`, `s`, `usemtl`, `mtllib`) are ignored.
* Multi-threading is not used, so a frame is rendered on a single core.
* PBRT files that use directives outside the supported subset are reported
  with a warning naming the line, and the run continues.
