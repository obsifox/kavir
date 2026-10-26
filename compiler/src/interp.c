/* interp.c — Kavir IR interpreter (M2).
 *
 * The executable reference semantics of the language (ADR-005 v3
 * amendment b): the SAME IrModule the backend consumes is executed
 * here, and every observable behaviour — stdout bytes, stderr trap
 * lines, exit codes — must match the compiled ELF byte for byte
 * (ADR-008 decisions 2/3/4; ADR-007 decision 2 adds the independent
 * tests/reference.py as the third arm of every conformance case).
 *
 * M2 semantics implemented here:
 *   - vregs hold i32 values (stored sign-exact in long long slots);
 *   - + - * / % execute at RUNTIME (no folding); an out-of-range
 *     result, division by zero, INT32_MIN / -1 and INT32_MIN % -1
 *     raise a trap: one line to stderr, exit code 101 (ADR-008);
 *   - print writes decimal (sign + digits, no padding) to stdout;
 *   - print_str writes the string-table bytes;
 *   - jmpz branches when the vreg is zero; evaluation order is
 *     statement order, lhs then rhs then op.
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

#define KAVIR_INTERP_MAX_REGS KAVIR_MAX_INSTS

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

int interp_run(const IrModule *mod, InterpResult *out) {
    const IrFn *main_fn = NULL;
    static long long regs[KAVIR_INTERP_MAX_REGS];
    int i, pc;

    out->trapped = 0;
    out->value = 0;
    out->trap_msg[0] = '\0';

    for (i = 0; i < mod->nfns; i++) {
        if (strcmp(mod->fns[i].name, "main") == 0) {
            main_fn = &mod->fns[i];
            break;
        }
    }
    if (main_fn == NULL) {
        /* the checker guarantees main exists; defensive only */
        fprintf(stderr, "interp: internal error: no main\n");
        return -1;
    }

    for (i = 0; i < KAVIR_INTERP_MAX_REGS; i++) regs[i] = 0;

    for (pc = 0; pc < main_fn->ninsts; pc++) {
        const IrInst *in = &main_fn->insts[pc];
        switch (in->op) {
        case IR_CONST:
            if (in->dest < KAVIR_INTERP_MAX_REGS)
                regs[in->dest] = in->val;
            break;
        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_REM: {
            long long r;
            if (arith(in->op, regs[in->src1], regs[in->src2], &r,
                      out, mod, in->line, in->col) != 0)
                return 0; /* trapped */
            regs[in->dest] = r;
            break;
        }
        case IR_PRINT:
            if (!g_quiet)
                printf("%lld", regs[in->dest]);
            break;
        case IR_PRINT_STR:
            if (!g_quiet)
                fwrite(mod->strings[in->str_idx], 1,
                       (size_t)mod->str_lens[in->str_idx], stdout);
            break;
        case IR_JMP:
            pc = in->target - 1; /* loop increment compensates */
            break;
        case IR_JMPZ:
            if (regs[in->src1] == 0)
                pc = in->target - 1;
            break;
        case IR_RET:
            out->value = regs[in->src1];
            return 0;
        }
    }
    /* falling off the end returns 0 (documented semantics; the checker
     * makes this unreachable for main) */
    out->value = 0;
    return 0;
}
