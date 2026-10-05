#!/bin/sh
# test6 - DRT: thin-lens defocus and depth of field
#
# Usage:   ./run.sh
#          RENDERER=/path/to/CGR26_renderer ./run.sh
#
# Renders scene.pbrt and checks the renderer's own log for the behaviour
# described in README.md. Images and logs are written to
# output/studentTests/test6/ inside the project.
#
# Exit status: 0 = every check passed, 1 = at least one check failed.
set -u

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
RENDERER=${RENDERER:-$root/output/CGR26_renderer}
OUT=${OUT:-$root/output/studentTests/test6}
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

printf 'test6: closed aperture against a strongly defocused lens\n'

"$RENDERER" "$here/scene.pbrt" -o "$OUT" -n test6_scene -q \
    || bad "scene-file lens render exited with $?"
"$RENDERER" "$here/scene.pbrt" -o "$OUT" -n test6_pinhole -q --aperture 0 \
    || bad "pinhole render exited with $?"
"$RENDERER" "$here/scene.pbrt" -o "$OUT" -n test6_defocus -q --aperture 0.35 --focusdistance 12 \
    || bad "defocused render exited with $?"
s="$OUT/test6_scene.log"
p="$OUT/test6_pinhole.log"
d="$OUT/test6_defocus.log"
for f in "$s" "$p" "$d"; do
    [ -f "$f" ] || { bad "missing log $f"; printf 'test6: FAIL\n'; exit 1; }
done

no_errors "DRT scene parses without errors" "$s"
has_log "aperture radius taken from the scene file" "$s" "lensRadius=0.120000"
has_log "focal distance taken from the scene file" "$s" "focus=6.200000"
has_log "closed aperture override" "$p" "lensRadius=0.000000"
has_log "wide aperture override" "$d" "lensRadius=0.350000"
has_log "focus override" "$d" "focus=12.000000"

in_range "defocused render is lit" "$(mean_of "$d" 1)" 0.08 0.25
# Defocus moves energy from the in-focus detail onto the surroundings, so the
# mean rises and the contrast of the frame falls.
above "defocus raises the mean" \
      "$(abs_diff "$(mean_of "$d" 1)" "$(mean_of "$p" 1)")" 0.002
sd_p=$(stat_of "$p" stddev)
sd_d=$(stat_of "$d" stddev)
if [ -n "$sd_p" ] && [ -n "$sd_d" ] && \
   awk -v a="$sd_d" -v b="$sd_p" 'BEGIN{{exit !(a < b - 0.002)}}'; then
    pass "defocus lowers the contrast ($sd_d < $sd_p)"
else
    bad "defocus did not lower the contrast ($sd_d vs $sd_p)"
fi

if [ "$fail" -eq 0 ]; then
    printf 'test6: PASS\n'
else
    printf 'test6: FAIL\n'
fi
exit "$fail"
