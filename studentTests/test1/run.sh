#!/bin/sh
# test1 - WSRT: ideal mirror, point light and hard shadows
#
# Usage:   ./run.sh
#          RENDERER=/path/to/CGR26_renderer ./run.sh
#
# Renders scene.pbrt and checks the renderer's own log for the behaviour
# described in README.md. Images and logs are written to
# output/studentTests/test1/ inside the project.
#
# Exit status: 0 = every check passed, 1 = at least one check failed.
set -u

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
RENDERER=${RENDERER:-$root/output/CGR26_renderer}
OUT=${OUT:-$root/output/studentTests/test1}
mkdir -p "$OUT" || exit 1

fail=0
pass() { printf '  ok    %s\n' "$*"; }
bad()  { printf '  FAIL  %s\n' "$*"; fail=1; }

# The renderer prints one summary line per image:
#   image mean=(r, g, b) min=.. max=.. stddev=(r, g, b)
# These helpers read values back out of that line (awk is used instead of sed
# so the scripts behave the same on BSD and GNU sed).
summary() {  # summary <log> -- everything after "image mean="
    awk '/image mean=/{sub(/^.*image mean=/, ""); print; exit}' "$1"
}
mean_of() {  # mean_of <log> <channel 1..3>
    summary "$1" | tr -d '(),' | awk -v i="$2" '{print $i}'
}
stat_of() {  # stat_of <log> <min|max|stddev>
    summary "$1" | tr -d '(),' | awk -v k="$2" '{
        for (i = 1; i <= NF; i++) {
            if ($i == k "=") { print $(i + 1); exit }
            if (index($i, k "=") == 1) { print substr($i, length(k) + 2); exit }
        }
    }'
}
close_to() {  # close_to <label> <actual> <expected> <tolerance>
    if [ -z "$2" ]; then bad "$1: no value found in the log"
    elif awk -v a="$2" -v e="$3" -v t="$4" 'BEGIN{d=a-e; if(d<0)d=-d; exit !(d<=t)}'; then
        pass "$1 ($2 vs $3 +/- $4)"
    else bad "$1: $2 differs from the expected $3 by more than $4"; fi
}
above() {  # above <label> <actual> <threshold>
    if [ -z "$2" ]; then bad "$1: no value found in the log"
    elif awk -v a="$2" -v t="$3" 'BEGIN{exit !(a>t)}'; then pass "$1 ($2 > $3)"
    else bad "$1: $2 is not above the required $3"; fi
}
below() {  # below <label> <actual> <threshold>
    if [ -z "$2" ]; then bad "$1: no value found in the log"
    elif awk -v a="$2" -v t="$3" 'BEGIN{exit !(a<t)}'; then pass "$1 ($2 < $3)"
    else bad "$1: $2 is not below the required $3"; fi
}
in_range() {  # in_range <label> <actual> <low> <high>
    if [ -z "$2" ]; then bad "$1: no value found in the log"
    elif awk -v a="$2" -v lo="$3" -v hi="$4" 'BEGIN{exit !(a>=lo && a<=hi)}'; then
        pass "$1 ($2 in [$3, $4])"
    else bad "$1: $2 is outside the expected range [$3, $4]"; fi
}
abs_diff() {  # abs_diff <a> <b>
    awk -v a="$1" -v b="$2" 'BEGIN{d=a-b; print (d<0 ? -d : d)}'
}
has_log() {  # has_log <label> <log> <fixed string>
    if grep -q -- "$3" "$2"; then pass "$1"
    else bad "$1: log does not mention '$3'"; fi
}
no_errors() {  # no_errors <label> <log>
    if grep -q '\[ERROR' "$2"; then
        bad "$1: unexpected error lines"
        grep '\[ERROR' "$2" | sed 's/^/        /'
    else pass "$1"; fi
}
no_log() {  # no_log <label> <log>
    if grep -q '\[' "$2"; then bad "$1: unexpected log records"
    else pass "$1"; fi
}

if [ ! -x "$RENDERER" ]; then
    printf 'renderer not found or not executable: %s\n' "$RENDERER" >&2
    printf 'build it first:  make -C %s\n' "$root" >&2
    exit 1
fi

printf 'test1: chrome sphere, two point lights, shadow-ray offset\n'

"$RENDERER" "$here/scene.pbrt" -o "$OUT" -n test1 -q || bad "renderer exited with $?"
log="$OUT/test1.log"
if [ ! -f "$log" ]; then bad "no log written to $log"; printf 'test1: FAIL\n'; exit 1; fi

no_errors "scene parses without errors" "$log"
has_log "whitted integrator selected" "$log" "integrator: whitted"
has_log "single sample per pixel (no antialiasing)" "$log" "spp=1"
has_log "point light intensity read from the scene file" "$log" "I=(42, 40, 38)"
has_log "second point light" "$log" "I=(10, 10, 12)"
has_log "mirror recursion depth" "$log" "maxDepth=8"
in_range "image mean" "$(mean_of "$log" 1)" 0.08 0.20
above "lit floor and sphere are visible" "$(stat_of "$log" max)" 0.25
below "background pixels stay near the background value" "$(stat_of "$log" min)" 0.05
above "shading produces contrast (lit plus shadow)" "$(stat_of "$log" stddev)" 0.08

if [ "$fail" -eq 0 ]; then
    printf 'test1: PASS\n'
else
    printf 'test1: FAIL\n'
fi
exit "$fail"
