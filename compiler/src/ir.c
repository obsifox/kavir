/* ir.c -- Kavir IR construction and text dump (M3 subset).
 *
 * M3 IR is a flat instruction list per function over virtual registers
 * (ADR-008 decision 5; extended additively in M2 and M3 -- the format is
 * internal and never persisted). Opcodes:
 *
 *   IR_CONST    dest, imm          -- materialize an i32 immediate
 *   IR_RET      src1               -- return a vreg from the function
 *   IR_ADD/SUB/MUL/DIV/REM d,s1,s2 -- i32 arithmetic (trap on overflow,
 *                                    division by zero, INT_MIN over -1)
 *   IR_PRINT    dest               -- print a vreg as decimal
 *   IR_PRINT_STR str_idx           -- print string-table bytes
 *   IR_JMP      target             -- unconditional jump
 *   IR_JMPZ     src1, target       -- jump when vreg == 0
 *   IR_LT/LE/GT/GE/EQ/NE d,s1,s2   -- comparison, yields 1/0 (M3)
 *   IR_COPY     dest, src1         -- vreg <- vreg (lets, assigns) (M3)
 *   IR_CALL     dest, fn_idx, args -- call, result into dest (M3)
 *
 * Variables map to fixed vregs: params first (0..nparams-1), then lets
 * in declaration order -- the checker guarantees every reference
 * resolves; the builder keeps its own name table as defense in depth.
 *
 * Evaluation order is fixed by construction: left operand, then right
 * operand, then the operation; call arguments left-to-right, then the
 * call (ADR-008 decision 2; ADR-010 decision 1) -- observable through
 * traps between prints.
 */
#include "kavir.h"
#include <string.h>

typedef struct {
    IrFn *out;
    int vreg;
    int label_fix[KAVIR_MAX_LABELS * 2][2]; /* {inst index, label id} */
    int nfix;
    const char *file;
    /* variable name -> vreg (params first 0..nparams-1, then lets in
     * declaration order at whatever vreg numbers they claimed); the
     * checker guarantees every lookup succeeds -- this table is defense
     * in depth and an internal-error source if violated. NOTE: the
     * vnames[] INDEX is not the vreg number -- lets allocate vregs after
     * expression temporaries, so the mapping is explicit. */
    char vnames[KAVIR_MAX_VARS][KAVIR_MAX_NAME];
    int vnums[KAVIR_MAX_VARS];
    int nvnames;
    const Program *prog; /* callee resolution for EX_CALL */
} IrBuild;

static int find_vreg(const IrBuild *b, const char *name) {
    int i;
    for (i = 0; i < b->nvnames; i++)
        if (strcmp(b->vnames[i], name) == 0) return b->vnums[i];
    return -1;
}

/* Type ids are checker annotations; words per type come from the
 * program's own type table (mirrors check.c's ty_words). */
static int ty_words(const Program *prog, int ty) {
    if (ty <= KAVIR_TY_I32) return 1;
    if (ty > prog->ntypes) return 1; /* unreachable on checked programs */
    return prog->types[ty - 1].size_words;
}

static int irf_add(IrBuild *b, const IrInst *in) {
    if (b->out->ninsts >= KAVIR_MAX_INSTS) {
        kavir_error(b->file, in->line, in->col,
                    "function too large (M2 limit %d IR instructions per "
                    "function)", KAVIR_MAX_INSTS);
        return -1;
    }
    b->out->insts[b->out->ninsts++] = *in;
    return 0;
}

static int new_label(IrBuild *b) {
    if (b->out->nlabels >= KAVIR_MAX_LABELS) {
        kavir_error(b->file, 1, 1,
                    "too many labels (M2 limit %d per function)",
                    KAVIR_MAX_LABELS);
        return -1;
    }
    b->out->labels[b->out->nlabels] = -1; /* resolved when placed */
    return b->out->nlabels++;
}

static int place_label(IrBuild *b, int label) {
    b->out->labels[label] = b->out->ninsts;
    return 0;
}

static void fix_labels(IrBuild *b) {
    int i;
    for (i = 0; i < b->nfix; i++) {
        int inst_at = b->label_fix[i][0];
        int label = b->label_fix[i][1];
        b->out->insts[inst_at].target = b->out->labels[label];
    }
}

/* Lowers an expression; returns the BASE vreg holding its value (a
 * scalar uses one vreg; an aggregate of k words uses k consecutive
 * vregs), or -1. Type ids come from the checker's annotations. */
static int lower_expr(IrBuild *b, const Program *prog, int eidx) {
    const Expr *e = &prog->exprs[eidx];
    int words = ty_words(prog, e->ty);

    switch (e->kind) {
    case EX_INT: {
        IrInst in;
        memset(&in, 0, sizeof(in));
        in.op = IR_CONST;
        in.dest = b->vreg++;
        /* checker guarantees val <= INT32_MAX here */
        in.val = (long long)e->val;
        in.line = e->line;
        in.col = e->col;
        if (irf_add(b, &in) != 0) return -1;
        return in.dest;
    }
    case EX_NEG: {
        /* Compile-time literal interpretation (ADR-008 decision 1):
         * the checker already bounded the operand; 2147483648 becomes
         * INT32_MIN here. This is numeral interpretation, NOT
         * arithmetic folding. */
        IrInst in;
        long long v = -(long long)prog->exprs[e->a].val;
        memset(&in, 0, sizeof(in));
        in.op = IR_CONST;
        in.dest = b->vreg++;
        in.val = v;
        in.line = e->line;
        in.col = e->col;
        if (irf_add(b, &in) != 0) return -1;
        return in.dest;
    }
    case EX_IDENT: {
        int v = find_vreg(b, e->name);
        IrInst in;
        if (v < 0) {
            /* the checker guarantees this is unreachable */
            kavir_error(b->file, e->line, e->col,
                        "internal: unresolved variable '%s'", e->name);
            return -1;
        }
        if (words == 1) {
            memset(&in, 0, sizeof(in));
            in.op = IR_COPY;
            in.dest = b->vreg++;
            in.src1 = v;
            in.line = e->line;
            in.col = e->col;
            if (irf_add(b, &in) != 0) return -1;
            return in.dest;
        }
        return v; /* aggregates are read in place (value semantics per
                   * statement: nothing mutates between statements) */
    }
    case EX_CALL: {
        int fi, k, dst;
        IrInst in;
        for (fi = 0; fi < b->prog->nfns; fi++)
            if (strcmp(b->prog->fns[fi].name, e->name) == 0) break;
        if (fi >= b->prog->nfns) {
            /* the checker guarantees this is unreachable */
            kavir_error(b->file, e->line, e->col,
                        "internal: unresolved call to '%s'", e->name);
            return -1;
        }
        memset(&in, 0, sizeof(in));
        for (k = 0; k < e->nargs; k++) {
            int a = lower_expr(b, prog, e->args[k]);
            if (a < 0) return -1;
            in.args[k] = a; /* left-to-right, before the call */
            in.arg_words[k] = ty_words(prog,
                                       prog->exprs[e->args[k]].ty);
        }
        in.op = IR_CALL;
        in.ret_words = ty_words(prog, prog->fns[fi].ret_ty);
        in.dest = b->vreg;
        b->vreg += in.ret_words; /* dest..dest+ret_words-1 */
        in.fn_idx = fi;
        in.nargs = e->nargs;
        in.line = e->line;
        in.col = e->col;
        dst = in.dest;
        if (irf_add(b, &in) != 0) return -1;
        return dst;
    }
    case EX_STRUCT_LIT: {
        const TypeDef *td = &prog->types[e->ty - 1];
        int dst = b->vreg, k;
        IrInst in;
        b->vreg += words;
        for (k = 0; k < e->nargs; k++) {
            int v = lower_expr(b, prog, e->args[k]);
            int off = td->field_off[k];
            int w = ty_words(prog, td->field_ty[k]);
            int j;
            if (v < 0) return -1;
            for (j = 0; j < w; j++) {
                memset(&in, 0, sizeof(in));
                in.op = IR_COPY;
                in.dest = dst + off + j;
                in.src1 = v + j;
                in.line = e->line;
                in.col = e->col;
                if (irf_add(b, &in) != 0) return -1;
            }
        }
        return dst;
    }
    case EX_ENUM_CTOR: {
        const TypeDef *td = &prog->types[e->ty - 1];
        int variant = e->aux;
        int dst = b->vreg, k, word_off = 1, j;
        IrInst in;
        b->vreg += words;
        memset(&in, 0, sizeof(in));
        in.op = IR_CONST;
        in.dest = b->vreg++;
        in.val = variant; /* tag = variant index (ADR-011) */
        in.line = e->line;
        in.col = e->col;
        if (irf_add(b, &in) != 0) return -1;
        memset(&in, 0, sizeof(in));
        in.op = IR_COPY;
        in.dest = dst;
        in.src1 = b->vreg - 1;
        in.line = e->line;
        in.col = e->col;
        if (irf_add(b, &in) != 0) return -1;
        for (j = 0; j < e->nargs; j++) {
            int v = lower_expr(b, prog, e->args[j]);
            int w = ty_words(prog, td->variant_ty[variant][j]);
            if (v < 0) return -1;
            for (k = 0; k < w; k++) {
                memset(&in, 0, sizeof(in));
                in.op = IR_COPY;
                in.dest = dst + word_off + k;
                in.src1 = v + k;
                in.line = e->line;
                in.col = e->col;
                if (irf_add(b, &in) != 0) return -1;
            }
            word_off += w;
        }
        return dst;
    }
    case EX_OPT_CTOR: {
        const TypeDef *td = &prog->types[e->ty - 1];
        int dst = b->vreg;
        IrInst in;
        b->vreg += words;
        memset(&in, 0, sizeof(in));
        in.op = IR_CONST;
        in.dest = b->vreg++;
        in.val = e->aux; /* 0 = None, 1 = Some */
        in.line = e->line;
        in.col = e->col;
        if (irf_add(b, &in) != 0) return -1;
        memset(&in, 0, sizeof(in));
        in.op = IR_COPY;
        in.dest = dst;
        in.src1 = b->vreg - 1;
        in.line = e->line;
        in.col = e->col;
        if (irf_add(b, &in) != 0) return -1;
        if (e->aux == 1) {
            int v = lower_expr(b, prog, e->args[0]);
            int w = ty_words(prog, td->elem_ty);
            int j;
            if (v < 0) return -1;
            for (j = 0; j < w; j++) {
                memset(&in, 0, sizeof(in));
                in.op = IR_COPY;
                in.dest = dst + 1 + j;
                in.src1 = v + j;
                in.line = e->line;
                in.col = e->col;
                if (irf_add(b, &in) != 0) return -1;
            }
        }
        return dst;
    }
    case EX_FIELD: {
        int base, dst, j;
        IrInst in;
        if (e->b == 1) {
            /* bare enum ctor (Type.Variant, no payload): the checker
             * resolved e->aux to the variant index */
            const TypeDef *td = &prog->types[e->ty - 1];
            dst = b->vreg;
            b->vreg += words;
            memset(&in, 0, sizeof(in));
            in.op = IR_CONST;
            in.dest = b->vreg++;
            in.val = e->aux;
            in.line = e->line;
            in.col = e->col;
            if (irf_add(b, &in) != 0) return -1;
            memset(&in, 0, sizeof(in));
            in.op = IR_COPY;
            in.dest = dst;
            in.src1 = b->vreg - 1;
            in.line = e->line;
            in.col = e->col;
            if (irf_add(b, &in) != 0) return -1;
            (void)td;
            return dst;
        }
        base = lower_expr(b, prog, e->a);
        if (base < 0) return -1;
        dst = b->vreg;
        b->vreg += words;
        for (j = 0; j < words; j++) {
            memset(&in, 0, sizeof(in));
            in.op = IR_COPY;
            in.dest = dst + j;
            in.src1 = base + e->aux + j;
            in.line = e->line;
            in.col = e->col;
            if (irf_add(b, &in) != 0) return -1;
        }
        return dst;
    }
    case EX_MATCH: {
        int scrut = lower_expr(b, prog, e->a);
        int dst = b->vreg++;
        int l_end = new_label(b);
        int arm0 = e->aux, narms = e->nargs;
        int i, any_wild = 0;
        if (scrut < 0) return -1;
        if (l_end < 0) return -1;
        for (i = 0; i < narms; i++) {
            const MatchArm *arm = &prog->arms[arm0 + i];
            if (arm->is_wild) {
                int v = lower_expr(b, prog, arm->body);
                IrInst in;
                int jm;
                if (v < 0) return -1;
                memset(&in, 0, sizeof(in));
                in.op = IR_COPY;
                in.dest = dst;
                in.src1 = v;
                in.line = arm->line;
                in.col = arm->col;
                if (irf_add(b, &in) != 0) return -1;
                memset(&in, 0, sizeof(in));
                in.op = IR_JMP;
                in.line = arm->line;
                in.col = arm->col;
                jm = b->out->ninsts;
                if (irf_add(b, &in) != 0) return -1;
                b->label_fix[b->nfix][0] = jm;
                b->label_fix[b->nfix][1] = l_end;
                b->nfix++;
                any_wild = 1;
                break;
            } else {
                int tagv, cmp, l_next, v, j, word_off = 1;
                int saved_names = b->nvnames;
                IrInst in;
                memset(&in, 0, sizeof(in));
                in.op = IR_CONST;
                in.dest = b->vreg++;
                in.val = arm->variant; /* tag */
                in.line = arm->line;
                in.col = arm->col;
                if (irf_add(b, &in) != 0) return -1;
                tagv = in.dest;
                memset(&in, 0, sizeof(in));
                in.op = IR_EQ;
                in.dest = b->vreg++;
                in.src1 = tagv;
                in.src2 = scrut; /* word 0 of the scrutinee = tag */
                in.line = arm->line;
                in.col = arm->col;
                if (irf_add(b, &in) != 0) return -1;
                cmp = in.dest;
                memset(&in, 0, sizeof(in));
                in.op = IR_JMPZ;
                in.src1 = cmp;
                in.line = arm->line;
                in.col = arm->col;
                {
                    int jz = b->out->ninsts;
                    l_next = new_label(b);
                    if (l_next < 0) return -1;
                    if (irf_add(b, &in) != 0) return -1;
                    b->label_fix[b->nfix][0] = jz;
                    b->label_fix[b->nfix][1] = l_next;
                    b->nfix++;
                }
                /* payload bindings: fresh vregs, arm-local names */
                for (j = 0; j < arm->nbinds; j++) {
                    int bw = ty_words(prog, arm->bind_ty[j]);
                    int bbase = b->vreg;
                    int k;
                    b->vreg += bw;
                    if (b->nvnames >= KAVIR_MAX_VARS) {
                        kavir_error(b->file, arm->line, arm->col,
                                    "too many variables (limit %d per "
                                    "function)", KAVIR_MAX_VARS);
                        return -1;
                    }
                    strncpy(b->vnames[b->nvnames], arm->bind_names[j],
                            KAVIR_MAX_NAME - 1);
                    b->vnames[b->nvnames][KAVIR_MAX_NAME - 1] = '\0';
                    b->vnums[b->nvnames] = bbase;
                    b->nvnames++;
                    for (k = 0; k < bw; k++) {
                        memset(&in, 0, sizeof(in));
                        in.op = IR_COPY;
                        in.dest = bbase + k;
                        in.src1 = scrut + word_off + k;
                        in.line = arm->line;
                        in.col = arm->col;
                        if (irf_add(b, &in) != 0) return -1;
                    }
                    word_off += bw;
                }
                v = lower_expr(b, prog, arm->body);
                if (v < 0) return -1;
                memset(&in, 0, sizeof(in));
                in.op = IR_COPY;
                in.dest = dst;
                in.src1 = v;
                in.line = arm->line;
                in.col = arm->col;
                if (irf_add(b, &in) != 0) return -1;
                memset(&in, 0, sizeof(in));
                in.op = IR_JMP;
                in.line = arm->line;
                in.col = arm->col;
                {
                    int jm = b->out->ninsts;
                    if (irf_add(b, &in) != 0) return -1;
                    b->label_fix[b->nfix][0] = jm;
                    b->label_fix[b->nfix][1] = l_end;
                    b->nfix++;
                }
                b->nvnames = saved_names; /* bindings vanish */
                place_label(b, l_next);
            }
        }
        if (!any_wild) {
            /* exhaustiveness guarantees this is unreachable; keep the
             * result defined for the dataflow (interpreter zeroes new
             * frames, so CONST 0 mirrors the machine exactly) */
            IrInst in;
            memset(&in, 0, sizeof(in));
            in.op = IR_CONST;
            in.dest = b->vreg++;
            in.val = 0;
            in.line = e->line;
            in.col = e->col;
            if (irf_add(b, &in) != 0) return -1;
            memset(&in, 0, sizeof(in));
            in.op = IR_COPY;
            in.dest = dst;
            in.src1 = b->vreg - 1;
            in.line = e->line;
            in.col = e->col;
            if (irf_add(b, &in) != 0) return -1;
        }
        place_label(b, l_end);
        return dst;
    }
    case EX_BIN: {
        int l = lower_expr(b, prog, e->a);
        int r, dst;
        IrInst in;
        if (l < 0) return -1;
        r = lower_expr(b, prog, e->b);
        if (r < 0) return -1;
        memset(&in, 0, sizeof(in));
        switch (e->op) {
        case BIN_ADD: in.op = IR_ADD; break;
        case BIN_SUB: in.op = IR_SUB; break;
        case BIN_MUL: in.op = IR_MUL; break;
        case BIN_DIV: in.op = IR_DIV; break;
        case BIN_REM: in.op = IR_REM; break;
        case BIN_LT: in.op = IR_LT; break;
        case BIN_LE: in.op = IR_LE; break;
        case BIN_GT: in.op = IR_GT; break;
        case BIN_GE: in.op = IR_GE; break;
        case BIN_EQ: in.op = IR_EQ; break;
        case BIN_NE: in.op = IR_NE; break;
        }
        in.dest = b->vreg++;
        in.src1 = l;
        in.src2 = r;
        in.line = e->line; /* operator position (trap messages) */
        in.col = e->col;
        dst = in.dest;
        if (irf_add(b, &in) != 0) return -1;
        return dst;
    }
    }
    return -1;
}

static int lower_stmts(IrBuild *b, const Program *prog, const Block *blk) {
    int s;
    for (s = 0; s < blk->nstmts; s++) {
        const Stmt *st = &blk->stmts[s];
        switch (st->kind) {
        case ST_RETURN: {
            int v = lower_expr(b, prog, st->expr_idx);
            IrInst in;
            if (v < 0) return -1;
            memset(&in, 0, sizeof(in));
            in.op = IR_RET;
            in.src1 = v;
            in.line = st->line;
            in.col = st->col;
            if (irf_add(b, &in) != 0) return -1;
            break;
        }
        case ST_PRINT: {
            int v = lower_expr(b, prog, st->expr_idx);
            IrInst in;
            if (v < 0) return -1;
            memset(&in, 0, sizeof(in));
            in.op = IR_PRINT;
            in.dest = v;
            in.line = st->line;
            in.col = st->col;
            if (irf_add(b, &in) != 0) return -1;
            break;
        }
        case ST_PRINT_STR: {
            IrInst in;
            memset(&in, 0, sizeof(in));
            in.op = IR_PRINT_STR;
            in.str_idx = st->str_idx;
            in.line = st->line;
            in.col = st->col;
            if (irf_add(b, &in) != 0) return -1;
            break;
        }
        case ST_LET: {
            int v = lower_expr(b, prog, st->expr_idx);
            IrInst in;
            int lv, nw, j;
            if (v < 0) return -1;
            if (b->nvnames >= KAVIR_MAX_VARS) {
                kavir_error(b->file, st->line, st->col,
                            "too many variables (limit %d per function)",
                            KAVIR_MAX_VARS);
                return -1;
            }
            nw = ty_words(prog, st->ty); /* checker-annotated type */
            lv = b->vreg; /* lets claim nwords consecutive vregs */
            b->vreg += nw;
            strncpy(b->vnames[b->nvnames], st->name, KAVIR_MAX_NAME - 1);
            b->vnames[b->nvnames][KAVIR_MAX_NAME - 1] = '\0';
            b->vnums[b->nvnames] = lv;
            b->nvnames++;
            for (j = 0; j < nw; j++) {
                memset(&in, 0, sizeof(in));
                in.op = IR_COPY;
                in.dest = lv + j;
                in.src1 = v + j;
                in.line = st->line;
                in.col = st->col;
                if (irf_add(b, &in) != 0) return -1;
            }
            break;
        }
        case ST_ASSIGN: {
            int v = lower_expr(b, prog, st->expr_idx);
            IrInst in;
            int tv, nw, j;
            if (v < 0) return -1;
            tv = find_vreg(b, st->name);
            if (tv < 0) {
                kavir_error(b->file, st->line, st->col,
                            "internal: unresolved assignment target '%s'",
                            st->name);
                return -1;
            }
            nw = ty_words(prog, st->ty);
            tv += st->word_off; /* field path offset (0 for plain var) */
            for (j = 0; j < nw; j++) {
                memset(&in, 0, sizeof(in));
                in.op = IR_COPY;
                in.dest = tv + j;
                in.src1 = v + j;
                in.line = st->line;
                in.col = st->col;
                if (irf_add(b, &in) != 0) return -1;
            }
            break;
        }
        case ST_WHILE: {
            int l_start, l_end, c, jz;
            IrInst in;
            l_start = new_label(b);
            if (l_start < 0) return -1;
            l_end = new_label(b);
            if (l_end < 0) return -1;
            place_label(b, l_start);
            c = lower_expr(b, prog, st->expr_idx);
            if (c < 0) return -1;
            memset(&in, 0, sizeof(in));
            in.op = IR_JMPZ;
            in.src1 = c;
            in.line = st->line;
            in.col = st->col;
            jz = b->out->ninsts;
            if (irf_add(b, &in) != 0) return -1;
            b->label_fix[b->nfix][0] = jz;
            b->label_fix[b->nfix][1] = l_end;
            b->nfix++;
            if (lower_stmts(b, prog, &prog->blocks[st->body_block]) != 0)
                return -1;
            memset(&in, 0, sizeof(in));
            in.op = IR_JMP;
            in.target = b->out->labels[l_start]; /* backward: resolved */
            in.line = st->line;
            in.col = st->col;
            if (irf_add(b, &in) != 0) return -1;
            place_label(b, l_end);
            break;
        }
        case ST_IF: {
            int c = lower_expr(b, prog, st->expr_idx);
            int jz, jend = -1, l_else, l_end;
            IrInst in;
            if (c < 0) return -1;
            memset(&in, 0, sizeof(in));
            in.op = IR_JMPZ;
            in.src1 = c;
            in.line = st->line;
            in.col = st->col;
            jz = b->out->ninsts;
            if (irf_add(b, &in) != 0) return -1;
            l_else = new_label(b);
            if (l_else < 0) return -1;
            b->label_fix[b->nfix][0] = jz;
            b->label_fix[b->nfix][1] = l_else;
            b->nfix++;

            if (lower_stmts(b, prog, &prog->blocks[st->then_block]) != 0)
                return -1;

            if (st->else_block >= 0) {
                /* jump over the else-branch */
                memset(&in, 0, sizeof(in));
                in.op = IR_JMP;
                in.line = st->line;
                in.col = st->col;
                jend = b->out->ninsts;
                if (irf_add(b, &in) != 0) return -1;
                l_end = new_label(b);
                if (l_end < 0) return -1;
                b->label_fix[b->nfix][0] = jend;
                b->label_fix[b->nfix][1] = l_end;
                b->nfix++;
                place_label(b, l_else);
                if (lower_stmts(b, prog, &prog->blocks[st->else_block]) != 0)
                    return -1;
                place_label(b, l_end);
            } else {
                place_label(b, l_else);
            }
            break;
        }
        }
    }
    return 0;
}

static int ir_build_fn(const char *file, const Fn *fn, const Program *prog,
                       IrFn *out) {
    IrBuild b;
    int rc, k;

    memset(out, 0, sizeof(*out));
    {
        size_t i = 0;
        while (fn->name[i] != '\0' && i < KAVIR_MAX_NAME - 1) {
            out->name[i] = fn->name[i];
            i++;
        }
        out->name[i] = '\0';
    }
    out->ret_is_i32 = (fn->ret_ty == KAVIR_TY_I32);
    out->ret_words = ty_words(prog, fn->ret_ty);
    out->nparams = fn->nparams;
    out->line = fn->line;
    out->col = fn->col;
    {
        for (k = 0; k < fn->nparams; k++) {
            int w = ty_words(prog, fn->param_ty[k]);
            out->param_words[k] = w;
            strncpy(out->param_names[k], fn->param_names[k],
                    KAVIR_MAX_NAME - 1);
            out->param_names[k][KAVIR_MAX_NAME - 1] = '\0';
        }
    }

    memset(&b, 0, sizeof(b));
    b.out = out;
    b.vreg = 0;
    b.nfix = 0;
    b.file = file;
    b.prog = prog;
    /* params claim their words first, in order -- the interpreter and
     * the backend lay the incoming ABI values into the same bases */
    for (k = 0; k < fn->nparams; k++) {
        int w = out->param_words[k];
        strncpy(b.vnames[b.nvnames], fn->param_names[k],
                KAVIR_MAX_NAME - 1);
        b.vnames[b.nvnames][KAVIR_MAX_NAME - 1] = '\0';
        b.vnums[b.nvnames] = b.vreg;
        b.nvnames++;
        b.vreg += w;
    }

    rc = lower_stmts(&b, prog, &prog->blocks[fn->body_block]);
    if (rc == 0) {
        fix_labels(&b);
        out->nvregs = b.vreg;
        if (out->nvregs > KAVIR_MAX_VREGS) {
            kavir_error(file, fn->line, fn->col,
                        "function too large (limit %d values per "
                        "function)", KAVIR_MAX_VREGS);
            rc = -1;
        }
    }
    return rc;
}

int ir_build(const char *file, const Program *prog, IrModule *mod) {
    int i;
    memset(mod, 0, sizeof(*mod));
    mod->nfns = prog->nfns;
    mod->nstrings = prog->nstrings;
    mod->file = NULL;
    for (i = 0; i < prog->nstrings; i++) {
        memcpy(mod->strings[i], prog->strings[i], (size_t)prog->str_lens[i]);
        mod->strings[i][prog->str_lens[i]] = '\0';
        mod->str_lens[i] = prog->str_lens[i];
    }
    for (i = 0; i < prog->nfns; i++)
        if (ir_build_fn(file, &prog->fns[i], prog, &mod->fns[i]) != 0)
            return -1;
    return 0;
}

/* prints s with \n, \t, \r, \\, \" re-escaped for the text dump */
static void dump_escaped(FILE *out, const char *s, int len) {
    int i;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '\n': fputs("\\n", out); break;
        case '\t': fputs("\\t", out); break;
        case '\r': fputs("\\r", out); break;
        case '\\': fputs("\\\\", out); break;
        case '"':  fputs("\\\"", out); break;
        default:
            if (c >= 32 && c < 127) fputc((int)c, out);
            else fprintf(out, "\\x%02x", (unsigned)c);
        }
    }
}

void ir_dump(const IrModule *mod, FILE *out) {
    int i, k, l;
    for (i = 0; i < mod->nfns; i++) {
        const IrFn *fn = &mod->fns[i];
        fprintf(out, "fn %s(", fn->name);
        for (k = 0; k < fn->nparams; k++)
            fprintf(out, "%s%s: i32", k > 0 ? ", " : "",
                    fn->param_names[k]);
        fprintf(out, ") -> %s {\n", fn->ret_is_i32 ? "i32" : "?");
        for (k = 0; k < fn->ninsts; k++) {
            const IrInst *in = &fn->insts[k];
            int li;
            for (li = 0; li < fn->nlabels; li++)
                if (fn->labels[li] == k) fprintf(out, "  L%d:\n", li);
            switch (in->op) {
            case IR_CONST:
                fprintf(out, "  %%r%d = const %lld\n", in->dest, in->val);
                break;
            case IR_RET:
                fprintf(out, "  ret %%r%d\n", in->src1);
                break;
            case IR_ADD: case IR_SUB: case IR_MUL:
            case IR_DIV: case IR_REM: {
                const char *name = in->op == IR_ADD ? "add" :
                                   in->op == IR_SUB ? "sub" :
                                   in->op == IR_MUL ? "mul" :
                                   in->op == IR_DIV ? "div" : "rem";
                fprintf(out, "  %%r%d = %s %%r%d, %%r%d\n",
                        in->dest, name, in->src1, in->src2);
                break;
            }
            case IR_PRINT:
                fprintf(out, "  print %%r%d\n", in->dest);
                break;
            case IR_PRINT_STR:
                fprintf(out, "  print_str \"");
                dump_escaped(out, mod->strings[in->str_idx],
                             mod->str_lens[in->str_idx]);
                fprintf(out, "\"\n");
                break;
            case IR_JMP:
                fprintf(out, "  jmp L%d\n", in->target);
                break;
            case IR_JMPZ:
                fprintf(out, "  jmpz %%r%d, L%d\n", in->src1, in->target);
                break;
            case IR_LT: case IR_LE: case IR_GT:
            case IR_GE: case IR_EQ: case IR_NE: {
                const char *name = in->op == IR_LT ? "lt" :
                                   in->op == IR_LE ? "le" :
                                   in->op == IR_GT ? "gt" :
                                   in->op == IR_GE ? "ge" :
                                   in->op == IR_EQ ? "eq" : "ne";
                fprintf(out, "  %%r%d = %s %%r%d, %%r%d\n",
                        in->dest, name, in->src1, in->src2);
                break;
            }
            case IR_COPY:
                fprintf(out, "  %%r%d = copy %%r%d\n", in->dest, in->src1);
                break;
            case IR_CALL: {
                int a;
                fprintf(out, "  %%r%d = call %s(", in->dest,
                        mod->fns[in->fn_idx].name);
                for (a = 0; a < in->nargs; a++)
                    fprintf(out, "%s%%r%d", a > 0 ? ", " : "",
                            in->args[a]);
                fprintf(out, ")\n");
                break;
            }
            }
        }
        for (l = 0; l < fn->nlabels; l++)
            if (fn->labels[l] == fn->ninsts) fprintf(out, "  L%d:\n", l);
        fprintf(out, "}\n");
    }
}
