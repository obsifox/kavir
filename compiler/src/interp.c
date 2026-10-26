/* interp.c — Kavir IR interpreter (M3).
 *
 * The executable reference semantics of the language (ADR-005 v3
 * amendment b): the SAME IrModule the backend consumes is executed
 * here, and every observable behaviour — stdout bytes, stderr trap
 * lines, exit codes — must match the compiled ELF byte for byte
 * (ADR-008 decisions 2/3/4; ADR-007 decision 2 adds the independent
 * tests/reference.py as the third arm of every conformance case).
 *
 * M3 semantics implemented here:
 *   - vregs hold i32 values (stored sign-exact in long long slots);
 *   - + - * / % execute at RUNTIME (no folding); an out-of-range
 *     result, division by zero, INT32_MIN / -1 and INT32_MIN % -1
 *     raise a trap: one line to stderr, exit code 101 (ADR-008);
 *   - comparisons < <= > >= == != yield 1/0;
 *   - IR_COPY copies a vreg (lets and assignments);
 *   - IR_CALL executes a user function: an EXPLICIT frame stack (no
 *     C recursion) mirrors the machine's call stack; the frame index
 *     IS the Kavir call depth, capped at KAVIR_MAX_CALL_DEPTH (1024,
 *     user decision 2026-10-26, ADR-010) with a trap byte-identical to
 *     the ELF's prologue check;
 *   - print writes decimal (sign + digits, no padding) to stdout;
 *   - falling off the end of a function returns 0.
 *
 * Traps do NOT exit the process: interp_run reports them via
 * InterpResult so the fuzz harness can keep running; the CLI driver
 * maps a trap to KAVIR_TRAP_EXIT_CODE.
 */
#include "kavir.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#define KAVIR_INTERP_MAX_REGS KAVIR_MAX_VREGS

/* Explicit frame stack — mirrors the machine stack of the ELF without
 * consuming C stack. Frame i's depth is exactly i (matches the hidden
 * r11d argument in the backend: main=0, each call +1). */
typedef struct {
    int fn_idx;
    int pc;
    int call_dest; /* vreg in the CALLER receiving the result; -1 outer */
} Frame;

static Frame g_frames[KAVIR_MAX_CALL_DEPTH];
static long long g_iregs[KAVIR_MAX_CALL_DEPTH][KAVIR_INTERP_MAX_REGS];

static int g_quiet = 0;

void interp_set_quiet(int quiet) { g_quiet = quiet; }

static int fmt_trap(InterpResult *out, const IrModule *mod,
                    const char *kind, int line, int col) {
    out->trapped = 1;
    snprintf(out->trap_msg, sizeof(out->trap_msg),
             "trap: %s at %s:%d:%d\n", kind,
             mod->file != NULL ? mod->file : "?", line, col);
    return 1;
}

static int arith(IrOpKind op, long long a, long long b, long long *res,
                 InterpResult *out, const IrModule *mod,
                 int line, int col) {
    long long r;

    switch (op) {
    case IR_ADD:
        r = a + b;
        break;
    case IR_SUB:
        r = a - b;
        break;
    case IR_MUL:
        r = a * b; /* operands are i32; the product fits in 64 bits */
        break;
    case IR_DIV:
    case IR_REM:
        if (b == 0) {
            fmt_trap(out, mod, "division by zero", line, col);
            return -1;
        }
        if (a == INT32_MIN && b == -1) {
            /* ADR-007 implementation note: both INT_MIN / -1 and
             * INT_MIN % -1 trap (hardware-consistent). */
            fmt_trap(out, mod, "integer overflow", line, col);
            return -1;
        }
        r = (op == IR_DIV) ? (a / b) : (a % b); /* C truncates toward 0 */
        break;
    default:
        return -1;
    }
    if (r < (long long)INT32_MIN || r > (long long)INT32_MAX) {
        fmt_trap(out, mod, "integer overflow", line, col);
        return -1;
    }
    *res = r;
    return 0;
}

/* returns the comparison result (1/0); op is a comparison opcode */
static int compare(IrOpKind op, long long a, long long b) {
    switch (op) {
    case IR_LT: return a <  b;
    case IR_LE: return a <= b;
    case IR_GT: return a >  b;
    case IR_GE: return a >= b;
    case IR_EQ: return a == b;
    case IR_NE: return a != b;
    default:    return 0;
    }
}

int interp_run(const IrModule *mod, InterpResult *out) {
    int main_idx = -1;
    int i, sp;

    out->trapped = 0;
    out->value = 0;
    out->trap_msg[0] = '\0';

    for (i = 0; i < mod->nfns; i++) {
        if (strcmp(mod->fns[i].name, "main") == 0) {
            main_idx = i;
            break;
        }
    }
    if (main_idx < 0) {
        /* the checker guarantees main exists; defensive only */
        fprintf(stderr, "interp: internal error: no main\n");
        return -1;
    }

    sp = 0;
    g_frames[0].fn_idx = main_idx;
    g_frames[0].pc = 0;
    g_frames[0].call_dest = -1;
    for (i = 0; i < mod->fns[main_idx].nvregs; i++) g_iregs[0][i] = 0;

    for (;;) {
        const IrFn *fn;
        long long *caller_regs;
        long long *regs;
        const IrInst *in;

        if (sp < 0) break; /* returned from the outermost frame */
        fn = &mod->fns[g_frames[sp].fn_idx];
        regs = g_iregs[sp];

        if (g_frames[sp].pc >= fn->ninsts) {
            /* fell off the end: return 0 (documented semantics) */
            if (g_frames[sp].call_dest < 0) {
                out->value = 0;
                return 0;
            }
            caller_regs = g_iregs[sp - 1];
            caller_regs[g_frames[sp].call_dest] = 0;
            sp--;
            continue;
        }

        in = &fn->insts[g_frames[sp].pc];
        switch (in->op) {
        case IR_CONST:
            regs[in->dest] = in->val;
            g_frames[sp].pc++;
            break;
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_REM: {
            long long r;
            if (arith(in->op, regs[in->src1], regs[in->src2], &r,
                      out, mod, in->line, in->col) != 0)
                return 0; /* trapped */
            regs[in->dest] = r;
            g_frames[sp].pc++;
            break;
        }
        case IR_LT: case IR_LE: case IR_GT:
        case IR_GE: case IR_EQ: case IR_NE:
            regs[in->dest] = compare(in->op, regs[in->src1],
                                     regs[in->src2]);
            g_frames[sp].pc++;
            break;
        case IR_COPY:
            regs[in->dest] = regs[in->src1];
            g_frames[sp].pc++;
            break;
        case IR_PRINT:
            if (!g_quiet)
                printf("%lld", regs[in->dest]);
            g_frames[sp].pc++;
            break;
        case IR_PRINT_STR:
            if (!g_quiet)
                fwrite(mod->strings[in->str_idx], 1,
                       (size_t)mod->str_lens[in->str_idx], stdout);
            g_frames[sp].pc++;
            break;
        case IR_JMP:
            g_frames[sp].pc = in->target;
            break;
        case IR_JMPZ:
            g_frames[sp].pc = (regs[in->src1] == 0)
                                  ? in->target : g_frames[sp].pc + 1;
            break;
        case IR_CALL: {
            const IrFn *callee;
            int k;
            if (sp + 1 >= KAVIR_MAX_CALL_DEPTH) {
                /* identical rule to the ELF: entering the callee would
                 * be frame 1024; the trap names the callee and its
                 * definition position (ADR-010 section 3b) */
                const IrFn *c = &mod->fns[in->fn_idx];
                char kind[128];
                snprintf(kind, sizeof(kind),
                         "call depth limit exceeded (%d) in fn '%s'",
                         KAVIR_MAX_CALL_DEPTH, c->name);
                fmt_trap(out, mod, kind, c->line, c->col);
                return 0;
            }
            callee = &mod->fns[in->fn_idx];
            g_frames[sp].pc++; /* the caller resumes AFTER the call */
            caller_regs = g_iregs[sp];
            sp++;
            g_frames[sp].fn_idx = in->fn_idx;
            g_frames[sp].pc = 0;
            g_frames[sp].call_dest = in->dest;
            regs = g_iregs[sp];
            for (k = 0; k < callee->nvregs; k++) regs[k] = 0;
            for (k = 0; k < in->nargs; k++)
                regs[k] = caller_regs[in->args[k]]; /* args pre-evaluated */
            break;
        }
        case IR_RET: {
            long long rv = regs[in->src1];
            if (g_frames[sp].call_dest < 0) {
                out->value = rv;
                return 0;
            }
            caller_regs = g_iregs[sp - 1];
            caller_regs[g_frames[sp].call_dest] = rv;
            sp--; /* the caller's pc already points after its IR_CALL */
            break;
        }
        }
    }
    out->value = 0;
    return 0;
}
