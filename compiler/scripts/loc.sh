#!/bin/sh
# LOC accounting with an EXPLICIT definition (M1.1 review item 6).
#
# Definition (used in all Kavir reports):
#   "C LOC" = lines in compiler/src/*.c PLUS the single header
#             compiler/src/kavir.h.
#   Test harness (tests/run.sh, shell) and fuzz scripts (scripts/*.py,
#   Python; fuzz/fuzz_frontend.c counts as C when present) are reported
#   separately and are NOT part of "C LOC".
# Usage: sh scripts/loc.sh
cd "$(dirname "$0")/.."

c_files=$(ls src/*.c)
c_loc=$(cat $c_files src/kavir.h | wc -l)
c_only=$(cat $c_files | wc -l)
h_loc=$(wc -l < src/kavir.h)

echo "== C LOC (definition: src/*.c + src/kavir.h) =="
for f in $c_files; do printf '%8d  %s\n' "$(wc -l < $f)" "$f"; done
printf '%8d  src/kavir.h (the single header)\n' "$h_loc"
echo "-----------------------------------------"
printf '%8d  TOTAL C LOC  (%d .c + %d header)\n' "$c_loc" "$c_only" "$h_loc"

if [ -f fuzz/fuzz_frontend.c ]; then
    printf '%8d  fuzz/fuzz_frontend.c (C, reported separately)\n' \
        "$(wc -l < fuzz/fuzz_frontend.c)"
fi
printf '%8d  tests/run.sh (shell, reported separately)\n' "$(wc -l < tests/run.sh)"
printf '%8d  scripts/fuzz.py (python, reported separately)\n' "$(wc -l < scripts/fuzz.py)"
