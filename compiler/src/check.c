/* check.c — Kavir semantic checker (M3 subset).
 *
 * M3 rules (ADR-010 decisions 1-9):
 *   - every function returns i32; function names unique;
 *   - exactly one 'main' must exist (its position is free; it may have
 *     any body, including an empty one — falling off the end returns 0,
 *     a documented M2 semantics both backends already implement);
 *   - 'return' is allowed ANYWHERE a statement is allowed (this relaxes
 *     the M2 rule; early returns inside if/while work — ADR-010);
 *   - flat per-function variable scope: params first, then lets in
 *     declaration order; NO shadowing anywhere in the function;
 *   - 'let x: i32 = e;' is immutable; 'let mut x: i32 = e;' is
 *     mutable (USER DECISION 2026-10-26, ADR-005 v3 wording);
 *     parameters are immutable; assignment 'x = e;' is a statement
 *     (not an expression) and only targets mutable locals;
 *   - calls resolve against ALL function signatures (collected in a
 *     first pass), so forward references and mutual recursion work;
 *     arity must match exactly;
 *   - integer literals must fit i32 (ADR-007); the single exception is
 *     the numeral 2147483648 DIRECTLY under unary '-' (ADR-008);
 *   - comparisons yield i32 1/0 and are parsed non-associatively.
 *
 * Arithmetic is NOT evaluated or folded here (ADR-008 decision 2).
 */
#include "kavir.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define KAVIR_INT32_MIN_AS_ULL 2147483648ULL /* = (unsigned)INT32_MIN */

/* Flat per-function variable scope. Index in vars[] == the variable's
 * vreg index (params first, then lets in declaration order) — ir.c
 * rebuilds the same mapping independently. */
typedef struct {
    char name[KAVIR_MAX_NAME];
    int is_param;
    int is_mut;
} Var;

typedef struct {
    Var vars[KAVIR_MAX_VARS];
    int nvars;
    int nparams;
} Scope;

static int scope_find(const Scope *sc, const char *name) {
    int i;
    for (i = 0; i < sc->nvars; i++)
        if (strcmp(sc->vars[i].name, name) == 0) return i;
    return -1;
}

static int scope_add(const char *file, Scope *sc, const char *name,
                     int is_param, int is_mut, int line, int col) {
    if (sc->nvars >= KAVIR_MAX_VARS) {
        kavir_error(file, line, col,
                    "too many variables (limit %d params + lets per "
                    "function)", KAVIR_MAX_VARS);
        return -1;
    }
    if (scope_find(sc, name) >= 0) {
        kavir_error(file, line, col,
                    "duplicate variable '%s' (no shadowing in the same "
                    "function)", name);
        return -1;
    }
    strncpy(sc->vars[sc->nvars].name, name, KAVIR_MAX_NAME - 1);
    sc->vars[sc->nvars].name[KAVIR_MAX_NAME - 1] = '\0';
    sc->vars[sc->nvars].is_param = is_param;
    sc->vars[sc->nvars].is_mut = is_mut;
    sc->nvars++;
    return 0;
}

static int find_fn(const Program *prog, const char *name) {
    int i;
    for (i = 0; i < prog->nfns; i++)
        if (strcmp(prog->fns[i].name, name) == 0) return i;
    return -1;
}

/* Checks an expression node (recursively). Everything is i32. */
static int expr_check(const char *file, const Program *prog, int eidx,
                      Scope *sc) {
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
    case EX_IDENT:
        if (scope_find(sc, e->name) < 0) {
            kavir_error(file, e->line, e->col,
                        "undeclared variable '%s'", e->name);
            return -1;
        }
        return 0;
    case EX_CALL: {
        int fi = find_fn(prog, e->name);
        int k;
        if (fi < 0) {
            kavir_error(file, e->line, e->col,
                        "call to undefined function '%s'", e->name);
            return -1;
        }
        if (e->nargs != prog->fns[fi].nparams) {
            kavir_error(file, e->line, e->col,
                        "function '%s' expects %d argument(s), got %d",
                        e->name, prog->fns[fi].nparams, e->nargs);
            return -1;
        }
        for (k = 0; k < e->nargs; k++)
            if (expr_check(file, prog, e->args[k], sc) != 0) return -1;
        return 0;
    }
    case EX_BIN:
        if (expr_check(file, prog, e->a, sc) != 0) return -1;
        if (expr_check(file, prog, e->b, sc) != 0) return -1;
        return 0;
    }
    kavir_error(file, e->line, e->col, "internal: unknown expression kind");
    return -1;
}

static int stmt_check(const char *file, const Program *prog,
                      const Stmt *st, Scope *sc) {
    switch (st->kind) {
    case ST_RETURN:
        return expr_check(file, prog, st->expr_idx, sc);
    case ST_PRINT:
        return expr_check(file, prog, st->expr_idx, sc);
    case ST_PRINT_STR:
        return 0; /* literal already validated by the lexer */
    case ST_LET:
        /* the initializer runs BEFORE the binding exists: referencing
         * the new name inside it is use-before-declaration */
        if (expr_check(file, prog, st->expr_idx, sc) != 0) return -1;
        return scope_add(file, sc, st->name, 0, st->is_mut,
                         st->line, st->col);
    case ST_ASSIGN: {
        int vi = scope_find(sc, st->name);
        if (vi < 0) {
            kavir_error(file, st->line, st->col,
                        "undeclared variable '%s'", st->name);
            return -1;
        }
        if (sc->vars[vi].is_param) {
            kavir_error(file, st->line, st->col,
                        "cannot assign to parameter '%s' (parameters are "
                        "immutable)", st->name);
            return -1;
        }
        if (!sc->vars[vi].is_mut) {
            kavir_error(file, st->line, st->col,
                        "cannot assign to immutable variable '%s' "
                        "(declare it with 'let mut')", st->name);
            return -1;
        }
        return expr_check(file, prog, st->expr_idx, sc);
    }
    case ST_IF: {
        const Block *b;
        int s;
        if (expr_check(file, prog, st->expr_idx, sc) != 0) return -1;
        b = &prog->blocks[st->then_block];
        for (s = 0; s < b->nstmts; s++)
            if (stmt_check(file, prog, &b->stmts[s], sc) != 0) return -1;
        if (st->else_block >= 0) {
            b = &prog->blocks[st->else_block];
            for (s = 0; s < b->nstmts; s++)
                if (stmt_check(file, prog, &b->stmts[s], sc) != 0)
                    return -1;
        }
        return 0;
    }
    case ST_WHILE: {
        const Block *b = &prog->blocks[st->body_block];
        int s;
        if (expr_check(file, prog, st->expr_idx, sc) != 0) return -1;
        for (s = 0; s < b->nstmts; s++)
            if (stmt_check(file, prog, &b->stmts[s], sc) != 0) return -1;
        return 0;
    }
    }
    kavir_error(file, st->line, st->col, "internal: unknown statement kind");
    return -1;
}

int check(const char *file, const Program *prog) {
    int i, j;

    /* pass 1: signatures (names unique, i32 return) — collected BEFORE
     * bodies so forward references and mutual recursion resolve */
    for (i = 0; i < prog->nfns; i++) {
        const Fn *fn = &prog->fns[i];
        int k;
        if (!fn->ret_is_i32) {
            kavir_error(file, fn->line, fn->col,
                        "function '%s': the return type must be i32",
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
        for (k = 0; k < fn->nparams; k++) {
            int m;
            for (m = 0; m < k; m++) {
                if (strcmp(fn->param_names[k], fn->param_names[m]) == 0) {
                    kavir_error(file, fn->line, fn->col,
                                "duplicate parameter '%s' in function "
                                "'%s'", fn->param_names[k], fn->name);
                    return -1;
                }
            }
        }
    }
    if (find_fn(prog, "main") < 0) {
        kavir_error(file, 1, 1,
                    "no 'main' function found (programs need "
                    "'fn main() -> i32')");
        return -1;
    }

    /* pass 2: bodies */
    for (i = 0; i < prog->nfns; i++) {
        const Fn *fn = &prog->fns[i];
        const Block *body = &prog->blocks[fn->body_block];
        Scope sc;
        int s;
        memset(&sc, 0, sizeof(sc));
        sc.nparams = fn->nparams;
        for (s = 0; s < fn->nparams; s++) {
            if (scope_add(file, &sc, fn->param_names[s], 1, 0,
                          fn->line, fn->col) != 0)
                return -1;
        }
        for (s = 0; s < body->nstmts; s++)
            if (stmt_check(file, prog, &body->stmts[s], &sc) != 0)
                return -1;
    }
    return 0;
}
