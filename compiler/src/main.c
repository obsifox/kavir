/* main.c — kavir driver (M2).
 *
 * Subcommands:
 *   kavir build <in.k> -o <out>   compile to a static ELF executable
 *   kavir interp <in.k>           execute the IR directly (reference
 *                                 semantics); prints to stdout like the
 *                                 ELF, reports traps identically
 *   kavir dump-ir <in.k>          print the IR text
 *
 * Exit codes: 0 success; 1 compile error; 2 usage/IO error; a runtime
 * TRAP in 'interp' exits 101 exactly like the compiled program
 * (ADR-008 decision 3). The compiled-program path NEVER touches
 * cc/as/ld (ADR-002 v2).
 */
#include "kavir.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int usage(void) {
    fprintf(stderr,
            "usage: kavir build <in.k> -o <out>\n"
            "       kavir interp <in.k>\n"
            "       kavir dump-ir <in.k>\n");
    return 2;
}

static char *read_file(const char *path, long *out_len) {
    FILE *f = fopen(path, "rb");
    char *buf;
    long len;
    if (f == NULL) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) { fclose(f); return NULL; }
    if (len > 0 && fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[len] = '\0';
    fclose(f);
    if (out_len != NULL) *out_len = len;
    return buf;
}

/* Runs the full frontend; returns 0 and fills mod on success. */
static int frontend(const char *path, IrModule *mod) {
    static Program prog; /* large (pools with name fields): keep off the
                          * C stack for sanitizer builds */
    char *src;
    Token *toks = NULL;
    int ntoks, rc = -1;

    src = read_file(path, NULL);
    if (src == NULL) {
        fprintf(stderr, "kavir: cannot read '%s'\n", path);
        return -1;
    }

    toks = (Token *)calloc(KAVIR_MAX_TOKENS, sizeof(Token));
    if (toks == NULL) {
        fprintf(stderr, "kavir: out of memory\n");
        goto done;
    }

    ntoks = lex(path, src, toks, KAVIR_MAX_TOKENS);
    if (ntoks < 0) goto done;

    if (parse(path, toks, ntoks, &prog) != 0) goto done;
    if (check(path, &prog) != 0) goto done;

    if (ir_build(path, &prog, mod) != 0) goto done;
    mod->file = path; /* used for trap diagnostics (ADR-008 decision 3) */
    rc = 0;

done:
    free(toks);
    free(src);
    return rc;
}

static int cmd_build(const char *in_path, const char *out_path) {
    static IrModule mod; /* large: keep off the C stack */
    unsigned char *buf = NULL;
    char errbuf[KAVIR_MAX_ERR];
    int size = 0, entry_off = 0, rc = 1;
    FILE *f;

    if (frontend(in_path, &mod) != 0) return 1;

    buf = (unsigned char *)malloc(KAVIR_MAX_CODE);
    if (buf == NULL) {
        fprintf(stderr, "kavir: out of memory\n");
        return 1;
    }
    errbuf[0] = '\0';

    if (target_x86_64_linux_emit(&mod, buf, KAVIR_MAX_CODE, &size,
                                 &entry_off, errbuf, KAVIR_MAX_ERR) != 0) {
        fprintf(stderr, "kavir: codegen failed: %s\n",
                errbuf[0] != '\0' ? errbuf : "unknown error");
        goto done;
    }

    f = fopen(out_path, "wb");
    if (f == NULL) {
        fprintf(stderr, "kavir: cannot write '%s'\n", out_path);
        goto done;
    }
    if (fwrite(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "kavir: short write to '%s'\n", out_path);
        fclose(f);
        goto done;
    }
    fclose(f);
    chmod(out_path, 0755);

    fprintf(stderr, "kavir: wrote %s (%d bytes, entry @0x%x)\n",
            out_path, size, entry_off);
    rc = 0;

done:
    free(buf);
    return rc;
}

static int cmd_interp(const char *in_path) {
    static IrModule mod;
    InterpResult r;
    if (frontend(in_path, &mod) != 0) return 1;
    if (interp_run(&mod, &r) != 0) return 1;
    if (r.trapped) {
        /* identical bytes to the compiled program's trap line */
        fputs(r.trap_msg, stderr);
        return KAVIR_TRAP_EXIT_CODE;
    }
    /* mirror OS exit-code truncation so harness comparisons are exact */
    return (int)(r.value & 0xff);
}

static int cmd_dump_ir(const char *in_path) {
    static IrModule mod;
    if (frontend(in_path, &mod) != 0) return 1;
    ir_dump(&mod, stdout);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) return usage();

    if (strcmp(argv[1], "build") == 0) {
        /* Flags and the input may appear in any order after 'build'
         * (M1.1 review item: `kavir build -o out in.k` used to feed the
         * literal string "-o" to the compiler as the input path). */
        const char *out = NULL, *in = NULL;
        int i;
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr,
                            "kavir build: '-o' must be followed by the "
                            "output path\n");
                    return usage();
                }
                out = argv[i + 1];
                i++;
            } else if (in == NULL) {
                in = argv[i];
            } else {
                fprintf(stderr,
                        "kavir build: unexpected extra argument '%s' "
                        "(only one input file is allowed)\n", argv[i]);
                return usage();
            }
        }
        if (in == NULL) {
            fprintf(stderr,
                    "kavir build: no input file given "
                    "(usage: kavir build <in.k> -o <out>)\n");
            return usage();
        }
        if (out == NULL) {
            fprintf(stderr,
                    "kavir build: no output path given — add '-o <out>'\n"
                    "       example: kavir build %s -o prog\n", in);
            return usage();
        }
        return cmd_build(in, out);
    }
    if (strcmp(argv[1], "interp") == 0 && argc == 3)
        return cmd_interp(argv[2]);
    if (strcmp(argv[1], "dump-ir") == 0 && argc == 3)
        return cmd_dump_ir(argv[2]);

    return usage();
}
