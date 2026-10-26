/* interp.c — Kavir IR interpreter (M1).
 *
 * This is the executable reference semantics of the language
 * (ADR-005 v3 amendment b): the SAME IrModule the backend consumes is
 * executed here. Every codegen conformance test compares the ELF
 * program's observable behaviour (exit code; stdout from M2 on) against
 * this interpreter. A mismatch is a conformance failure.
 *
 * M1 semantics: functions are straight-line code ending in 'ret';
 * registers hold i32 values; main's return value is the process exit
 * code (truncated to & 0xFF by the OS, matching the ELF path).
 */
#include "kavir.h"
#include <string.h>

#define KAVIR_INTERP_MAX_REGS 64

long long interp_run(const IrModule *mod) {
    const IrFn *main_fn = NULL;
    long long regs[KAVIR_INTERP_MAX_REGS];
    int i;

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

    for (i = 0; i < main_fn->ninsts; i++) {
        const IrInst *in = &main_fn->insts[i];
        switch (in->op) {
        case IR_CONST:
            if (in->dest < KAVIR_INTERP_MAX_REGS)
                regs[in->dest] = in->val;
            break;
        case IR_RET:
            return regs[in->operand % KAVIR_INTERP_MAX_REGS];
        }
    }
    /* falling off the end returns 0 (documented M1 semantics) */
    return 0;
}
