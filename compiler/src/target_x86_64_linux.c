/* target_x86_64_linux.c — Kavir target backend (M1).
 *
 * ADR-002 v2: custom backend, DIRECT x86-64 machine-code emission
 * through our own encoder, internal ELF writer, static ET_EXEC, no libc,
 * direct Linux syscalls. No .s text, no as, no ld, no cc in this path.
 *
 * ADR-006 discipline rule: ALL x86-64/Linux-specific constants
 * (opcodes, encodings, ELF fields, syscall numbers, load base, ABI
 * register choices) live in THIS file only.
 *
 * M1 lowering (documented special case; generalizes in M2/M3):
 *   - the single live value of a function lives in EAX (return register);
 *   - IR_CONST  -> mov eax, imm32
 *   - IR_RET    -> pop rbp; ret
 *   - entry (_start): call main; mov edi, eax; mov eax, <exit_group>;
 *                     syscall   (exit code = main's i32 return value).
 */
#include "kavir.h"
#include <string.h>

/* ---- Linux/x86-64 ABI constants (this file only, per ADR-006) ---- */
#define LIN_EXIT_GROUP_SYSCALL 231L   /* x86-64 syscall number */
#define LIN_LOAD_BASE 0x400000UL      /* static ET_EXEC base vaddr */
#define LIN_PAGE_ALIGN 0x1000UL

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
    e->buf[at]     = (unsigned char)(v & 0xff);
    e->buf[at + 1] = (unsigned char)((v >> 8) & 0xff);
    e->buf[at + 2] = (unsigned char)((v >> 16) & 0xff);
    e->buf[at + 3] = (unsigned char)((v >> 24) & 0xff);
}

/* ---- x86-64 encoders (this file only, per ADR-006) ---- */

/* mov eax, imm32 (B8+imm32) */
static void emit_mov_eax_imm32(Emit *e, long long v) {
    put8(e, 0xB8);
    put32(e, (unsigned long)(v & 0xffffffffUL));
}
/* mov edi, eax (89 C7) — zero-extends into RDI */
static void emit_mov_edi_eax(Emit *e) { put8(e, 0x89); put8(e, 0xC7); }
/* xor eax, eax (31 C0) */
static void emit_xor_eax_eax(Emit *e) { put8(e, 0x31); put8(e, 0xC0); }
/* push rbp (55) / pop rbp (5D) / ret (C3) / syscall (0F 05) */
static void emit_push_rbp(Emit *e) { put8(e, 0x55); }
static void emit_pop_rbp(Emit *e)  { put8(e, 0x5D); }
static void emit_ret(Emit *e)      { put8(e, 0xC3); }
static void emit_syscall(Emit *e)  { put8(e, 0x0F); put8(e, 0x05); }
/* mov rbp, rsp (48 89 E5) — frame convention used from M3 on */
static void emit_mov_rbp_rsp(Emit *e) { put8(e, 0x48); put8(e, 0x89); put8(e, 0xE5); }
/* call rel32 (E8 rel32) — rel32 patched after main is placed */
static int emit_call_rel32_placeholder(Emit *e, int *rel32_at) {
    put8(e, 0xE8);
    if (e->size + 4 > e->cap) return emit_fail(e, "code buffer overflow");
    *rel32_at = e->size;
    patch32(e, e->size, 0);
    e->size += 4;
    return 0;
}

/* ---- code generation ---- */

typedef struct {
    int code_start;   /* file offset of the code area */
    int main_off;     /* offset of main within the code area */
    int start_off;    /* offset of _start within the code area */
} CodeLayout;

static int gen_function(Emit *e, const IrFn *fn) {
    int k;
    emit_push_rbp(e);
    emit_mov_rbp_rsp(e);

    for (k = 0; k < fn->ninsts; k++) {
        const IrInst *in = &fn->insts[k];
        switch (in->op) {
        case IR_CONST:
            /* M1 lowering: one live value in EAX (documented special
             * case; the register allocator generalizes in M2/M3). */
            emit_mov_eax_imm32(e, in->val);
            break;
        case IR_RET:
            /* value is already in EAX (the SysV i32 return register) */
            emit_pop_rbp(e);
            emit_ret(e);
            return 0;
        }
    }
    /* falling off the end: return 0 (must match interpreter semantics) */
    emit_xor_eax_eax(e);
    emit_pop_rbp(e);
    emit_ret(e);
    return 0;
}

static int gen_program(Emit *e, const IrModule *mod, CodeLayout *layout) {
    const IrFn *main_fn = NULL;
    int i, rel32_at = -1, call_next;

    for (i = 0; i < mod->nfns; i++)
        if (strcmp(mod->fns[i].name, "main") == 0) main_fn = &mod->fns[i];
    if (main_fn == NULL) return emit_fail(e, "internal: no main in IR");

    layout->code_start = CODE_FILE_OFFSET;
    layout->main_off = 0;

    if (gen_function(e, main_fn) != 0) return -1;

    layout->start_off = e->size;
    /* _start: call main; mov edi, eax; mov eax, exit_group; syscall */
    if (emit_call_rel32_placeholder(e, &rel32_at) != 0) return -1;
    call_next = e->size;
    emit_mov_edi_eax(e);
    emit_mov_eax_imm32(e, (long long)LIN_EXIT_GROUP_SYSCALL);
    emit_syscall(e);

    /* patch call target: rel32 = (main vaddr) - (next-instr vaddr) */
    patch32(e, rel32_at,
            (LIN_LOAD_BASE + (unsigned long)layout->code_start +
             (unsigned long)layout->main_off) -
            (LIN_LOAD_BASE + (unsigned long)layout->code_start +
             (unsigned long)call_next));
    return 0;
}

/* ---- ELF writer ---- */

static int write_elf(Emit *e, const CodeLayout *layout, int *entry_file_off) {
    int code_size;
    unsigned long entry_vaddr;
    unsigned long total;

    code_size = e->size;
    entry_vaddr = LIN_LOAD_BASE + (unsigned long)layout->code_start +
                  (unsigned long)layout->start_off;
    *entry_file_off = layout->code_start + layout->start_off;
    total = (unsigned long)CODE_FILE_OFFSET + (unsigned long)code_size;

    /* The code was generated starting at buf[0]; shift it to make room
     * for the ELF header + program header at the file start. */
    if (CODE_FILE_OFFSET + code_size > e->cap)
        return emit_fail(e, "code buffer overflow");
    memmove(e->buf + CODE_FILE_OFFSET, e->buf, (size_t)code_size);
    e->size = 0; /* now emit the headers at file offset 0 */

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

    /* --- program header #0: one R+X PT_LOAD covering the whole image ---
     * M1 has no writable data segment yet (arrives with M2 data). */
    put32(e, ELF_PT_LOAD);
    put32(e, ELF_PF_R | ELF_PF_X);
    put64(e, 0);                /* p_offset */
    put64(e, LIN_LOAD_BASE);    /* p_vaddr  */
    put64(e, LIN_LOAD_BASE);    /* p_paddr  */
    put64(e, total);            /* p_filesz */
    put64(e, total);            /* p_memsz  */
    put64(e, LIN_PAGE_ALIGN);   /* p_align  */

    if (e->size != CODE_FILE_OFFSET)
        return emit_fail(e, "internal: ELF header size mismatch");

    e->size = CODE_FILE_OFFSET + code_size;
    return 0;
}

int target_x86_64_linux_emit(const IrModule *mod, unsigned char *buf,
                             int cap, int *out_size, int *entry_file_offset,
                             char *errbuf, int errcap) {
    Emit e;
    CodeLayout layout;

    e.buf = buf; e.cap = cap; e.size = 0;
    e.err = errbuf; e.errcap = errcap;

    if (gen_program(&e, mod, &layout) != 0) return -1;
    if (write_elf(&e, &layout, entry_file_offset) != 0) return -1;

    *out_size = e.size;
    return 0;
}
