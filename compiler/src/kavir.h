/* kavir.h — shared definitions for the Kavir compiler (M3).
 *
 * Pipeline: lexer -> parser -> checker -> IR -> (x86-64 encoder -> ELF)
 *                                                  -> (IR interpreter)
 *
 * The interpreter is the executable reference semantics (ADR-005 v3
 * amendment b): every codegen conformance test must match it, byte for
 * byte on stdout/stderr including traps (ADR-008). A third, independent
 * expectation lives in tests/reference.py (ADR-007 decision 2).
 *
 * M3 adds comparisons, while, let/let mut, and user function calls with
 * recursion (ADR-010: SysV AMD64 integer subset, RBP frames, call-depth
 * limit 1024 enforced identically in the interpreter and the ELF).
 * The surface remains TEMPORARY (ADR-008 section 0); semantics bind.
 */
#ifndef KAVIR_H
#define KAVIR_H

#include <stdio.h>

/* ---------------- capacity limits (ADR-008 decision 7) ----------------
 * Each limit is reported as a clean diagnostic when exceeded; none may
 * be crossed by an out-of-bounds access. */
#define KAVIR_MAX_TOKENS 256
#define KAVIR_MAX_FNS 8
#define KAVIR_MAX_STMTS 32          /* statements per block */
#define KAVIR_MAX_BLOCKS 128        /* block pool, program-wide */
#define KAVIR_MAX_EXPRS 256         /* expression node pool, program-wide */
#define KAVIR_MAX_INSTS 1024        /* IR instructions per function */
#define KAVIR_MAX_LABELS 256        /* IR labels per function */
#define KAVIR_MAX_NAME 64           /* identifiers: max 63 chars + NUL */
#define KAVIR_MAX_STR 128           /* string literal: max 127 bytes + NUL */
#define KAVIR_MAX_STRINGS 32        /* string table, program-wide */
#define KAVIR_MAX_CODE (1 << 18)    /* ELF image buffer: 256 KiB */
#define KAVIR_MAX_ERR 256
#define KAVIR_MAX_ARGS 8            /* arguments per call (ADR-010) */
#define KAVIR_MAX_VARS 64           /* params + lets per function */
/* USER DECISION 2026-10-26 (ADR-010 section 3b): recursion depth limit,
 * enforced IDENTICALLY by the interpreter (frame count) and the ELF
 * (hidden depth argument in r11d checked in every prologue). */
#define KAVIR_MAX_CALL_DEPTH 1024
/* vregs per function are bounded by exprs + vars: 256 + 64 <= 320;
 * the interpreter sizes its per-frame register files with this. */
#define KAVIR_MAX_VREGS 336

/* Pinned by ADR-008 decision 3 (fills the ADR-007 placeholder): a
 * runtime trap writes one diagnostic line to fd 2 and exits 101. */
#define KAVIR_TRAP_EXIT_CODE 101

/* ---------------- tokens ---------------- */

typedef enum {
    T_EOF, T_FN, T_RETURN, T_I32, T_IF, T_ELSE, T_PRINT,
    T_LET, T_MUT, T_WHILE,                       /* M3 */
    T_IDENT, T_INT, T_STR,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_ARROW, T_SEMI,
    T_COLON, T_COMMA, T_EQ,                      /* M3 */
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_LT, T_LE, T_GT, T_GE, T_EQEQ, T_NE         /* M3 comparisons */
} TokKind;

typedef struct {
    TokKind kind;
    char lex[KAVIR_MAX_NAME];
    /* T_STR: decoded literal bytes (NUL-terminated for debuggers; the
     * authoritative length is str_len). */
    char str[KAVIR_MAX_STR];
    int str_len;
    /* Unsigned so numeral accumulation can never hit signed-overflow
     * UB; the checker rejects any value above INT32_MAX (ADR-007). */
    unsigned long long int_val;
    int line, col;
} Token;

/* ---------------- AST ----------------
 * Expressions live in a program-wide pool; children are pool indices.
 * (Fixed pools instead of malloc: bounded memory, fuzz-safe, no
 * cleanup paths.) */

typedef enum {
    EX_INT, EX_NEG, EX_BIN, EX_IDENT, EX_CALL
} ExprKind;
typedef enum {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_REM,
    BIN_LT, BIN_LE, BIN_GT, BIN_GE, BIN_EQ, BIN_NE /* M3: yield 1/0 */
} BinOp;

typedef struct {
    ExprKind kind;
    unsigned long long val; /* EX_INT: numeral (checker-bounded, or
                             * 2147483648 directly under EX_NEG) */
    int op;                 /* EX_BIN: BinOp */
    int a, b;               /* EX_BIN: operand indices; EX_NEG: a only;
                             * -1 where unused */
    char name[KAVIR_MAX_NAME]; /* EX_IDENT: variable; EX_CALL: callee */
    int args[KAVIR_MAX_ARGS];  /* EX_CALL: argument expression indices */
    int nargs;                 /* EX_CALL: argument count */
    int line, col;          /* EX_BIN: operator position; EX_CALL: '(' */
} Expr;

typedef enum {
    ST_RETURN, ST_PRINT, ST_PRINT_STR, ST_IF,
    ST_LET, ST_ASSIGN, ST_WHILE                  /* M3 */
} StmtKind;

typedef struct {
    StmtKind kind;
    int expr_idx;   /* RETURN/PRINT/IF/WHILE/LET/ASSIGN: expression pool
                     * index, else -1 (PRINT_STR/none) */
    int str_idx;    /* PRINT_STR: program string index, else -1 */
    int then_block; /* IF: block pool index, else -1 */
    int else_block; /* IF: block pool index or -1 */
    int body_block; /* WHILE: block pool index, else -1 */
    int is_mut;     /* LET: 1 = declared 'let mut', 0 = immutable */
    char name[KAVIR_MAX_NAME]; /* LET: variable; ASSIGN: target */
    int line, col;
} Stmt;

typedef struct {
    Stmt stmts[KAVIR_MAX_STMTS];
    int nstmts;
} Block;

typedef struct {
    char name[KAVIR_MAX_NAME];
    int ret_is_i32;
    char param_names[KAVIR_MAX_ARGS][KAVIR_MAX_NAME]; /* M3 */
    int nparams;                 /* M3 */
    int body_block; /* index into Program.blocks */
    int line, col;
} Fn;

typedef struct {
    Fn fns[KAVIR_MAX_FNS];
    int nfns;
    Block blocks[KAVIR_MAX_BLOCKS];
    int nblocks;
    Expr exprs[KAVIR_MAX_EXPRS];
    int nexprs;
    char strings[KAVIR_MAX_STRINGS][KAVIR_MAX_STR];
    int str_lens[KAVIR_MAX_STRINGS];
    int nstrings;
} Program;

/* ---------------- IR ---------------- */

typedef enum {
    IR_CONST, IR_RET,                       /* M1 (encoding unchanged)  */
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_REM, /* M2 arithmetic (ADR-008)  */
    IR_PRINT, IR_PRINT_STR,                 /* M2 output                */
    IR_JMP, IR_JMPZ,                        /* M2 control flow          */
    IR_LT, IR_LE, IR_GT, IR_GE, IR_EQ, IR_NE, /* M3 comparisons -> 1/0  */
    IR_COPY,                                /* M3 vreg <- vreg          */
    IR_CALL                                 /* M3 dest, fn_idx, args    */
} IrOpKind;

typedef struct {
    IrOpKind op;
    int dest;      /* CONST/arith/cmp/COPY/CALL: destination vreg;
                    * PRINT: source vreg */
    long long val; /* CONST: immediate in [-2^31, 2^31-1] */
    int src1;      /* arith/cmp/COPY: lhs vreg; JMPZ: condition; RET: value */
    int src2;      /* arith/cmp: rhs vreg */
    int target;    /* JMP/JMPZ: instruction index (patched from label) */
    int str_idx;   /* PRINT_STR: string index */
    int fn_idx;    /* CALL: callee function index */
    int args[KAVIR_MAX_ARGS]; /* CALL: argument vregs, call order */
    int nargs;     /* CALL: argument count */
    int line, col; /* operator/statement position (trap messages) */
} IrInst;

typedef struct {
    char name[KAVIR_MAX_NAME];
    int ret_is_i32;
    char param_names[KAVIR_MAX_ARGS][KAVIR_MAX_NAME];
    int nparams;
    int line, col; /* definition position (depth-trap message) */
    IrInst insts[KAVIR_MAX_INSTS];
    int ninsts;
    int labels[KAVIR_MAX_LABELS]; /* label id -> instruction index */
    int nlabels;
    int nvregs;    /* vreg count == interpreter frame size */
} IrFn;

typedef struct {
    IrFn fns[KAVIR_MAX_FNS];
    int nfns;
    char strings[KAVIR_MAX_STRINGS][KAVIR_MAX_STR];
    int str_lens[KAVIR_MAX_STRINGS];
    int nstrings;
    const char *file; /* not owned; used for trap messages */
} IrModule;

/* ---------------- interpreter ---------------- */

typedef struct {
    int trapped;        /* 1 = trap occurred (trap_msg filled) */
    long long value;    /* main's return value when not trapped */
    char trap_msg[256]; /* full diagnostic line, trailing '\n' included */
} InterpResult;

/* Executes main. Returns 0 and fills out (value or trap) on a complete
 * run; returns -1 only on internal errors. Traps do NOT exit the
 * process (the driver maps them to KAVIR_TRAP_EXIT_CODE; the fuzz
 * harness inspects them in-process). */
int interp_run(const IrModule *mod, InterpResult *out);

/* Discard print output (fuzz harnesses); trap/value behaviour unchanged. */
void interp_set_quiet(int quiet);

/* ---------------- diagnostics ---------------- */

void kavir_error(const char *file, int line, int col, const char *fmt, ...);
int kavir_error_count(void);

/* ---------------- stages ---------------- */

/* Lex src into toks (up to max). Returns token count, or -1 on lexical
 * error (error already reported). */
int lex(const char *file, const char *src, Token *toks, int max);

/* Parse tokens into prog. Returns 0 on success, -1 on error (reported). */
int parse(const char *file, const Token *toks, int ntoks, Program *prog);

/* Semantic checks (M3: main exists; literal ranges per ADR-007 incl.
 * the -2147483648 special case; flat per-function variable scopes with
 * let/let mut and immutable params; call signatures (existence + arity);
 * no shadowing; return allowed anywhere, fall-off-end returns 0).
 * Returns 0 on success, -1 on error (reported). */
int check(const char *file, const Program *prog);

/* Build IR from a checked program. Returns 0, or -1 on a capacity
 * error (reported). file is used for diagnostics. */
int ir_build(const char *file, const Program *prog, IrModule *mod);
void ir_dump(const IrModule *mod, FILE *out);

/* Target backend: x86-64 Linux, static ET_EXEC ELF, no libc, direct
 * syscalls (ADR-002 v2). ALL x86-64/Linux-specific constants live in
 * this module only (ADR-006 discipline rule).
 *
 * Emits a complete ELF file image into buf (code + cold trap stubs +
 * emitted runtime + _start + rodata; ADR-008 decision 6). Returns 0 on
 * success; on failure returns -1 with a message in errbuf.
 * entry_file_offset receives the file offset of the ELF entry point. */
int target_x86_64_linux_emit(const IrModule *mod, unsigned char *buf,
                             int cap, int *out_size, int *entry_file_offset,
                             char *errbuf, int errcap);

#endif /* KAVIR_H */
