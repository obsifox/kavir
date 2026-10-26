/* check.c — Kavir semantic checker (M2 subset).
 *
 * M2 rules:
 *   - exactly one function named 'main' must exist, returning i32, with
 *     a non-empty body whose last statement is 'return' (M1 rule kept);
 *   - 'return' is only allowed as that last top-level statement —
 *     returning from inside an 'if' is a compile error in M2;
 *   - integer literals must fit i32 (ADR-007); the single exception is
 *     the numeral 2147483648 DIRECTLY under unary '-', which defines
 *     INT32_MIN (ADR-008 decision 1);
 *   - print/if statements are checked recursively into nested blocks;
 *   - function names must be unique.
 *
 * Arithmetic is NOT evaluated or folded here (ADR-008 decision 2): it
 * executes at runtime and traps per ADR-007.
 */
#include "kavir.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define KAVIR_INT32_MIN_AS_ULL 2147483648ULL /* = (unsigned)INT32_MIN */

/* Checks an expression node (recursively). Everything is i32 by
 * construction; this validates literal ranges. */
static int expr_check(const char *file, const Program *prog, int eidx) {
    const Expr *e;

    if (eidx < 0 || eidx >= prog->nexprs) {
        kavir_error(file, 1, 1, "internal: expression index out of range");
        return -1;
    }
    e = &prog->exprs[eidx];

    switch (e->kind) {
    case EX_INT:
        /* Unsigned comparison so values at/above 2^63 cannot wrap into
         * the accepted range (M1.1 review fix, ADR-007). */
        if (e->val > (unsigned long long)INT32_MAX) {
            kavir_error(file, e->line, e->col,
                        "integer literal %llu does not fit in i32 "
                        "(range %d .. %d)", e->val, INT32_MIN, INT32_MAX);
            return -1;
        }
        return 0;
    case EX_NEG:
        /* Parser guarantees the operand is a bare EX_INT literal.
         * 2147483648 is accepted ONLY here: it defines INT32_MIN. */
        if (e->a < 0 || e->a >= prog->nexprs ||
            prog->exprs[e->a].kind != EX_INT) {
            kavir_error(file, e->line, e->col,
                        "internal: unary '-' operand is not a literal");
            return -1;
        }
        {
            const Expr *inner = &prog->exprs[e->a];
            if (inner->val != KAVIR_INT32_MIN_AS_ULL &&
                inner->val > (unsigned long long)INT32_MAX) {
                kavir_error(file, inner->line, inner->col,
                            "integer literal %llu does not fit in i32 "
                            "(range %d .. %d)", inner->val,
                            INT32_MIN, INT32_MAX);
                return -1;
            }
        }
        return 0;
    case EX_BIN:
        if (expr_check(file, prog, e->a) != 0) return -1;
        if (expr_check(file, prog, e->b) != 0) return -1;
        return 0;
    }
    kavir_error(file, e->line, e->col, "internal: unknown expression kind");
    return -1;
}

static int stmt_check(const char *file, const Program *prog,
                      const Stmt *st, int in_if_block) {
    switch (st->kind) {
    case ST_RETURN:
        if (in_if_block) {
            kavir_error(file, st->line, st->col,
                        "'return' inside an 'if' block is not supported in "
                        "M2 (functions get real control flow in M3)");
            return -1;
        }
        if (expr_check(file, prog, st->expr_idx) != 0) return -1;
        return 0;
    case ST_PRINT:
        return expr_check(file, prog, st->expr_idx);
    case ST_PRINT_STR:
        return 0; /* literal already validated by the lexer */
    case ST_IF:
        if (expr_check(file, prog, st->expr_idx) != 0) return -1;
        {
            const Block *b = &prog->blocks[st->then_block];
            int s;
            for (s = 0; s < b->nstmts; s++)
                if (stmt_check(file, prog, &b->stmts[s], 1) != 0) return -1;
        }
        if (st->else_block >= 0) {
            const Block *b = &prog->blocks[st->else_block];
            int s;
            for (s = 0; s < b->nstmts; s++)
                if (stmt_check(file, prog, &b->stmts[s], 1) != 0) return -1;
        }
        return 0;
    }
    kavir_error(file, st->line, st->col, "internal: unknown statement kind");
    return -1;
}

int check(const char *file, const Program *prog) {
    int i, j, s;
    const Fn *main_fn = NULL;

    for (i = 0; i < prog->nfns; i++) {
        const Fn *fn = &prog->fns[i];
        const Block *body;
        if (!fn->ret_is_i32) {
            kavir_error(file, fn->line, fn->col,
                        "function '%s': M2 requires an i32 return type",
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
        body = &prog->blocks[fn->body_block];
        for (s = 0; s < body->nstmts; s++) {
            const Stmt *st = &body->stmts[s];
            /* top-level statements: 'return' must be the LAST one */
            if (stmt_check(file, prog, st, 0) != 0) return -1;
            if (st->kind == ST_RETURN && s != body->nstmts - 1) {
                kavir_error(file, st->line, st->col,
                            "'return' must be the last statement of the "
                            "function body");
                return -1;
            }
        }
        if (strcmp(fn->name, "main") == 0) main_fn = fn;
    }

    if (main_fn == NULL) {
        kavir_error(file, 1, 1,
                    "no 'main' function found (M2 programs need "
                    "'fn main() -> i32')");
        return -1;
    }
    {
        const Block *body = &prog->blocks[main_fn->body_block];
        if (body->nstmts == 0 ||
            body->stmts[body->nstmts - 1].kind != ST_RETURN) {
            kavir_error(file, main_fn->line, main_fn->col,
                        "function 'main' must end with a 'return' "
                        "statement");
            return -1;
        }
    }
    return 0;
}
