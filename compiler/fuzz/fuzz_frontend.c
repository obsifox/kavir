/* fuzz_frontend.c -- libFuzzer harness for the Kavir M2 pipeline
 * (ADR-005 v3 amendment b; ADR-008 decision 8).
 *
 * libFuzzer is shipped with clang only (gcc does not provide it):
 *
 *   make kavir-libfuzzer          (errors out with guidance if no clang)
 *   make fuzz-libfuzzer           (runs it against corpus/ for 60s)
 *
 * Equivalent manual invocation (note: main.c is EXCLUDED -- libFuzzer
 * provides its own main):
 *   clang -std=c17 -g -O1 -fsanitize=fuzzer,address,undefined \
 *     -fno-sanitize-recover=all -fno-omit-frame-pointer -Wall -Wextra \
 *     -o kavir-libfuzzer src/lexer.c src/parser.c src/check.c \
 *     src/ir.c src/interp.c src/target_x86_64_linux.c fuzz/fuzz_frontend.c
 *   mkdir -p corpus && ./kavir-libfuzzer corpus -max_len=2048 \
 *     -print_final_stats=1 -detect_leaks=0
 *
 * In-process: one input goes through lex -> parse -> check -> ir_build
 * -> interp (quiet) -> target emit, the same stages the driver
 * exercises, with real coverage feedback. Traps are REPORTED (not
 * exited) and stdout is suppressed via interp_set_quiet so the harness
 * output stays readable. Sanitizer abort on the first bad input.
 */
#include "../src/kavir.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static Program prog;   /* large pooled structures: keep off the C
                            * stack and reuse across inputs */
    static IrModule mod;
    char *src;
    Token *toks;
    unsigned char *buf;
    InterpResult res;
    char errbuf[KAVIR_MAX_ERR];
    int st, n, out_size = 0, entry_off = 0;

    if (size == 0 || size > 2048) return 0; /* cap: huge inputs only hit
                                               the token-count limit */

    src = (char *)malloc(size + 1);
    if (src == NULL) return 0;
    memcpy(src, data, size);
    src[size] = '\0';

    toks = (Token *)calloc(KAVIR_MAX_TOKENS, sizeof(Token));
    if (toks != NULL) {
        n = lex("fuzz", src, toks, KAVIR_MAX_TOKENS);
        if (n >= 0) {
            if (parse("fuzz", toks, n, &prog) == 0) {
                if (check("fuzz", &prog) == 0) {
                    if (ir_build("fuzz", &prog, &mod) == 0) {
                        mod.file = "fuzz";
                        interp_set_quiet(1);
                        interp_set_step_limit(1000000UL);
                        /* M4.1: coverage-guided mutation can legally
                         * turn a bounded loop into a divergent one; the
                         * budget expires as the explicit
                         * KAVIR_INTERP_STEP_LIMIT status (never a
                         * trap), and the harness skips the remaining
                         * stages so the campaign can continue. */
                        st = interp_run(&mod, &res);
                        if (st == KAVIR_INTERP_INTERNAL) {
                            /* invalid interpreter state: must never
                             * happen on a checked program */
                            abort();
                        }
                        buf = (unsigned char *)malloc(KAVIR_MAX_CODE);
                        if (buf != NULL && st == KAVIR_INTERP_OK) {
                            errbuf[0] = '\0';
                            (void)target_x86_64_linux_emit(
                                &mod, buf, KAVIR_MAX_CODE,
                                &out_size, &entry_off, errbuf, KAVIR_MAX_ERR);
                            free(buf);
                        }
                    }
                }
            }
        }
        free(toks);
    }
    free(src);
    return 0;
}
