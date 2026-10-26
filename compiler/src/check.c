/* check.c — Kavir semantic checker (M1 subset).
 *
 * M1 rules:
 *   - exactly one function named 'main' must exist, returning i32;
 *   - every return-statement's expression type must match the function's
 *     return type;
 *   - integer literals must fit in i32 (the only M1 type);
 *   - function names must be unique.
 *
 * The M1 surface has no references and no unsafe primitives, so the
 * ADR-003 borrow/move rules are vacuous at this milestone; move/XOR
 * checking activates with reference types in M5.
 */
#include "kavir.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>

static int expr_is_i32(const char *file, const Fn *fn, const Expr *e) {
    /* M1: the only expression form is an integer literal; its type is
     * i32 iff the value fits. */
    (void)fn;
    if (e->kind != EX_INT) {
        kavir_error(file, e->line, e->col, "internal: unknown expression kind");
        return -1;
    }
    if (e->val > INT32_MAX) {
        kavir_error(file, e->line, e->col,
                    "integer literal %lld does not fit in i32 "
                    "(range %d .. %d)", e->val, INT32_MIN, INT32_MAX);
        return -1;
    }
    return 0;
}

int check(const char *file, const Program *prog) {
    int i, j, s;
    const Fn *main_fn = NULL;

    for (i = 0; i < prog->nfns; i++) {
        const Fn *fn = &prog->fns[i];
        if (!fn->ret_is_i32) {
            kavir_error(file, fn->line, fn->col,
                        "function '%s': M1 requires an i32 return type",
                        fn->name);
            return -1;
        }
        for (j = i + 1; j < prog->nfns; j++) {
            if (strcmp(fn->name, prog->fns[j].name) == 0) {
                kavir_error(file, prog->fns[j].line, prog->fns[j].col,
                            "duplicate function '%s'", fn->name);
                return -1;
            }
        }
        for (s = 0; s < fn->nstmts; s++) {
            const Stmt *st = &fn->stmts[s];
            if (st->kind == ST_RETURN) {
                if (expr_is_i32(file, fn, &st->expr) != 0) return -1;
                /* M1: return must be the last statement of the body */
                if (s != fn->nstmts - 1) {
                    kavir_error(file, st->line, st->col,
                                "in M1 'return' must be the last statement "
                                "(unreachable-code rules arrive later)");
                    return -1;
                }
            }
        }
        if (strcmp(fn->name, "main") == 0) main_fn = fn;
    }

    if (main_fn == NULL) {
        kavir_error(file, 1, 1,
                    "no 'main' function found (M1 programs need "
                    "'fn main() -> i32')");
        return -1;
    }
    if (main_fn->nstmts == 0) {
        kavir_error(file, main_fn->line, main_fn->col,
                    "function 'main' has no return statement (M1 requires "
                    "one)");
        return -1;
    }
    return 0;
}
