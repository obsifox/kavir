/* kavir.h — shared definitions for the Kavir compiler (M1 walking skeleton).
 *
 * Pipeline: lexer -> parser -> checker -> IR -> (x86-64 encoder -> ELF)
 *                                                  -> (IR interpreter)
 *
 * The interpreter is the executable reference semantics (ADR-005 v3
 * amendment b): every codegen conformance test must match it.
 */
#ifndef KAVIR_H
#define KAVIR_H

#include <stdio.h>

#define KAVIR_MAX_TOKENS 256
#define KAVIR_MAX_FNS 8
#define KAVIR_MAX_STMTS 32
#define KAVIR_MAX_INSTS 64
#define KAVIR_MAX_NAME 64          /* identifiers: max 63 chars + NUL */
#define KAVIR_MAX_CODE (1 << 16)
#define KAVIR_MAX_ERR 256

/* ---------------- tokens ---------------- */

typedef enum {
    T_EOF, T_FN, T_RETURN, T_I32, T_IDENT, T_INT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_ARROW, T_SEMI
} TokKind;

typedef struct {
    TokKind kind;
    char lex[KAVIR_MAX_NAME];
    /* Unsigned so accumulation can never hit signed-overflow UB; the
     * checker rejects any value above INT32_MAX (ADR-007). Values stored
     * here are therefore always representable; too-large-for-u64 inputs
     * are rejected in the lexer before reaching this field. */
    unsigned long long int_val;
    int line, col;
} Token;

/* ---------------- AST ---------------- */

typedef enum { EX_INT } ExprKind;

typedef struct {
    ExprKind kind;
    unsigned long long val; /* checker guarantees <= INT32_MAX */
    int line, col;
} Expr;

typedef enum { ST_RETURN } StmtKind;

typedef struct {
    StmtKind kind;
    Expr expr;
    int line, col;
} Stmt;

typedef struct {
    char name[KAVIR_MAX_NAME];
    int ret_is_i32;
    Stmt stmts[KAVIR_MAX_STMTS];
    int nstmts;
    int line, col;
} Fn;

typedef struct {
    Fn fns[KAVIR_MAX_FNS];
    int nfns;
} Program;

/* ---------------- IR ---------------- */

typedef enum { IR_CONST, IR_RET } IrOpKind;

typedef struct {
    IrOpKind op;
    int dest;      /* IR_CONST: destination virtual register */
    long long val; /* IR_CONST: immediate                     */
    int operand;   /* IR_RET: source virtual register         */
    int line, col;
} IrInst;

typedef struct {
    char name[KAVIR_MAX_NAME];
    int ret_is_i32;
    IrInst insts[KAVIR_MAX_INSTS];
    int ninsts;
} IrFn;

typedef struct {
    IrFn fns[KAVIR_MAX_FNS];
    int nfns;
} IrModule;

/* ---------------- diagnostics ---------------- */

void kavir_error(const char *file, int line, int col, const char *fmt, ...);
int kavir_error_count(void);

/* ---------------- stages ---------------- */

/* Lex src into toks (up to max). Returns token count, or -1 on lexical
 * error (error already reported). */
int lex(const char *file, const char *src, Token *toks, int max);

/* Parse tokens into prog. Returns 0 on success, -1 on error (reported). */
int parse(const char *file, const Token *toks, int ntoks, Program *prog);

/* Semantic checks (M1: main exists, types match, literals fit i32).
 * Returns 0 on success, -1 on error (reported). */
int check(const char *file, const Program *prog);

/* Build IR from a checked program. */
void ir_build(const Program *prog, IrModule *mod);
void ir_dump(const IrModule *mod, FILE *out);

/* Interpreter: executes main and returns its return value.
 * This is the reference semantics (ADR-005 v3 amendment b). */
long long interp_run(const IrModule *mod);

/* Target backend: x86-64 Linux, static ET_EXEC ELF, no libc, direct
 * syscalls (ADR-002 v2). ALL x86-64/Linux-specific constants live in
 * this module only (ADR-006 discipline rule).
 *
 * Emits a complete ELF file image into buf. Returns 0 on success; on
 * failure returns -1 with a message in errbuf.
 * entry_file_offset receives the file offset of the ELF entry point
 * (for diagnostics/tests). */
int target_x86_64_linux_emit(const IrModule *mod, unsigned char *buf,
                             int cap, int *out_size, int *entry_file_offset,
                             char *errbuf, int errcap);

#endif /* KAVIR_H */
