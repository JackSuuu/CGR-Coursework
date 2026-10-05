#!/bin/sh
# runAllTests - runs every student test case in order and writes a summary log.
#
# Usage:
#   ./runAllTests.sh                 # build if needed, then run test1..test9
#   RENDERER=/path/to/CGR26_renderer ./runAllTests.sh
#   ./runAllTests.sh test3 test7      # run only the named tests
#
# Each test lives in its own numbered subdirectory together with its scene
# file, any referenced data (PPM textures, OBJ meshes), a run.sh with the checks
# and a README.md explaining what the test proves. Nothing outside this
# directory is required, so the suite can be handed to another student.
#
# Images and per-render logs go to <project>/output/studentTests/testN/.
# The transcript is written to studentTests/testResults.log next to this script.
# Exit status is 0 only when every test passes.

set -u

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/.." && pwd)
RENDERER=${RENDERER:-$root/output/CGR26_renderer}
LOG=$here/testResults.log

if [ ! -x "$RENDERER" ]; then
    printf 'building the renderer (make -C %s)\n' "$root"
    if ! make -C "$root" >/dev/null 2>&1 || [ ! -x "$RENDERER" ]; then
        printf 'error: cannot build %s\n' "$RENDERER" >&2
        exit 1
    fi
fi

if [ "$#" -gt 0 ]; then
    tests=$*
else
    # Only the numbered case directories (test1, test2, ...). A plain glob would
    # also match testResults.log, which lives next to this script.
    tests=
    for d in "$here"/test[0-9]*; do
        [ -d "$d" ] || continue
        tests="$tests $(basename "$d")"
    done
fi

printf 'student test suite\n'
printf 'renderer: %s\n' "$RENDERER"
printf 'scenes:   %s\n' "$tests"
printf '\n' | tee "$LOG"

passed=0
failed=0
failedNames=

for t in $tests; do
    dir=$here/$t
    if [ ! -x "$dir/run.sh" ]; then
        printf '%s: SKIP (no run.sh)\n' "$t" | tee -a "$LOG"
        continue
    fi
    printf -- '---- %s ----\n' "$t" | tee -a "$LOG"
    out=$(cd "$dir" && RENDERER="$RENDERER" sh ./run.sh 2>&1)
    status=$?
    printf '%s\n' "$out" | tee -a "$LOG"
    if [ "$status" -eq 0 ]; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
        failedNames="$failedNames $t"
    fi
    printf '\n' | tee -a "$LOG"
done

printf '==================== summary ====================\n' | tee -a "$LOG"
printf 'passed: %d\nfailed: %d\n' "$passed" "$failed" | tee -a "$LOG"
if [ "$failed" -ne 0 ]; then
    printf 'failing:%s\n' "$failedNames" | tee -a "$LOG"
    printf 'full transcript: %s\n' "$LOG"
    exit 1
fi
printf 'all tests passed; full transcript: %s\n' "$LOG"
exit 0
