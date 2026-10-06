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
-o, --output <dir>       directory for the image and log (default: output)
-n, --name <stem>        output file stem (default: the scene stem)
-f, --format <fmt>       ppm | png (default: png)
-w, --width <n>          override the film x resolution
-H, --height <n>         override the film y resolution
-s, --spp <n>            override samples per pixel
-d, --depth <n>          override the maximum ray-tree depth
-b, --bf <n>             override the branching factor
    --sampler <name>     uniform | grid | halton | haltonjittered
    --tonemap <name>     reinhard | reinhardluminance
    --white <v>          Reinhard white point
    --aperture <r>       thin-lens aperture radius (0 = pinhole)
    --focusdistance <d>  thin-lens focal distance
    --fov <deg>          override the field of view
-q, --quiet              do not echo the log to stdout
-v, --version            print the version and exit
-h, --help               print this message and exit
```

Command line values win over the scene file, which is how a single scene can
be rendered as a resolution, sample-count or parameter sweep without editing
it.

Output is 8-bit, gamma corrected by default. The `Gamma` setting is controlled
by the scene file, not by a command line switch.

### The log file

Each render writes `output/<name>.log` next to `output/<name>.png`. It records:

* the command line and the scene file being parsed;
* every parse warning and parse error with its line number, followed by a
  `parse warnings=N parse errors=N` summary;
* the scene contents (shapes, lights, materials, integrator) and the resolved
  camera parameters;
* the per-frame render time and a per-stage profile (`total`, `parse`,
  `render`);
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
| `scenes/WSRT_creative.pbrt` | `whitted` | a creative scene built from a photograph, using the same feature set |
| `scenes/DRT_simple.pbrt` | `distributed` | OBJ triangle mesh with smooth normals and UV textures, area lights and soft shadows, Phong BRDF, thin-lens depth of field, Reinhard tone mapping |
| `scenes/DRT_creative.pbrt` | `distributed` | a creative scene using the same feature set |
| `scenes/camera_persp.pbrt` | `whitted` | the reference perspective view, 45 degree field of view |
| `scenes/camera_ortho.pbrt` | `whitted` | the same view through an orthographic camera |
| `scenes/camera_fov1.pbrt` | `whitted` | 20 degree field of view (telephoto) |
| `scenes/camera_fov2.pbrt` | `whitted` | 45 degree field of view |
| `scenes/camera_fov3.pbrt` | `whitted` | 85 degree field of view (wide) |
| `scenes/aa_wsrt.pbrt` | `whitted` | the WSRT with one centre sample per pixel: the aliased image |
| `scenes/aa_drt.pbrt` | `distributed` | the DRT with one centre sample per pixel, pinhole camera |
| `scenes/aa_spp1/4/9/16.pbrt` | `whitted` | the same scene at 1, 4, 9 and 16 rays per pixel |
| `scenes/sampler_unif.pbrt` | `distributed` | uniform random sampling, 16 samples per pixel |
| `scenes/sampler_grid.pbrt` | `distributed` | regular grid sampling, 16 samples per pixel |
| `scenes/sampler_halton.pbrt` | `distributed` | Halton sampling, 16 samples per pixel |
| `scenes/param_small_low.pbrt` | `distributed` | branching factor 1, ray-tree depth 2 |
| `scenes/param_small_high.pbrt` | `distributed` | branching factor 1, ray-tree depth 4 |
| `scenes/param_large_low.pbrt` | `distributed` | branching factor 6, ray-tree depth 2 |
| `scenes/param_large_high.pbrt` | `distributed` | branching factor 6, ray-tree depth 4 |

Every `camera_*` scene contains the same geometry, materials and lights, and
every `sampler_*` scene differs from its siblings in exactly the
one parameter it is named after, so a difference between two images is a
difference in that parameter and nothing else. Each of those scene files says
in its header which lines are allowed to differ and, for the sampler study,
which pixels the report measures.

The four `param_*` scenes are **distinct scenes**, each designed to suit its
BF/MD combination (see featureList.txt for the design rationale). This
follows the spec requirement: "design four scenes, one well suited to each
parameter combination."

Sampler quantitative comparison: each sampler rendered at spp=16 and at
spp=512 (high-sample reference); RMSE at three designated pixels (penumbra,
open floor, near sphere) is recorded in the logs and discussed in the report.

Textures live in `scenes/textures/`, meshes in `scenes/meshes/`. Relative paths
inside a scene file are resolved against the directory of that scene file, so
the scenes can be launched from any working directory.

### Scene resolutions

The reported images are rendered at the resolutions written into the scene
files: `WSRT_*` at 900x560 / 960x600, `DRT_simple` at 450x300, `DRT_creative`
at 360x240, the camera and aliasing studies at 480x480 / 480x360, and the
sampler and branching-factor studies at 320x240. The distributed-ray-tracer
scenes stay modest because this checkpoint still intersects every primitive
with a linear scan (the bounding volume hierarchy arrives in Module 3), and
the mesh plus the area-light sampling dominate the run time at higher
resolutions.

`./renderReportedImages.sh` prints how long each scene takes, which is the
same timing the report quotes.

## Running the student test suite

The student test suite (`studentTests/runAllTests.sh`) is part of the Module 3
checkpoint and is not present in this checkpoint.

## Known limitations at this checkpoint

* **No acceleration structure yet.** Every ray is tested against every
  primitive, so scenes with several thousand primitives render slowly. The
  timing gap that leaves is measured in Module 3.
* **No student test suite yet.** `studentTests/` arrives with Module 3, so
  there is no reference-image comparison to run at this checkpoint.
* **No importance sampling of the Phong specular lobe.** Direct lighting
  samples the light surface; the diffuse lobe is sampled cosine-weighted. A
  BRDF-importance-sampled variant is the final-phase task.
* The samplers are deterministic: the sample stream is derived from the pixel
  and sample index, so a scene renders identically on every run. Variance is
  therefore measured against a high-sample reference image at the pixels named
  in the scene files, rather than across repeated runs.
* Textures must be 8-bit PPM (`P3` or `P6`) with 3 colour components.
* The OBJ reader handles `v`, `vn`, `vt` and `f`, including `v/vt/vn` corner
  triples and negative (relative) indices. Meshes without `vn` records get
  area-weighted vertex normals recomputed from the faces. Other records (`g`,
  `o`, `s`, `usemtl`, `mtllib`) are ignored.
* Multi-threading is not used, so a frame is rendered on a single core.
* PBRT files that use directives outside the supported subset are reported
  with a warning naming the line, and the run continues.
