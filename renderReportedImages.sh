#!/bin/sh
# renderReportedImages -- builds the renderer and renders every image that is
# shown in the Module 1 and Module 2 reports into output/.
#
# Usage:  ./renderReportedImages.sh [output-directory]
#
# Each render also writes a matching <name>.log next to the image; the report
# quotes the frame times, camera settings, image statistics and parse summaries
# taken from those logs.
set -e

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:-$root/output}
scene=$root/scenes
bin=$root/output/CGR26_renderer

printf 'building the renderer...\n'
make -C "$root"

printf 'rendering the reported images into %s\n' "$out"

render() {
    # render <scene stem>
    printf '  %-20s' "$1"
    start=$(date +%s)
    "$bin" "$scene/$1.pbrt" -o "$out" -n "$1" -q
    end=$(date +%s)
    printf ' %3ss\n' "$((end - start))"
}

# ---- Module 1: the two core renderers -------------------------------------
render WSRT_simple
render WSRT_creative
render DRT_simple
render DRT_creative

# ---- Module 2.1: perspective vs orthographic, and the field of view ------
render camera_persp
render camera_ortho
render camera_fov1
render camera_fov2
render camera_fov3

# ---- Module 2.2: aliasing and supersampling ------------------------------
render aa_wsrt
render aa_drt
render aa_spp1
render aa_spp4
render aa_spp9
render aa_spp16

# ---- Module 2.3: sampling strategies -------------------------------------
render sampler_unif
render sampler_grid
render sampler_halton

# ---- Module 2.4: branching factor and ray-tree depth ---------------------
render param_small_low
render param_small_high
render param_large_low
render param_large_high

printf 'done. images and logs:\n'
ls -1 "$out" | sed 's/^/  /'
