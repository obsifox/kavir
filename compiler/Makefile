# Kavir compiler (M2) -- C17, gcc.
# The compiler itself is bootstrapped with cc (ADR-002 v2 boundary note);
# the PROGRAMS IT EMITS never touch cc/as/ld.
CC ?= gcc

SRC := $(wildcard src/*.c)
HDR := src/kavir.h

CFLAGS ?= -std=c17 -O2 -Wall -Wextra -Werror

# M1.2 fix (verification finding F1): the libFuzzer harness must NOT link
# the CLI driver (main.c defines main, which collides with libFuzzer's
# own main). It only needs the pipeline stages.
FUZZ_SRC := $(filter-out src/main.c,$(SRC))

all: kavir

kavir: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

# sanitizer build for the fuzz drivers (ADR-005 v3 amendment b)
kavir-asan: $(SRC) $(HDR)
	$(CC) -std=c17 -O1 -g -fsanitize=address,undefined \
		-fno-sanitize-recover=all -fno-omit-frame-pointer \
		-Wall -Wextra -o $@ $(SRC)

# libFuzzer harness -- REQUIRES clang (gcc does not ship libFuzzer).
kavir-libfuzzer: $(SRC) $(HDR) fuzz/fuzz_frontend.c
	@command -v clang >/dev/null 2>&1 || { \
		echo "kavir-libfuzzer needs clang (libFuzzer is clang-only);"; \
		echo "this environment has gcc only -- use 'make fuzz' instead."; \
		exit 1; }
	clang -std=c17 -g -O1 -fsanitize=fuzzer,address,undefined \
		-fno-sanitize-recover=all -fno-omit-frame-pointer \
		-Wall -Wextra -o $@ $(FUZZ_SRC) fuzz/fuzz_frontend.c

test: kavir
	sh tests/run.sh
	python3 tests/run_m2.py
	python3 tests/run_m3.py
	python3 tests/run_m31.py
	python3 tests/run_m4.py
	python3 tests/run_m4_elf.py

# quick sanitizer fuzz (grammar-aware + mutation driver, gcc-friendly)
fuzz: kavir-asan
	python3 scripts/fuzz.py --mode asan --iterations 2000 --seconds 60

# three-way differential fuzz against the independent oracle (fast build)
fuzz-diff: kavir
	python3 scripts/fuzz.py --mode diff --iterations 20000 --seconds 600

# longer sanitizer campaign; override with e.g. make fuzz-long FUZZ_SECS=600
fuzz-long: kavir-asan
	python3 scripts/fuzz.py --mode asan --iterations 50000 --seconds $(FUZZ_SECS)

# coverage-guided campaign (clang only; corpus dir is gitignored,
# seeded from the fixture corpus on each run)
# M4.1: ASAN_OPTIONS keeps LSan's exit-time check from flagging the
# pipeline's process-lifetime pools (measured: 1 x 256KB pool at 2.5k
# execs, 41 allocs / 10.7MiB at 407k execs, RSS flat ~144MB -- bounded
# pools, not per-input growth; see reports/milestones/M4.1.md).
fuzz-libfuzzer: kavir-libfuzzer
	mkdir -p corpus
	cp tests/fixtures/*.k corpus/ 2>/dev/null || true
	cp tests/cases_m2/*.k corpus/ 2>/dev/null || true
	cp tests/cases_m3/*.k corpus/ 2>/dev/null || true
	ASAN_OPTIONS=detect_leaks=0 ./kavir-libfuzzer corpus -max_len=2048 \
		-detect_leaks=0 -print_final_stats=1 -max_total_time=60 \
		-timeout=10

clean:
	rm -f kavir kavir-asan kavir-libfuzzer

.PHONY: all test fuzz fuzz-diff fuzz-long fuzz-libfuzzer clean
