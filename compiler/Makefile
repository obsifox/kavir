# Kavir compiler (M1 walking skeleton + M1.1 stabilization) — C17, gcc.
# The compiler itself is bootstrapped with cc (ADR-002 v2 boundary note);
# the PROGRAMS IT EMITS never touch cc/as/ld.
CC ?= gcc

SRC := $(wildcard src/*.c)
HDR := src/kavir.h

CFLAGS ?= -std=c17 -O2 -Wall -Wextra -Werror

all: kavir

kavir: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

# sanitizer build for the fuzz drivers (ADR-005 v3 amendment b)
kavir-asan: $(SRC) $(HDR)
	$(CC) -std=c17 -O1 -g -fsanitize=address,undefined \
	        -fno-sanitize-recover=all -fno-omit-frame-pointer \
	        -Wall -Wextra -o $@ $(SRC)

# libFuzzer harness — REQUIRES clang (gcc does not ship libFuzzer).
kavir-libfuzzer: $(SRC) $(HDR) fuzz/fuzz_frontend.c
	@command -v clang >/dev/null 2>&1 || { \
	        echo "kavir-libfuzzer needs clang (libFuzzer is clang-only);"; \
	        echo "this environment has gcc only — use 'make fuzz' instead."; \
	        exit 1; }
	clang -std=c17 -g -O1 -fsanitize=fuzzer,address,undefined \
	        -fno-sanitize-recover=all -fno-omit-frame-pointer \
	        -Wall -Wextra -o $@ $(SRC) fuzz/fuzz_frontend.c

test: kavir
	sh tests/run.sh

# quick sanitizer fuzz (grammar-aware + mutation driver, gcc-friendly)
fuzz: kavir-asan
	python3 scripts/fuzz.py --iterations 2000 --seconds 60

# longer campaign; override with e.g. make fuzz-long FUZZ_SECS=600
fuzz-long: kavir-asan
	python3 scripts/fuzz.py --iterations 50000 --seconds $(FUZZ_SECS)

# coverage-guided campaign (clang only; corpus dir is gitignored,
# seeded from the fixture corpus on each run)
fuzz-libfuzzer: kavir-libfuzzer
	mkdir -p corpus
	cp tests/fixtures/*.k corpus/ 2>/dev/null || true
	./kavir-libfuzzer corpus -max_len=2048 -detect_leaks=0 \
	        -print_final_stats=1 -max_total_time=60

clean:
	rm -f kavir kavir-asan kavir-libfuzzer

.PHONY: all test fuzz fuzz-long fuzz-libfuzzer clean
