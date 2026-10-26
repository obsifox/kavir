/* ir.c — Kavir IR construction and text dump (M1 subset).
 *
 * M1 IR is a flat instruction list per function over virtual registers:
 *   IR_CONST dest, imm       — materialize an i32 immediate
 *   IR_RET   operand         — return a register value from the function
 *
 * The IR is the frontend/backend seam (ADR-002 v2): the backend consumes
 * it, and the interpreter executes it as the reference semantics. In M1
 * the register allocator is trivial (one value, one register); it
 * generalizes in M2/M3.
 */
#include "kavir.h"
#include <string.h>

static void ir_build_fn(const Fn *fn, IrFn *out) {
    int s;
    int vreg = 0;

    memset(out, 0, sizeof(*out));
    {
        size_t i = 0;
        while (fn->name[i] != '\0' && i < KAVIR_MAX_NAME - 1) {
            out->name[i] = fn->name[i];
            i++;
        }
        out->name[i] = '\0';
    }
    out->ret_is_i32 = fn->ret_is_i32;
    out->ninsts = 0;

    for (s = 0; s < fn->nstmts; s++) {
        const Stmt *st = &fn->stmts[s];
        if (st->kind != ST_RETURN) continue; /* checker guarantees shape */
        if (out->ninsts + 2 > KAVIR_MAX_INSTS) {
            /* capacity is checked in the checker path; defensive */
            return;
        }
        out->insts[out->ninsts].op = IR_CONST;
        out->insts[out->ninsts].dest = vreg;
        /* safe: the checker guarantees expr.val <= INT32_MAX (ADR-007) */
        out->insts[out->ninsts].val = (long long)st->expr.val;
        out->insts[out->ninsts].line = st->expr.line;
        out->insts[out->ninsts].col = st->expr.col;
        out->ninsts++;

        out->insts[out->ninsts].op = IR_RET;
        out->insts[out->ninsts].operand = vreg;
        out->insts[out->ninsts].line = st->line;
        out->insts[out->ninsts].col = st->col;
        out->ninsts++;

        vreg++;
    }
}

void ir_build(const Program *prog, IrModule *mod) {
    int i;
    memset(mod, 0, sizeof(*mod));
    mod->nfns = prog->nfns;
    for (i = 0; i < prog->nfns; i++)
        ir_build_fn(&prog->fns[i], &mod->fns[i]);
}

void ir_dump(const IrModule *mod, FILE *out) {
    int i, k;
    for (i = 0; i < mod->nfns; i++) {
        const IrFn *fn = &mod->fns[i];
        fprintf(out, "fn %s() -> %s {\n", fn->name,
                fn->ret_is_i32 ? "i32" : "?");
        for (k = 0; k < fn->ninsts; k++) {
            const IrInst *in = &fn->insts[k];
            switch (in->op) {
            case IR_CONST:
                fprintf(out, "  %%r%d = const %lld\n", in->dest, in->val);
                break;
            case IR_RET:
                fprintf(out, "  ret %%r%d\n", in->operand);
                break;
            }
        }
        fprintf(out, "}\n");
    }
}
