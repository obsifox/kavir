#!/bin/sh
# LOC accounting with an EXPLICIT definition (M1.1 review item 6;
# M2 additions keep every number's definition next to it).
#
# Definitions (used in all Kavir reports):
#   "C LOC"       = lines in compiler/src/*.c PLUS the single header
#                   compiler/src/kavir.h.
#   "Harness C"   = fuzz/fuzz_frontend.c (libFuzzer harness).
#   "Tooling PY"  = tests/reference.py (independent oracle),
#                   tests/run_m2.py (M2 runner), tests/run_m3.py (M3
#                   runner), scripts/fuzz.py (fuzz driver),
#                   scripts/gen_m3_fixtures.py (fixture generator).
#   "Suite SH"    = tests/run.sh (M1.1 shell suite).
#   Fixtures      = number of .k case files (not counted as LOC).
# None of these are summed into "C LOC"; categories are never mixed.
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
    printf '%8d  Harness C LOC  (fuzz/fuzz_frontend.c)\n' \
        "$(wc -l < fuzz/fuzz_frontend.c)"
fi
printf '%8d  Tooling PY LOC (tests/reference.py — independent oracle)\n' \
    "$(wc -l < tests/reference.py)"
printf '%8d  Tooling PY LOC (tests/run_m2.py — M2 runner)\n' \
    "$(wc -l < tests/run_m2.py)"
printf '%8d  Tooling PY LOC (tests/run_m3.py — M3 runner)\n' \
    "$(wc -l < tests/run_m3.py)"
printf '%8d  Tooling PY LOC (scripts/gen_m3_fixtures.py — M3 fixtures)\n' \
    "$(wc -l < scripts/gen_m3_fixtures.py)"
printf '%8d  Tooling PY LOC (scripts/fuzz.py — fuzz driver)\n' \
    "$(wc -l < scripts/fuzz.py)"
printf '%8d  Suite SH LOC   (tests/run.sh — M1.1 suite)\n' \
    "$(wc -l < tests/run.sh)"
printf '     %4d fixtures  (tests/fixtures/*.k, M1 boundary corpus)\n' \
    "$(ls tests/fixtures/*.k | wc -l)"
printf '     %4d fixtures  (tests/cases_m2/*.k, M2 three-way corpus)\n' \
    "$(ls tests/cases_m2/*.k | wc -l)"
printf '     %4d fixtures  (tests/cases_m3/*.k, M3 three-way corpus)\n' \
    "$(ls tests/cases_m3/*.k | wc -l)"
