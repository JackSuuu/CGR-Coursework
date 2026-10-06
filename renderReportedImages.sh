#!/bin/sh
# renderReportedImages -- builds the renderer and renders every image that is
# shown in the Module 1 report into output/.
#
# Usage:  ./renderReportedImages.sh [output-directory]
#
# Each render also writes a matching <name>.log next to the image; the report
# quotes timings and image statistics taken from those logs.
set -e

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:-$root/output}
scene=$root/scenes

printf 'building the renderer...\n'
make -C "$root"

printf 'rendering the Module 1 reported images into %s\n' "$out"

# WSRT: one scene that puts every WSRT feature on show, one creative scene.
"$root/output/CGR26_renderer" "$scene/WSRT_simple.pbrt"    -o "$out" -n WSRT_simple    -q
"$root/output/CGR26_renderer" "$scene/WSRT_creative.pbrt"  -o "$out" -n WSRT_creative  -q

# DRT: mesh, area lights, Phong BRDF, depth of field and Reinhard tone mapping.
"$root/output/CGR26_renderer" "$scene/DRT_simple.pbrt"     -o "$out" -n DRT_simple     -q
"$root/output/CGR26_renderer" "$scene/DRT_creative.pbrt"   -o "$out" -n DRT_creative   -q

printf 'done. images and logs:\n'
ls -1 "$out" | sed 's/^/  /'
