/* fuzz_frontend.c — libFuzzer harness for the Kavir frontend + backend
 * (M1.1 review item 3; ADR-005 v3 amendment b).
 *
 * libFuzzer is shipped with clang only (gcc does not provide it), so this
 * harness builds with clang and NOT with the plain gcc build used in this
 * environment:
 *
 *   make kavir-libfuzzer          (errors out with guidance if no clang)
 *   make fuzz-libfuzzer           (runs it against corpus/ for 60s)
 *
 * Equivalent manual invocation (note: main.c is EXCLUDED — libFuzzer
 * provides its own main):
 *   clang -std=c17 -g -O1 -fsanitize=fuzzer,address,undefined \
 *     -fno-sanitize-recover=all -fno-omit-frame-pointer -Wall -Wextra \
 *     -o kavir-libfuzzer src/lexer.c src/parser.c src/check.c \
 *     src/ir.c src/interp.c src/target_x86_64_linux.c fuzz/fuzz_frontend.c
 *   mkdir -p corpus && ./kavir-libfuzzer corpus -max_len=2048 \
 *     -print_final_stats=1 -detect_leaks=0
 *
 * In-process: one input goes through lex -> parse -> check -> ir_build ->
 * interp_run -> target emit, the same stages the driver exercises, but
 * with real coverage feedback. Sanitizer abort on the first bad input.
 */
#include "../src/kavir.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *src;
    Token *toks;
    unsigned char *buf;
    Program prog;
    IrModule mod;
    char errbuf[KAVIR_MAX_ERR];
    int n, out_size = 0, entry_off = 0;

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
                    ir_build(&prog, &mod);
                    (void)interp_run(&mod);
                    buf = (unsigned char *)malloc(KAVIR_MAX_CODE);
                    if (buf != NULL) {
                        errbuf[0] = '\0';
                        (void)target_x86_64_linux_emit(
                            &mod, buf, KAVIR_MAX_CODE,
                            &out_size, &entry_off, errbuf, KAVIR_MAX_ERR);
                        free(buf);
                    }
                }
            }
        }
        free(toks);
    }
    free(src);
    return 0;
}
