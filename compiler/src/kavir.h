/* kavir.h -- shared definitions for the Kavir compiler (M4).
 *
 * Pipeline: lexer -> parser -> checker -> IR -> (x86-64 encoder -> ELF)
 *                                                 -> (IR interpreter)
 *
 * The interpreter is the executable reference semantics (ADR-005 v3
 * amendment b): every codegen conformance test must match it, byte for
 * byte on stdout/stderr including traps (ADR-008). A third, independent
 * expectation lives in tests/reference.py (ADR-007 decision 2).
 *
 * M4 adds structs and enums (value semantics), exhaustive match, and
 * the builtin Option[T] (ADR-004 v0.1.0 subset). Aggregate ABI and
 * representation decisions LOCKED by the user 2026-10-27 (ADR-011):
 * C layout (declaration order, natural alignment); uniform i32 tag at
 * word 0; values <= 8 bytes travel in ONE integer register exactly
 * like a SysV INTEGER eightbyte; values > 8 bytes ALWAYS travel
 * through memory (stack copy for arguments, hidden-pointer return) --
 * a documented deviation from the psABI two-register class for
 * 9..16-byte aggregates. The surface remains TEMPORARY (ADR-008
 * section 0); semantics bind.
 */
#ifndef KAVIR_H
#define KAVIR_H

#include <stdio.h>

/* ---------------- capacity limits (ADR-008 decision 7) ----------------
 * Each limit is reported as a clean diagnostic when exceeded; none may
 * be crossed by an out-of-bounds access.
 *
 * M3.1 (2026-10-26, user-mandated 50-function label-allocation test):
 * TOKENS 256->2048, FNS 8->64, BLOCKS 128->256, EXPRS 256->1024.
 * M4 keeps those and raises VREGS (vregs are now 32-bit WORDS). */
#define KAVIR_MAX_TOKENS 2048
#define KAVIR_MAX_FNS 64
#define KAVIR_MAX_STMTS 32          /* statements per block */
#define KAVIR_MAX_BLOCKS 256        /* block pool, program-wide */
#define KAVIR_MAX_EXPRS 1024        /* expression node pool, program-wide */
#define KAVIR_MAX_INSTS 1024        /* IR instructions per function */
#define KAVIR_MAX_LABELS 256        /* IR labels per function */
#define KAVIR_MAX_NAME 64           /* identifiers: max 63 chars + NUL */
#define KAVIR_MAX_STR 128           /* string literal: max 127 bytes + NUL */
#define KAVIR_MAX_STRINGS 32        /* string table, program-wide */
#define KAVIR_MAX_CODE (1 << 18)    /* ELF image buffer: 256 KiB */
#define KAVIR_MAX_ERR 256
/* ADR-010 (M3.1 amendment makes the split explicit): at most 8
 * arguments per call = 6 register slots (RDI RSI RDX RCX R8 R9; fewer
 * when a hidden return pointer occupies RDI) + the rest on the stack
 * (right-to-left placement, left-to-right evaluation). An aggregate
 * argument counts as ONE argument regardless of its size (ADR-011). */
#define KAVIR_MAX_ARGS 8
#define KAVIR_MAX_VARS 64           /* params + lets per function */
/* USER DECISION 2026-10-26 (ADR-010 section 3b): recursion depth limit,
 * enforced IDENTICALLY by the interpreter (frame count) and the ELF
 * (hidden depth argument in r11d checked in every prologue). */
#define KAVIR_MAX_CALL_DEPTH 1024
/* M4: vregs are 32-bit WORDS (a struct of k words occupies k
 * consecutive vregs, ADR-011). 512 words = 4 KiB per frame; the
 * interpreter sizes its 1024-frame stack with this (4 MiB static). */
#define KAVIR_MAX_VREGS 512

/* ---------------- M4 type-system capacities (ADR-011) ----------------
 * Each limit is a clean diagnostic; none may be crossed by an
 * out-of-bounds access (ADR-008 decision 7 discipline). */
#define KAVIR_MAX_TYPES 24          /* user types + Option monos */
#define KAVIR_MAX_FIELDS 8          /* fields per struct */
#define KAVIR_MAX_VARIANTS 16       /* variants per enum */
#define KAVIR_MAX_VARIANT_WORDS 8   /* payload words per variant */
#define KAVIR_MAX_STRUCT_WORDS 8    /* max struct size in 4-byte words */
#define KAVIR_MAX_ARMS 32           /* match arms per match expression */
#define KAVIR_MAX_LITFIELDS 256     /* struct-literal field names, pooled */
#define KAVIR_MAX_PATHS 256         /* assignment field-path segments */
#define KAVIR_MAX_ASSIGN_PATH 3     /* max field steps in one assignment */
#define KAVIR_MAX_RAWS 256          /* unresolved type expressions */
#define KAVIR_MAX_OPT_DEPTH 8       /* Option[Option[...]] nesting cap */

/* Type ids: 0 = i32; 1..n = user types; Option[T] monomorphizations
 * are synthesized enum entries appended to the same table. */
#define KAVIR_TY_I32 0
typedef enum { TY_STRUCT, TY_ENUM } TypeKind;

typedef struct {
    char name[KAVIR_MAX_NAME];
    TypeKind kind;
    int is_option;              /* 1 = synthesized Option[T] mono */
    int elem_ty;                /* Option[T]: the resolved T (monos) */
    /* STRUCT: fields in declaration order; field types are i32 or
     * previously declared aggregate types (all word-based, M4). */
    char field_names[KAVIR_MAX_FIELDS][KAVIR_MAX_NAME];
    int field_raw[KAVIR_MAX_FIELDS]; /* parser: raw type pool index */
    int field_ty[KAVIR_MAX_FIELDS];  /* checker: resolved type id */
    int field_off[KAVIR_MAX_FIELDS]; /* word offsets (C layout) */
    int nfields;
    /* ENUM: variants in declaration order; tag = variant index. */
    char variant_names[KAVIR_MAX_VARIANTS][KAVIR_MAX_NAME];
    int variant_raw[KAVIR_MAX_VARIANTS][KAVIR_MAX_VARIANT_WORDS];
    int variant_ty[KAVIR_MAX_VARIANTS][KAVIR_MAX_VARIANT_WORDS];
    int variant_nargs[KAVIR_MAX_VARIANTS];  /* payload VALUES */
    int variant_nwords[KAVIR_MAX_VARIANTS]; /* payload WORDS */
    int nvariants;
    int size_words;             /* total size in 4-byte words */
} TypeDef;

/* Unresolved type expression (parser output; the checker resolves it
 * and overwrites consumers in place): base = "i32" or a type name;
 * opt = raw index of the Option element type, or -1. */
typedef struct {
    char base[KAVIR_MAX_NAME];
    int opt; /* -1 = not an Option; else raw index of T in Option[T] */
} RawType;

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
    T_LT, T_LE, T_GT, T_GE, T_EQEQ, T_NE,        /* M3 comparisons */
    T_STRUCT, T_ENUM, T_MATCH, T_OPTION,         /* M4 type surface */
    T_DOT, T_LBRACKET, T_RBRACKET, T_FATARROW,   /* M4: . [ ] => */
    T_UNDERSCORE                                 /* M4: _ wildcard */
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
 * cleanup paths.) The checker annotates every node with its resolved
 * type id (e->ty, -1 before checking); ir.c trusts the annotation and
 * re-derives only the vreg placement. */

typedef enum {
    EX_INT, EX_NEG, EX_BIN, EX_IDENT, EX_CALL,   /* M1..M3 */
    EX_STRUCT_LIT,  /* M4: Name { f: e, ... } -- name = type name      */
    EX_ENUM_CTOR,   /* M4: Name.Variant(e, ...) -- name_b = variant     */
    EX_OPT_CTOR,    /* M4: Option.None | Option.Some(e) -- aux = tag    */
    EX_FIELD,       /* M4: base.name -- a = base expr; aux = word offset*/
    EX_MATCH        /* M4: match e { arms } -- a = scrutinee            */
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
                             * EX_FIELD: base expr; EX_MATCH: scrutinee;
                             * -1 where unused */
    char name[KAVIR_MAX_NAME]; /* EX_IDENT: variable; EX_CALL: callee;
                                * EX_STRUCT_LIT/EX_ENUM_CTOR: type name;
                                * EX_FIELD: field/variant name */
    char name_b[KAVIR_MAX_NAME]; /* EX_ENUM_CTOR: variant name */
    int args[KAVIR_MAX_ARGS];  /* EX_CALL/ctor/literal: operand exprs */
    int nargs;                 /* operand count */
    int aux;                /* EX_ENUM_CTOR: variant index (checker);
                             * EX_OPT_CTOR: tag (0 None, 1 Some);
                             * EX_FIELD: word offset (checker);
                             * EX_STRUCT_LIT: litfield pool start;
                             * EX_MATCH: arm pool start */
    int ty;                 /* checker: resolved type id (-1 raw) */
    int line, col;          /* EX_BIN: operator position; EX_CALL: '(' */
} Expr;

/* One match arm (pooled in Program.arms; EX_MATCH stores the start
 * index + count in aux/nargs). Patterns are flat: Type.Variant with
 * direct payload bindings, or the trailing wildcard _ (ADR-004 cut
 * list 1: no guards, no nested patterns). */
typedef struct {
    int is_wild;    /* 1 = _ */
    int is_none;    /* 1 = Option.None pattern (ADR-011) */
    char ty_name[KAVIR_MAX_NAME];      /* pattern type name */
    char variant_name[KAVIR_MAX_NAME]; /* variant name */
    int variant;    /* checker: variant index */
    int pat_ty;     /* checker: resolved pattern type id */
    char bind_names[KAVIR_MAX_VARIANT_WORDS][KAVIR_MAX_NAME];
    int bind_ty[KAVIR_MAX_VARIANT_WORDS]; /* checker: per-bind types */
    int nbinds;
    int body;       /* body expression pool index */
    int line, col;
} MatchArm;

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
    char name[KAVIR_MAX_NAME]; /* LET: variable; ASSIGN: root variable */
    int raw_idx;    /* LET: raw type pool index (checker resolves) */
    int ty;         /* checker: resolved type id (LET annot; ASSIGN:
                     * final field type after the path) */
    int path_start; /* ASSIGN: first path-pool segment after the root,
                     * or -1 for a plain variable assignment */
    int path_len;   /* ASSIGN: number of path segments (0..3) */
    int word_off;   /* checker (ASSIGN): word offset from the root vreg
                     * to the assignment target */
    int line, col;
} Stmt;

typedef struct {
    Stmt stmts[KAVIR_MAX_STMTS];
    int nstmts;
} Block;

typedef struct {
    char name[KAVIR_MAX_NAME];
    int ret_raw;    /* parser: raw type pool index (checker resolves) */
    int ret_ty;     /* checker: resolved return type id */
    char param_names[KAVIR_MAX_ARGS][KAVIR_MAX_NAME]; /* M3 */
    int param_raw[KAVIR_MAX_ARGS]; /* parser: raw type pool indices */
    int param_ty[KAVIR_MAX_ARGS];  /* checker: resolved type ids */
    int nparams;                 /* M3 */
    int body_block; /* index into Program.blocks */
    int line, col;
} Fn;

typedef struct {
    Fn fns[KAVIR_MAX_FNS];
    int nfns;
    TypeDef types[KAVIR_MAX_TYPES];
    int ntypes;
    RawType raws[KAVIR_MAX_RAWS];
    int nraws;
    MatchArm arms[KAVIR_MAX_ARMS];
    int narms;
    char litfields[KAVIR_MAX_LITFIELDS][KAVIR_MAX_NAME];
    int nlitfields;
    char paths[KAVIR_MAX_PATHS][KAVIR_MAX_NAME];
    int npaths;
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
    IR_CALL                                 /* M3/M4 dest, fn_idx, args */
} IrOpKind;

typedef struct {
    IrOpKind op;
    int dest;      /* CONST/arith/cmp/COPY/CALL: destination vreg;
                    * PRINT: source vreg. M4: for a multi-word CALL
                    * result, dest is the BASE of ret_words vregs. */
    long long val; /* CONST: immediate in [-2^31, 2^31-1] */
    int src1;      /* arith/cmp/COPY: lhs vreg; JMPZ: condition; RET:
                    * value (M4: base of the returned words) */
    int src2;      /* arith/cmp: rhs vreg */
    int target;    /* JMP/JMPZ: instruction index (patched from label) */
    int str_idx;   /* PRINT_STR: string index */
    int fn_idx;    /* CALL: callee function index */
    int args[KAVIR_MAX_ARGS]; /* CALL: argument BASE vregs, call order */
    int arg_words[KAVIR_MAX_ARGS]; /* CALL: words per argument (ADR-011:
                                    * 1 = i32, k = aggregate) */
    int nargs;     /* CALL: argument count */
    int ret_words; /* CALL: words in the result (1 = i32) */
    int line, col; /* operator/statement position (trap messages) */
} IrInst;

typedef struct {
    char name[KAVIR_MAX_NAME];
    int ret_is_i32;
    int ret_words;  /* M4 (ADR-011): 1 = i32; k = aggregate words;
                     * k > 2 returns through a hidden pointer (RDI). */
    char param_names[KAVIR_MAX_ARGS][KAVIR_MAX_NAME];
    int param_words[KAVIR_MAX_ARGS]; /* M4: vreg words per parameter */
    int nparams;
    int line, col; /* definition position (depth-trap message) */
    IrInst insts[KAVIR_MAX_INSTS];
    int ninsts;
    int labels[KAVIR_MAX_LABELS]; /* label id -> instruction index */
    int nlabels;
    int nvregs;    /* vreg (word) count == interpreter frame size */
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
    int step_limited;   /* 1 = the optional step budget expired (harness
                         * only; a legal divergent program, NOT a trap) */
} InterpResult;

/* Executes main. Returns 0 and fills out (value or trap) on a complete
 * run; returns -1 only on internal errors. Traps do NOT exit the
 * process (the driver maps them to KAVIR_TRAP_EXIT_CODE; the fuzz
 * harness inspects them in-process). */
int interp_run(const IrModule *mod, InterpResult *out);

/* Discard print output (fuzz harnesses); trap/value behaviour unchanged. */
void interp_set_quiet(int quiet);

/* Optional per-run step budget (counted IR instructions). 0 = unlimited
 * (the default; the CLI always runs unlimited). M4.1: the libFuzzer
 * harness sets a budget because coverage-guided mutation can legally
 * turn a bounded loop into a divergent one; the budget expires as
 * out->step_limited = 1 (never a trap, never any output). */
void interp_set_step_limit(unsigned long max_steps);

/* ---------------- diagnostics ---------------- */

void kavir_error(const char *file, int line, int col, const char *fmt, ...);
int kavir_error_count(void);

/* ---------------- stages ---------------- */

/* Lex src into toks (up to max). Returns token count, or -1 on lexical
 * error (error already reported). */
int lex(const char *file, const char *src, Token *toks, int max);

/* Parse tokens into prog. Returns 0 on success, -1 on error (reported). */
int parse(const char *file, const Token *toks, int ntoks, Program *prog);

/* Semantic checks. M4 additions: type declarations (names capitalized,
 * unique, not reserved; fields/variants/payloads word-based, no
 * recursive types); resolved signatures (aggregate params/returns
 * allowed); flat per-function scopes with word-counted variables;
 * struct literals (all fields, declaration order); field access and
 * field assignment through `let mut` roots; enum constructors;
 * Option[T] inference from the expected type; match with
 * exhaustiveness (every variant covered, or a final _), arm-local
 * bindings that may not shadow outer variables; main must be
 * `fn main() -> i32` with zero parameters. Returns 0 on success,
 * -1 on error (reported). */
int check(const char *file, const Program *prog);

/* Build IR from a checked program. Returns 0, or -1 on a capacity
 * error (reported). file is used for diagnostics. */
int ir_build(const char *file, const Program *prog, IrModule *mod);
void ir_dump(const IrModule *mod, FILE *out);

/* Target backend: x86-64 Linux, static ET_EXEC ELF, no libc, direct
 * syscalls (ADR-002 v2). ALL x86-64/Linux-specific constants live in
 * this module only (ADR-006 discipline rule).
 *
 * M4 aggregate ABI (ADR-011, user decisions 2026-10-27): C layout,
 * word-granular lowering; values <= 8 bytes in one integer register
 * (two i32 words packed low/high like a SysV INTEGER eightbyte);
 * values > 8 bytes through memory -- stack copy for arguments (each
 * rounded to 8 bytes, leftmost stack argument at [rsp+0]), hidden
 * pointer in RDI for returns (remaining register arguments shift).
 *
 * Emits a complete ELF file image into buf (code + cold trap stubs +
 * emitted runtime + _start + rodata; ADR-008 decision 6). Returns 0 on
 * success; on failure returns -1 with a message in errbuf.
 * entry_file_offset receives the file offset of the ELF entry point. */
int target_x86_64_linux_emit(const IrModule *mod, unsigned char *buf,
                             int cap, int *out_size, int *entry_file_offset,
                             char *errbuf, int errcap);

#endif /* KAVIR_H */
