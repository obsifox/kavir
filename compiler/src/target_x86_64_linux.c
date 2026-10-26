/* target_x86_64_linux.c — Kavir target backend (M3).
 *
 * ADR-002 v2: custom backend, DIRECT x86-64 machine-code emission
 * through our own encoder, internal ELF writer, static ET_EXEC, no libc,
 * direct Linux syscalls. No .s text, no as, no ld, no cc in this path.
 *
 * ADR-006 discipline rule: ALL x86-64/Linux-specific constants
 * (opcodes, encodings, ELF fields, syscall numbers, load base, ABI
 * register choices) live in THIS file only.
 *
 * Calling convention (ADR-010, user decision 2026-10-26): System V
 * AMD64 integer subset — first 6 integer arguments in RDI, RSI, RDX,
 * RCX, R8, R9; arguments 7+ on the caller stack (arg 7 at [rsp+0] /
 * [rbp+16] after the frame is built, right-to-left standard layout);
 * return value in EAX; RBP frame pointer; 16-byte stack alignment at
 * every call site; callee-saved registers (RBX, R12-R15) are simply
 * never allocated by this backend, so no save/restore code exists.
 *
 * Recursion depth (ADR-010 section 3b, user decision): a hidden depth
 * value travels in R11D (a caller-saved scratch register this backend
 * never uses for values). _start enters main with depth 0; every call
 * site loads its own depth, increments, and calls; every prologue
 * traps when the incoming depth would be KAVIR_MAX_CALL_DEPTH. The
 * interpreter enforces the identical rule with identical trap bytes.
 *
 * M3 lowering:
 *   - one stack slot per vreg at [rbp - 8*(vreg+1)] plus one depth
 *     slot at [rbp - 8*(nregs+1)]; everything spilled;
 *   - IR_CONST      -> mov dword [slot], imm32
 *   - IR_ADD/SUB    -> mov eax,[lhs]; add/sub eax,[rhs]; jo <trap>
 *   - IR_MUL        -> mov eax,[lhs]; imul eax,[rhs]; jo <trap>
 *   - IR_DIV/REM    -> mov eax,[lhs]; mov ecx,[rhs]; test/je/cmp guards
 *                      then cdq; idiv ecx (quotient eax / remainder edx)
 *   - IR_CMP        -> mov eax,[lhs]; cmp eax,[rhs]; setcc al;
 *                      movzx eax,al; mov [dest],eax
 *   - IR_COPY       -> mov eax,[src]; mov [dest],eax
 *   - IR_CALL       -> [stack args: sub rsp, mov [rsp+off],eax ...]
 *                      mov rdi/rsi/rdx/rcx/r8d/r9d,[argslots];
 *                      mov r11d,[depth]; inc r11d; call fn;
 *                      [add rsp]; mov [dest],eax
 *   - IR_PRINT      -> mov edi,[vreg]; call kavirrt_print_i32
 *   - IR_PRINT_STR  -> write(1, str, len) via syscall
 *   - IR_JMP/JMPZ   -> jmp/je rel32 to the target instruction
 *   - IR_RET        -> mov eax,[slot]; leave; ret
 *   - trap stubs: cold pool after all functions; each write(2,
 *     preformatted message) then jmp trap_exit -> exit_group(101)
 *   - image: ELF headers | functions | cold stubs | trap_exit | runtime
 *     | _start | rodata — ONE R+X PT_LOAD, vaddr = base + file offset
 */
#include "kavir.h"
#include <string.h>
#include <stdio.h>

/* ---- Linux/x86-64 ABI constants (this file only, per ADR-006) ---- */
#define LIN_WRITE_SYSCALL 1L
#define LIN_EXIT_GROUP_SYSCALL 231L   /* x86-64 syscall number */
#define LIN_LOAD_BASE 0x400000UL      /* static ET_EXEC base vaddr */
#define LIN_FD_STDOUT 1L
#define LIN_FD_STDERR 2L

/* SysV integer argument registers, in order (ADR-010): RDI RSI RDX RCX
 * R8 R9 — encoded inline in emit_load_arg_reg/emit_store_arg_reg below
 * (store = opcode 89, load = opcode 8B; r8d/r9d need REX.R = 0x44;
 * ModRM reg fields: edi=7 esi=6 edx=2 ecx=1 r8d=0 r9d=1). */

/* ---- ELF64 constants (this file only, per ADR-006) ---- */
#define ELF_MAGIC0 0x7f
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELF_CLASS64 2
#define ELF_DATA_LSB 1
#define ELF_VERSION_CURRENT 1
#define ELF_ET_EXEC 2
#define ELF_EM_X86_64 62
#define ELF_PT_LOAD 1
#define ELF_PF_R 4
#define ELF_PF_X 1

#define EHDR_SIZE 64
#define PHDR_SIZE 56
#define CODE_FILE_OFFSET (EHDR_SIZE + PHDR_SIZE) /* 120 */

/* ADR-008 decision 3: input paths longer than this are rejected at
 * compile time when a trap site exists (messages are baked in). */
#define KAVIR_MAX_TRAP_PATH 200

/* ---- cold-area capacity (ADR-008 decision 7) ---- */
#define KAVIR_MAX_ROTS 1024
#define KAVIR_MAX_FIXUPS 4096
#define KAVIR_MAX_ABS 4096
#define KAVIR_MAX_STUBS 1024

/* label ids: 0..7 fixed, 8..1031 trap stubs (incl. depth stubs), then
 * ad-hoc div-skip labels, then LBL_IR_BASE + fn_idx*KAVIR_MAX_INSTS +
 * instruction index (PER FUNCTION: one function's code generation must
 * not see another function's label positions), then LBL_FN_BASE +
 * function index. */
#define LBL_TRAP_EXIT 0
#define LBL_RT 1
#define LBL_RT_CONVERT 2
#define LBL_RT_OUT 3
#define LBL_ADHOC_BASE 8
#define LBL_IR_BASE 8192
#define LBL_FN_BASE (LBL_IR_BASE + KAVIR_MAX_FNS * KAVIR_MAX_INSTS)
#define KAVIR_MAX_LBL (LBL_FN_BASE + KAVIR_MAX_FNS)

/* per-function IR branch label */
static int ir_label(int fn_idx, int inst_idx) {
    return LBL_IR_BASE + fn_idx * KAVIR_MAX_INSTS + inst_idx;
}

/* ---- tiny emitter ---- */

typedef struct {
    unsigned char *buf;
    int cap, size;
    char *err;
    int errcap;
} Emit;

static int emit_fail(Emit *e, const char *msg) {
    if (e->err != NULL && e->errcap > 0)
        snprintf(e->err, (size_t)e->errcap, "%s", msg);
    return -1;
}

static void put8(Emit *e, unsigned v) {
    if (e->size < e->cap) e->buf[e->size] = (unsigned char)(v & 0xff);
    e->size++;
}

static void put16(Emit *e, unsigned long v) {
    put8(e, (unsigned)(v & 0xff));
    put8(e, (unsigned)((v >> 8) & 0xff));
}

static void put32(Emit *e, unsigned long v) {
    put16(e, v & 0xffff);
    put16(e, (v >> 16) & 0xffff);
}

static void put64(Emit *e, unsigned long long v) {
    put32(e, (unsigned long)(v & 0xffffffffUL));
    put32(e, (unsigned long)((v >> 32) & 0xffffffffUL));
}

static void patch32(Emit *e, int at, unsigned long v) {
    if (at < 0 || at + 4 > e->cap || at + 4 > e->size) {
        emit_fail(e, "internal: patch out of range");
        return;
    }
    e->buf[at]     = (unsigned char)(v & 0xff);
    e->buf[at + 1] = (unsigned char)((v >> 8) & 0xff);
    e->buf[at + 2] = (unsigned char)((v >> 16) & 0xff);
    e->buf[at + 3] = (unsigned char)((v >> 24) & 0xff);
}

/* ---- rodata: string literals + preformatted trap messages ---- */

typedef struct {
    char data[256];
    int len;
    int offset; /* within the rodata area; assigned after code emission */
} RotStr;

static RotStr g_rots[KAVIR_MAX_ROTS];
static int g_nrots;

typedef struct { int at; int rot; } AbsFix;
static AbsFix g_abs[KAVIR_MAX_ABS];
static int g_nabs;

/* adds s to rodata; returns its index (insertion order), or -1.
 * No dedup: IR string index == rot index is relied upon. */
static int rot_add(const char *s, int len) {
    if (g_nrots >= KAVIR_MAX_ROTS) return -1;
    if (len < 0 || len >= (int)sizeof(g_rots[0].data)) return -1;
    memcpy(g_rots[g_nrots].data, s, (size_t)len);
    g_rots[g_nrots].len = len;
    g_rots[g_nrots].offset = -1;
    return g_nrots++;
}

/* ---- rel32 code fixups (forward jumps) ---- */

typedef struct { int at; int label; } Fixup;
static Fixup g_fix[KAVIR_MAX_FIXUPS];
static int g_nfix;
static int g_lbl_pos[KAVIR_MAX_LBL];
/* ad-hoc labels (div-skip) are GLOBAL across functions: a per-function
 * counter would collide when patching forward references at the end */
static int g_nadhoc;

static void fix_rel32(Emit *e, int at, int label) {
    if (g_lbl_pos[label] >= 0) {
        /* backward (or already placed): patch immediately */
        patch32(e, at, (unsigned long)(g_lbl_pos[label] - (at + 4)));
        return;
    }
    if (g_nfix >= KAVIR_MAX_FIXUPS) {
        emit_fail(e, "too many code fixups");
        return;
    }
    g_fix[g_nfix].at = at;
    g_fix[g_nfix].label = label;
    g_nfix++;
}

static void place_lbl_at(Emit *e, int label) { g_lbl_pos[label] = e->size; }

static void fix_abs32(Emit *e, int at, int rot) {
    if (g_nabs >= KAVIR_MAX_ABS) {
        emit_fail(e, "too many rodata references");
        return;
    }
    g_abs[g_nabs].at = at;
    g_abs[g_nabs].rot = rot;
    g_nabs++;
}

/* ---- x86-64 encoders (this file only, per ADR-006) ----
 * Registers used for values: eax=0 ecx=1 edx=2 esi=6 edi=7.
 * Hidden depth travels in r11d. [rbp+disp32] ModRM: mod=10 rm=101 ->
 * 0x85 | (reg << 3). */

static void emit_mov_eax_imm32(Emit *e, long long v) { put8(e, 0xB8); put32(e, (unsigned long)(v & 0xffffffffUL)); }
static void emit_mov_ecx_imm32(Emit *e, long long v) { put8(e, 0xB9); put32(e, (unsigned long)(v & 0xffffffffUL)); }
static void emit_mov_edx_imm32(Emit *e, long long v) { put8(e, 0xBA); put32(e, (unsigned long)(v & 0xffffffffUL)); }
static void emit_mov_esi_imm32(Emit *e, long long v) { put8(e, 0xBE); put32(e, (unsigned long)(v & 0xffffffffUL)); }
static void emit_mov_edi_imm32(Emit *e, long long v) { put8(e, 0xBF); put32(e, (unsigned long)(v & 0xffffffffUL)); }
static void emit_mov_edi_eax(Emit *e) { put8(e, 0x89); put8(e, 0xC7); }
static void emit_xor_eax_eax(Emit *e) { put8(e, 0x31); put8(e, 0xC0); }
static void emit_xor_edx_edx(Emit *e) { put8(e, 0x31); put8(e, 0xD2); }
static void emit_xor_r11d_r11d(Emit *e) { put8(e, 0x45); put8(e, 0x31); put8(e, 0xDB); }
static void emit_push_rbp(Emit *e) { put8(e, 0x55); }
static void emit_mov_rbp_rsp(Emit *e) { put8(e, 0x48); put8(e, 0x89); put8(e, 0xE5); }
static void emit_leave(Emit *e) { put8(e, 0xC9); }
static void emit_ret(Emit *e)   { put8(e, 0xC3); }
static void emit_syscall(Emit *e) { put8(e, 0x0F); put8(e, 0x05); }
static void emit_cdq(Emit *e)      { put8(e, 0x99); }
static void emit_idiv_ecx(Emit *e) { put8(e, 0xF7); put8(e, 0xF9); }
static void emit_test_ecx_ecx(Emit *e) { put8(e, 0x85); put8(e, 0xC9); }
static void emit_test_eax_eax(Emit *e) { put8(e, 0x85); put8(e, 0xC0); }
static void emit_test_edi_edi(Emit *e) { put8(e, 0x85); put8(e, 0xFF); }
static void emit_cmp_ecx_m1(Emit *e)   { put8(e, 0x83); put8(e, 0xF9); put8(e, 0xFF); }
static void emit_cmp_eax_imm32(Emit *e, long long v) { put8(e, 0x3D); put32(e, (unsigned long)(v & 0xffffffffUL)); }

/* depth plumbing: cmp r11d, imm32 / inc r11d / mov r11d <-> [rbp+disp32] */
static void emit_cmp_r11d_imm32(Emit *e, long long v) {
    put8(e, 0x41); put8(e, 0x81); put8(e, 0xFB);
    put32(e, (unsigned long)(v & 0xffffffffUL));
}
static void emit_inc_r11d(Emit *e) { put8(e, 0x41); put8(e, 0xFF); put8(e, 0xC3); }
static void emit_mov_r11d_m(Emit *e, int disp) {
    put8(e, 0x44); put8(e, 0x8B); put8(e, 0x9D);
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
}
static void emit_mov_m_r11d(Emit *e, int disp) {
    put8(e, 0x44); put8(e, 0x89); put8(e, 0x9D);
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
}

/* r8d/r9d loads/stores through [rbp+disp32] (REX.R forms) */
static void emit_mov_r8d_m(Emit *e, int disp) {
    put8(e, 0x44); put8(e, 0x8B); put8(e, 0x85);
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
}
static void emit_mov_r9d_m(Emit *e, int disp) {
    put8(e, 0x44); put8(e, 0x8B); put8(e, 0x8D);
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
}
static void emit_mov_m_r8d(Emit *e, int disp) {
    put8(e, 0x44); put8(e, 0x89); put8(e, 0x85);
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
}
static void emit_mov_m_r9d(Emit *e, int disp) {
    put8(e, 0x44); put8(e, 0x89); put8(e, 0x8D);
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
}

static void emit_sub_rsp_imm(Emit *e, int imm) {
    if (imm >= -128 && imm <= 127) {
        put8(e, 0x48); put8(e, 0x83); put8(e, 0xEC); put8(e, (unsigned)imm);
    } else {
        put8(e, 0x48); put8(e, 0x81); put8(e, 0xEC);
        put32(e, (unsigned long)((unsigned)imm & 0xffffffffUL));
    }
}

static void emit_add_rsp_imm(Emit *e, int imm) {
    if (imm >= -128 && imm <= 127) {
        put8(e, 0x48); put8(e, 0x83); put8(e, 0xC4); put8(e, (unsigned)imm);
    } else {
        put8(e, 0x48); put8(e, 0x81); put8(e, 0xC4);
        put32(e, (unsigned long)((unsigned)imm & 0xffffffffUL));
    }
}

/* mov r32, [rbp+disp32] — reg: 0=eax 1=ecx 2=edx 6=esi 7=edi */
static void emit_mov_r_m(Emit *e, int reg, int disp) {
    put8(e, 0x8B);
    put8(e, (unsigned)(0x85 | (reg << 3)));
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
}

/* mov [rbp+disp32], r32 — reg: 0=eax 1=ecx 2=edx 6=esi 7=edi */
static void emit_mov_m_r(Emit *e, int disp, int reg) {
    put8(e, 0x89);
    put8(e, (unsigned)(0x85 | (reg << 3)));
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
}

/* mov dword [rbp+disp32], imm32 */
static void emit_mov_m_imm32(Emit *e, int disp, long long imm) {
    put8(e, 0xC7);
    put8(e, 0x85);
    put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL));
    put32(e, (unsigned long)(imm & 0xffffffffUL));
}

static void emit_add_eax_m(Emit *e, int disp)  { put8(e, 0x03); put8(e, 0x85); put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL)); }
static void emit_sub_eax_m(Emit *e, int disp)  { put8(e, 0x2B); put8(e, 0x85); put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL)); }
static void emit_imul_eax_m(Emit *e, int disp) { put8(e, 0x0F); put8(e, 0xAF); put8(e, 0x85); put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL)); }
static void emit_cmp_eax_m(Emit *e, int disp)  { put8(e, 0x3B); put8(e, 0x85); put32(e, (unsigned long)((unsigned)disp & 0xffffffffUL)); }

/* setcc al / movzx eax,al — comparison results (ADR-010 decision 2) */
static void emit_setcc_al(Emit *e, int cc) {
    put8(e, 0x0F);
    put8(e, (unsigned)cc);
    put8(e, 0xC0);
}
static void emit_movzx_eax_al(Emit *e) { put8(e, 0x0F); put8(e, 0xB6); put8(e, 0xC0); }

/* store a stack argument: mov [rsp+off], eax (off is 0 or 8 in practice) */
static void emit_mov_rsp_eax(Emit *e, int off) {
    if (off == 0) {
        put8(e, 0x89); put8(e, 0x04); put8(e, 0x24);
    } else {
        put8(e, 0x89); put8(e, 0x44); put8(e, 0x24);
        put8(e, (unsigned)off);
    }
}

/* 64-bit helpers for the emitted runtime (digit conversion) */
static void emit_movsxd_rax_edi(Emit *e) { put8(e, 0x48); put8(e, 0x63); put8(e, 0xC7); }
static void emit_lea_r9_rsp40(Emit *e)   { put8(e, 0x4C); put8(e, 0x8D); put8(e, 0x4C); put8(e, 0x24); put8(e, 0x28); }
static void emit_neg_rax(Emit *e)        { put8(e, 0x48); put8(e, 0xF7); put8(e, 0xD8); }
static void emit_div_rcx(Emit *e)        { put8(e, 0x48); put8(e, 0xF7); put8(e, 0xF1); }
static void emit_add_edx_48(Emit *e)     { put8(e, 0x81); put8(e, 0xC2); put32(e, 48); }
static void emit_dec_r9(Emit *e)         { put8(e, 0x49); put8(e, 0xFF); put8(e, 0xC9); }
static void emit_mov_r9_dl(Emit *e)      { put8(e, 0x41); put8(e, 0x88); put8(e, 0x11); }
static void emit_test_rax_rax(Emit *e)   { put8(e, 0x48); put8(e, 0x85); put8(e, 0xC0); }
static void emit_mov_r9_dash(Emit *e)    { put8(e, 0x41); put8(e, 0xC6); put8(e, 0x01); put8(e, 0x2D); }
static void emit_mov_rsi_r9(Emit *e)     { put8(e, 0x4C); put8(e, 0x89); put8(e, 0xCE); }
static void emit_mov_rdx_rsp(Emit *e)    { put8(e, 0x48); put8(e, 0x89); put8(e, 0xE2); }
static void emit_add_rdx_40(Emit *e)     { put8(e, 0x48); put8(e, 0x83); put8(e, 0xC2); put8(e, 0x28); }
static void emit_sub_rdx_rsi(Emit *e)    { put8(e, 0x48); put8(e, 0x29); put8(e, 0xF2); }
static void emit_add_rsp_48(Emit *e)     { put8(e, 0x48); put8(e, 0x83); put8(e, 0xC4); put8(e, 0x30); }

/* jcc rel32 into a (possibly forward) label — emit_jcc takes the SECOND
 * opcode byte directly (0x80+jcc code on the wire): */

static void emit_jcc(Emit *e, int opcode_low, int label) {
    /* opcode_low is the SECOND opcode byte (0x83 = jae, 0x84 = je, ...) */
    int at;
    put8(e, 0x0F);
    put8(e, (unsigned)opcode_low);
    at = e->size;
    put32(e, 0);        /* reserve the rel32 first so patches are in range */
    fix_rel32(e, at, label);
}

/* named condition-code opcodes used in this file (0x0F + byte) */
#define JCC_O   0x80
#define JCC_AE  0x83
#define JCC_E   0x84
#define JCC_NE  0x85
#define JCC_NS  0x89

static void emit_jmp(Emit *e, int label) {
    int at;
    put8(e, 0xE9);
    at = e->size;
    put32(e, 0);
    fix_rel32(e, at, label);
}

static void emit_call(Emit *e, int label) {
    int at;
    put8(e, 0xE8);
    at = e->size;
    put32(e, 0);
    fix_rel32(e, at, label);
}

/* ---- emitted freestanding runtime (no libc, ADR-002 v2) ---- */

static void emit_print_i32_runtime(Emit *e) {
    /* kavirrt_print_i32: edi = i32 value; writes decimal to fd 1.
     * Clobbers rax, rcx, rdx, rsi, rdi, r9. */
    place_lbl_at(e, LBL_RT);
    emit_sub_rsp_imm(e, 48);            /* digit scratch below the frame  */
    emit_movsxd_rax_edi(e);             /* rax = (i64)value               */
    emit_lea_r9_rsp40(e);               /* r9 = digits end pointer        */
    emit_mov_ecx_imm32(e, 10);          /* divisor                        */
    emit_test_edi_edi(e);
    emit_jcc(e, JCC_NS, LBL_RT_CONVERT); /* positive: skip the negate     */
    emit_neg_rax(e);                    /* 64-bit neg: safe for INT32_MIN */
    place_lbl_at(e, LBL_RT_CONVERT);
    /* digit loop (runs at least once, so 0 prints as "0") */
    emit_xor_edx_edx(e);
    emit_div_rcx(e);                    /* rax = q, rdx = digit           */
    emit_add_edx_48(e);                 /* '0'                            */
    emit_dec_r9(e);
    emit_mov_r9_dl(e);
    emit_test_rax_rax(e);
    emit_jcc(e, JCC_NE, LBL_RT_CONVERT); /* backward, already placed      */
    emit_test_edi_edi(e);
    emit_jcc(e, JCC_NS, LBL_RT_OUT);
    emit_dec_r9(e);
    emit_mov_r9_dash(e);                /* leading '-'                    */
    place_lbl_at(e, LBL_RT_OUT);
    emit_mov_rsi_r9(e);                 /* buf                            */
    emit_mov_rdx_rsp(e);
    emit_add_rdx_40(e);                 /* end - buf = length             */
    emit_sub_rdx_rsi(e);
    emit_mov_edi_imm32(e, LIN_FD_STDOUT);
    emit_mov_eax_imm32(e, LIN_WRITE_SYSCALL);
    emit_syscall(e);
    emit_add_rsp_48(e);
    emit_ret(e);
}

/* ---- code generation ---- */

typedef struct {
    int label; /* stub entry label */
    int rot;   /* message index in rodata */
} TrapStub;

static TrapStub g_stubs[KAVIR_MAX_STUBS];
static int g_nstubs;

static int add_trap_stub(Emit *e, const char *msg, int len) {
    int rot, lbl;
    rot = rot_add(msg, len);
    if (rot < 0) { emit_fail(e, "too many trap sites"); return -1; }
    if (g_nstubs >= KAVIR_MAX_STUBS) {
        emit_fail(e, "too many trap sites");
        return -1;
    }
    lbl = LBL_ADHOC_BASE + g_nstubs;
    g_stubs[g_nstubs].label = lbl;
    g_stubs[g_nstubs].rot = rot;
    g_nstubs++;
    return lbl;
}

/* one depth stub per FUNCTION; its message names the callee whose
 * prologue rejected the entry (identical bytes in the interpreter) */
static int add_depth_stub(Emit *e, const char *file, const IrFn *fn) {
    char msg[256];
    int ml;
    ml = snprintf(msg, sizeof(msg),
                  "trap: call depth limit exceeded (%d) in fn '%s' "
                  "at %s:%d:%d\n", KAVIR_MAX_CALL_DEPTH, fn->name,
                  file, fn->line, fn->col);
    return add_trap_stub(e, msg, ml);
}

static int add_overflow_stub(Emit *e, const char *file, int line, int col) {
    char msg[256];
    int ml;
    ml = snprintf(msg, sizeof(msg),
                  "trap: integer overflow at %s:%d:%d\n", file, line, col);
    return add_trap_stub(e, msg, ml);
}

static int add_divzero_stub(Emit *e, const char *file, int line, int col) {
    char msg[256];
    int ml;
    ml = snprintf(msg, sizeof(msg),
                  "trap: division by zero at %s:%d:%d\n", file, line, col);
    return add_trap_stub(e, msg, ml);
}

/* loads argument register i (0..5) from its vreg slot */
static void emit_load_arg_reg(Emit *e, int i, int vreg) {
    int disp = -8 * (vreg + 1);
    switch (i) {
    case 0: emit_mov_r_m(e, 7, disp); break;             /* edi */
    case 1: emit_mov_r_m(e, 6, disp); break;             /* esi */
    case 2: emit_mov_r_m(e, 2, disp); break;             /* edx */
    case 3: emit_mov_r_m(e, 1, disp); break;             /* ecx */
    case 4: emit_mov_r8d_m(e, disp); break;              /* r8d */
    case 5: emit_mov_r9d_m(e, disp); break;              /* r9d */
    }
}

/* stores argument register i into the callee's param vreg slot
 * (used in the prologue) */
static void emit_store_arg_reg(Emit *e, int i, int vreg) {
    int disp = -8 * (vreg + 1);
    switch (i) {
    case 0: emit_mov_m_r(e, disp, 7); break;             /* edi */
    case 1: emit_mov_m_r(e, disp, 6); break;             /* esi */
    case 2: emit_mov_m_r(e, disp, 2); break;             /* edx */
    case 3: emit_mov_m_r(e, disp, 1); break;             /* ecx */
    case 4: emit_mov_m_r8d(e, disp); break;              /* r8d */
    case 5: emit_mov_m_r9d(e, disp); break;              /* r9d */
    }
}

static int cmp_setcc_byte(IrOpKind op) {
    switch (op) {
    case IR_LT: return 0x9C; /* setl  */
    case IR_LE: return 0x9E; /* setle */
    case IR_GT: return 0x9F; /* setg  */
    case IR_GE: return 0x9D; /* setge */
    case IR_EQ: return 0x94; /* sete  */
    case IR_NE: return 0x95; /* setne */
    default:    return 0;
    }
}

static int gen_function(Emit *e, const char *file, const IrModule *mod,
                        int fn_idx, const IrFn *fn) {
    int k, nregs = 0, slots, depth_disp, rc_depth;

    /* pass 1: vreg count (dest is written by CONST/arith/cmp/COPY/CALL) */
    for (k = 0; k < fn->ninsts; k++) {
        const IrInst *in = &fn->insts[k];
        int writes = (in->op == IR_CONST) ||
                     (in->op >= IR_ADD && in->op <= IR_REM) ||
                     (in->op >= IR_LT && in->op <= IR_NE) ||
                     (in->op == IR_COPY) || (in->op == IR_CALL);
        if (writes && in->dest + 1 > nregs) nregs = in->dest + 1;
    }
    /* +1 slot for the hidden depth value (ADR-010 section 3b) */
    slots = ((8 * (nregs + 1)) + 15) & ~15;
    depth_disp = -8 * (nregs + 1);

    /* prologue: depth check first, then frame + depth spill + args.
     * Trap when the INCOMING depth is >= KAVIR_MAX_CALL_DEPTH (frame
     * indices 0..KAVIR_MAX_CALL_DEPTH-1 are legal — identical rule in
     * the interpreter and the oracle). */
    rc_depth = add_depth_stub(e, file, fn);
    if (rc_depth < 0) return -1;
    emit_cmp_r11d_imm32(e, KAVIR_MAX_CALL_DEPTH);
    emit_jcc(e, JCC_AE, rc_depth);

    emit_push_rbp(e);
    emit_mov_rbp_rsp(e);
    emit_sub_rsp_imm(e, slots);
    emit_mov_m_r11d(e, depth_disp);

    /* parameter copies: register args, then stack args ([rbp+16+8i]) */
    for (k = 0; k < fn->nparams; k++) {
        if (k < 6) {
            emit_store_arg_reg(e, k, k);
        } else {
            emit_mov_r_m(e, 0, 16 + 8 * (k - 6)); /* eax = stack arg */
            emit_mov_m_r(e, -8 * (k + 1), 0);
        }
    }

    for (k = 0; k < fn->ninsts; k++) {
        const IrInst *in = &fn->insts[k];
        /* branch target of IR inst k is the start of its machine code */
        g_lbl_pos[ir_label(fn_idx, k)] = e->size;

        switch (in->op) {
        case IR_CONST:
            emit_mov_m_imm32(e, -8 * (in->dest + 1), in->val);
            break;

        case IR_ADD: case IR_SUB: case IR_MUL: {
            int stub;
            emit_mov_r_m(e, 0, -8 * (in->src1 + 1));
            if (in->op == IR_ADD)      emit_add_eax_m(e, -8 * (in->src2 + 1));
            else if (in->op == IR_SUB) emit_sub_eax_m(e, -8 * (in->src2 + 1));
            else                       emit_imul_eax_m(e, -8 * (in->src2 + 1));
            stub = add_overflow_stub(e, file, in->line, in->col);
            if (stub < 0) return -1;
            emit_jcc(e, JCC_O, stub);
            emit_mov_m_r(e, -8 * (in->dest + 1), 0);
            break;
        }

        case IR_DIV: case IR_REM: {
            int rot0, rot1, lbl0, lbl1, skip_lbl;
            rot0 = add_divzero_stub(e, file, in->line, in->col);
            if (rot0 < 0) return -1;
            lbl0 = g_stubs[g_nstubs - 1].label;
            rot1 = add_overflow_stub(e, file, in->line, in->col);
            if (rot1 < 0) return -1;
            lbl1 = g_stubs[g_nstubs - 1].label;
            if (g_nadhoc >= LBL_IR_BASE) {
                emit_fail(e, "too many division sites");
                return -1;
            }
            skip_lbl = g_nadhoc++;

            emit_mov_r_m(e, 0, -8 * (in->src1 + 1));   /* eax = lhs      */
            emit_mov_r_m(e, 1, -8 * (in->src2 + 1));   /* ecx = rhs      */
            emit_test_ecx_ecx(e);
            emit_jcc(e, JCC_E, lbl0);                  /* division by 0  */
            emit_cmp_ecx_m1(e);
            emit_jcc(e, JCC_NE, skip_lbl);
            emit_cmp_eax_imm32(e, (long long)0x80000000);
            emit_jcc(e, JCC_E, lbl1);                  /* INT_MIN over -1*/
            place_lbl_at(e, skip_lbl);
            emit_cdq(e);
            emit_idiv_ecx(e);
            if (in->op == IR_DIV) emit_mov_m_r(e, -8 * (in->dest + 1), 0);
            else emit_mov_m_r(e, -8 * (in->dest + 1), 2); /* edx = rem   */
            break;
        }

        case IR_LT: case IR_LE: case IR_GT:
        case IR_GE: case IR_EQ: case IR_NE:
            emit_mov_r_m(e, 0, -8 * (in->src1 + 1));
            emit_cmp_eax_m(e, -8 * (in->src2 + 1));
            emit_setcc_al(e, cmp_setcc_byte(in->op));
            emit_movzx_eax_al(e);
            emit_mov_m_r(e, -8 * (in->dest + 1), 0);
            break;

        case IR_COPY:
            emit_mov_r_m(e, 0, -8 * (in->src1 + 1));
            emit_mov_m_r(e, -8 * (in->dest + 1), 0);
            break;

        case IR_CALL: {
            int i, nstack, stack_bytes;
            nstack = in->nargs > 6 ? in->nargs - 6 : 0;
            stack_bytes = nstack ? (((nstack + 1) & ~1) * 8) : 0;
            if (nstack) emit_sub_rsp_imm(e, stack_bytes);
            /* stack args: arg i (i>=6) lives at [rsp + 8*(i-6)]; that
             * puts arg 7 at [rsp+0] = [rbp+16] in the callee (SysV) */
            for (i = 6; i < in->nargs; i++) {
                emit_mov_r_m(e, 0, -8 * (in->args[i] + 1));
                emit_mov_rsp_eax(e, 8 * (i - 6));
            }
            for (i = 0; i < 6 && i < in->nargs; i++)
                emit_load_arg_reg(e, i, in->args[i]);
            emit_mov_r11d_m(e, depth_disp);
            emit_inc_r11d(e);
            emit_call(e, LBL_FN_BASE + in->fn_idx);
            if (nstack) emit_add_rsp_imm(e, stack_bytes);
            emit_mov_m_r(e, -8 * (in->dest + 1), 0);
            break;
        }

        case IR_PRINT:
            emit_mov_r_m(e, 7, -8 * (in->dest + 1));   /* edi = value    */
            emit_call(e, LBL_RT);
            break;

        case IR_PRINT_STR:
            emit_mov_edi_imm32(e, LIN_FD_STDOUT);
            emit_mov_esi_imm32(e, 0);      /* patched to the rodata vaddr */
            fix_abs32(e, e->size - 4, in->str_idx);
            emit_mov_edx_imm32(e, mod->str_lens[in->str_idx]);
            emit_mov_eax_imm32(e, LIN_WRITE_SYSCALL);
            emit_syscall(e);
            break;

        case IR_JMP:
            emit_jmp(e, ir_label(fn_idx, in->target));
            break;

        case IR_JMPZ:
            emit_mov_r_m(e, 0, -8 * (in->src1 + 1));
            emit_test_eax_eax(e);
            emit_jcc(e, JCC_E, ir_label(fn_idx, in->target));
            break;

        case IR_RET:
            emit_mov_r_m(e, 0, -8 * (in->src1 + 1));
            emit_leave(e);
            emit_ret(e);
            /* keep generating: early returns are mid-function in M3;
             * code after a RET is reachable via jumps. If the RET is
             * the last instruction, the epilogue below is dead bytes. */
            break;
        }
    }
    /* falling off the end: return 0 (must match interpreter semantics) */
    emit_xor_eax_eax(e);
    emit_leave(e);
    emit_ret(e);
    return 0;
}

/* ---- ELF writer ---- */

static int write_elf(Emit *e, int start_pos, int code_end, int *entry_file_off) {
    int total;
    unsigned long entry_vaddr;

    *entry_file_off = CODE_FILE_OFFSET + start_pos;
    entry_vaddr = LIN_LOAD_BASE + (unsigned long)*entry_file_off;
    total = code_end + (e->size - code_end); /* code + rodata */

    if (CODE_FILE_OFFSET + total > e->cap)
        return emit_fail(e, "code buffer overflow");

    /* The image was generated starting at buf[0]; shift it once to make
     * room for the ELF header + program header at the file start. The
     * relative layout (and every rel32 patch) survives the shift; the
     * absolute rodata vaddrs were patched with CODE_FILE_OFFSET included
     * beforehand. */
    memmove(e->buf + CODE_FILE_OFFSET, e->buf, (size_t)total);
    e->size = 0;

    /* --- ELF header (64 bytes) --- */
    put8(e, ELF_MAGIC0); put8(e, ELF_MAGIC1); put8(e, ELF_MAGIC2); put8(e, ELF_MAGIC3);
    put8(e, ELF_CLASS64); put8(e, ELF_DATA_LSB); put8(e, ELF_VERSION_CURRENT);
    put8(e, 0); /* ELFOSABI_NONE (SYSV) */
    put8(e, 0); /* ABIVERSION */
    { int i; for (i = 0; i < 7; i++) put8(e, 0); } /* e_ident padding */
    put16(e, ELF_ET_EXEC);        /* e_type */
    put16(e, ELF_EM_X86_64);      /* e_machine */
    put32(e, 1);                  /* e_version */
    put64(e, entry_vaddr);        /* e_entry */
    put64(e, (unsigned long)EHDR_SIZE); /* e_phoff */
    put64(e, 0);                  /* e_shoff (no section headers) */
    put32(e, 0);                  /* e_flags */
    put16(e, EHDR_SIZE);          /* e_ehsize */
    put16(e, PHDR_SIZE);          /* e_phentsize */
    put16(e, 1);                  /* e_phnum */
    put16(e, 0);                  /* e_shentsize */
    put16(e, 0);                  /* e_shnum */
    put16(e, 0);                  /* e_shstrndx */

    /* --- program header: one R+X PT_LOAD covering the whole image ---
     * (no RW segment yet: no globals, no mutable data — ADR-008 §6) */
    put32(e, ELF_PT_LOAD);
    put32(e, ELF_PF_R | ELF_PF_X);
    put64(e, 0);                /* p_offset */
    put64(e, LIN_LOAD_BASE);    /* p_vaddr  */
    put64(e, LIN_LOAD_BASE);    /* p_paddr  */
    put64(e, (unsigned long)(CODE_FILE_OFFSET + total)); /* p_filesz */
    put64(e, (unsigned long)(CODE_FILE_OFFSET + total)); /* p_memsz  */
    put64(e, 0x1000UL);         /* p_align  */

    if (e->size != CODE_FILE_OFFSET)
        return emit_fail(e, "internal: ELF header size mismatch");

    e->size = CODE_FILE_OFFSET + total;
    return 0;
}

int target_x86_64_linux_emit(const IrModule *mod, unsigned char *buf,
                             int cap, int *out_size, int *entry_file_offset,
                             char *errbuf, int errcap) {
    Emit e;
    int main_idx = -1;
    const char *file;
    int i, rc, code_end = 0, start_pos = 0;
    int rodata_start, rosize, has_trap_sites = 0;

    e.buf = buf; e.cap = cap; e.size = 0;
    e.err = errbuf; e.errcap = errcap;

    /* reset per-emit static state (the fuzz harness emits repeatedly) */
    for (i = 0; i < KAVIR_MAX_LBL; i++) g_lbl_pos[i] = -1;
    g_nfix = 0; g_nabs = 0; g_nrots = 0; g_nstubs = 0;
    /* ad-hoc labels live ABOVE the trap-stub range (8..1031) and below
     * the per-function IR label space (LBL_IR_BASE..) */
    g_nadhoc = LBL_ADHOC_BASE + KAVIR_MAX_STUBS;

    file = (mod->file != NULL) ? mod->file : "?";

    /* string literals first, so rot index == string index */
    for (i = 0; i < mod->nstrings; i++)
        if (rot_add(mod->strings[i], mod->str_lens[i]) < 0)
            return emit_fail(&e, "too many rodata entries");

    for (i = 0; i < mod->nfns; i++) {
        int k;
        if (strcmp(mod->fns[i].name, "main") == 0) main_idx = i;
        for (k = 0; k < mod->fns[i].ninsts; k++) {
            IrOpKind op = mod->fns[i].insts[k].op;
            if ((op >= IR_ADD && op <= IR_REM) || op == IR_CALL)
                has_trap_sites = 1;
        }
    }
    if (main_idx < 0) return emit_fail(&e, "internal: no main in IR");
    if (has_trap_sites && (int)strlen(file) > KAVIR_MAX_TRAP_PATH)
        return emit_fail(&e, "input path too long for trap diagnostics "
                             "(max 200 bytes)");

    /* all functions, in IR order (every prologue registers its depth
     * stub in the cold pool; arith/div stubs register inline). IR branch
     * labels are per-function (ir_label) so generation order never
     * leaks positions across functions. */
    for (i = 0; i < mod->nfns; i++) {
        place_lbl_at(&e, LBL_FN_BASE + i);
        rc = gen_function(&e, file, mod, i, &mod->fns[i]);
        if (rc != 0) return -1;
    }

    /* cold area: trap stubs */
    for (i = 0; i < g_nstubs; i++) {
        place_lbl_at(&e, g_stubs[i].label);
        emit_mov_esi_imm32(&e, 0); /* patched with the message vaddr */
        fix_abs32(&e, e.size - 4, g_stubs[i].rot);
        emit_mov_edx_imm32(&e, g_rots[g_stubs[i].rot].len);
        emit_mov_edi_imm32(&e, LIN_FD_STDERR);
        emit_mov_eax_imm32(&e, LIN_WRITE_SYSCALL);
        emit_syscall(&e);
        emit_jmp(&e, LBL_TRAP_EXIT);
    }
    /* shared trap exit: exit_group(101) (ADR-008 decision 3) */
    place_lbl_at(&e, LBL_TRAP_EXIT);
    emit_mov_edi_imm32(&e, KAVIR_TRAP_EXIT_CODE);
    emit_mov_eax_imm32(&e, LIN_EXIT_GROUP_SYSCALL);
    emit_syscall(&e);

    /* freestanding runtime + entry (depth 0 into main, ADR-010) */
    emit_print_i32_runtime(&e);
    start_pos = e.size;
    emit_xor_r11d_r11d(&e);
    emit_call(&e, LBL_FN_BASE + main_idx);
    emit_mov_edi_eax(&e);
    emit_mov_eax_imm32(&e, LIN_EXIT_GROUP_SYSCALL);
    emit_syscall(&e);

    code_end = e.size;

    /* patch pending rel32 fixups (all targets are placed by now) */
    for (i = 0; i < g_nfix; i++) {
        int target = g_lbl_pos[g_fix[i].label];
        if (target < 0)
            return emit_fail(&e, "internal: unresolved label");
        patch32(&e, g_fix[i].at,
                (unsigned long)(target - (g_fix[i].at + 4)));
    }

    /* rodata: appended after the code; vaddr = base + file offset */
    rodata_start = code_end;
    rosize = 0;
    for (i = 0; i < g_nrots; i++) {
        int b;
        g_rots[i].offset = rosize;
        for (b = 0; b < g_rots[i].len; b++) put8(&e, (unsigned)g_rots[i].data[b]);
        rosize += g_rots[i].len;
    }
    if (e.size > e.cap) return emit_fail(&e, "code buffer overflow");
    for (i = 0; i < g_nabs; i++)
        patch32(&e, g_abs[i].at,
                LIN_LOAD_BASE + (unsigned long)(CODE_FILE_OFFSET +
                                                rodata_start +
                                                g_rots[g_abs[i].rot].offset));

    if (write_elf(&e, start_pos, code_end, entry_file_offset) != 0) return -1;

    *out_size = e.size;
    return 0;
}
