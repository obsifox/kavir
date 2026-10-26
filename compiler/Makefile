# Kavir compiler (M1 walking skeleton) — C17, builds with gcc.
# The compiler itself is bootstrapped with cc (ADR-002 v2 boundary note);
# the PROGRAMS IT EMITS never touch cc/as/ld.
CC ?= gcc

SRC := $(wildcard src/*.c)
HDR := src/kavir.h

CFLAGS ?= -std=c17 -O2 -Wall -Wextra -Werror

all: kavir

kavir: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

# sanitizer build for the fuzz targets (ADR-005 v3 amendment b)
kavir-asan: $(SRC) $(HDR)
	$(CC) -std=c17 -O1 -g -fsanitize=address,undefined \
		-fno-sanitize-recover=all -fno-omit-frame-pointer \
		-Wall -Wextra -o $@ $(SRC)

test: kavir
	sh tests/run.sh

fuzz: kavir-asan
	python3 scripts/fuzz.py 300

clean:
	rm -f kavir kavir-asan

.PHONY: all test fuzz clean
