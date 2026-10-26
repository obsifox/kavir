/* parser.c — Kavir parser (M2 subset).
 *
 * Grammar (M2 — TEMPORARY, ADR-008 section 0; semantics are binding):
 *   program := fndef*
 *   fndef   := "fn" IDENT "(" ")" "->" "i32" block
 *   block   := "{" stmt* "}"
 *   stmt    := "return" expr ";"
 *            | "print" "(" expr ")" ";"
 *            | "print" "(" STRING ")" ";"
 *            | "if" "(" expr ")" block ("else" block)?
 *   expr    := muldiv (("+"|"-") muldiv)*
 *   muldiv  := unary (("*"|"/"|"%") unary)*
 *   unary   := "-" INT | primary
 *   primary := INT | "(" expr ")"
 *
 * Expressions are built into the program-wide Expr pool; children are
 * pool indices. Blocks come from the program-wide Block pool. Pools are
 * bounded (ADR-008 decision 7) and overflow with clean diagnostics.
 */
#include "kavir.h"
#include <string.h>
#include <stdarg.h>

typedef struct {
    const char *file;
    const Token *toks;
    int ntoks;
    int pos;
    Program *prog;
} Parser;

static const Token *peek(const Parser *p) { return &p->toks[p->pos]; }

static const Token *advance(Parser *p) {
    const Token *t = &p->toks[p->pos];
    if (t->kind != T_EOF && p->pos < p->ntoks - 1) p->pos++;
    return t;
}

static int expect(Parser *p, TokKind kind, const char *what) {
    const Token *t = peek(p);
    if (t->kind != kind) {
        kavir_error(p->file, t->line, t->col,
                    "expected %s, found '%s'", what,
                    t->kind == T_EOF ? "<end of file>" : t->lex);
        return -1;
    }
    advance(p);
    return 0;
}

/* Allocates an expression node in the pool; -1 with a diagnostic when
 * the pool is exhausted. */
static int expr_alloc(Parser *p, const Expr *node) {
    if (p->prog->nexprs >= KAVIR_MAX_EXPRS) {
        kavir_error(p->file, node->line, node->col,
                    "expression too complex (M2 limit %d expression nodes "
                    "per program)", KAVIR_MAX_EXPRS);
        return -1;
    }
    p->prog->exprs[p->prog->nexprs] = *node;
    return p->prog->nexprs++;
}

static int expr_int(Parser *p, const Token *t) {
    Expr e;
    memset(&e, 0, sizeof(e));
    e.kind = EX_INT;
    e.val = t->int_val;
    e.a = e.b = -1;
    e.line = t->line;
    e.col = t->col;
    return expr_alloc(p, &e);
}

static int expr_bin(Parser *p, int op, int a, int b, int line, int col) {
    Expr e;
    memset(&e, 0, sizeof(e));
    e.kind = EX_BIN;
    e.op = op;
    e.a = a;
    e.b = b;
    e.line = line;
    e.col = col;
    return expr_alloc(p, &e);
}

static int expr_neg(Parser *p, int a, int line, int col) {
    Expr e;
    memset(&e, 0, sizeof(e));
    e.kind = EX_NEG;
    e.a = a;
    e.b = -1;
    e.line = line;
    e.col = col;
    return expr_alloc(p, &e);
}

static int parse_expr(Parser *p);
static int parse_muldiv(Parser *p);

/* unary := "-" INT | primary
 * ADR-008 decision 1: unary minus applies to integer literals only and
 * is resolved as literal interpretation (the 2147483648 special case
 * lives in the checker). */
static int parse_unary(Parser *p) {
    const Token *t = peek(p);
    if (t->kind == T_MINUS) {
        int inner, neg;
        int line = t->line, col = t->col;
        advance(p);
        t = peek(p);
        if (t->kind != T_INT) {
            kavir_error(p->file, t->line, t->col,
                        "unary '-' in M2 applies to integer literals only "
                        "(parenthesized and computed operands arrive with "
                        "variables in M3)");
            return -1;
        }
        inner = expr_int(p, t);
        if (inner < 0) return -1;
        advance(p);
        neg = expr_neg(p, inner, line, col);
        return neg;
    }
    if (t->kind == T_INT) {
        int node = expr_int(p, t);
        if (node < 0) return -1;
        advance(p);
        return node;
    }
    if (t->kind == T_LPAREN) {
        int e;
        advance(p);
        e = parse_expr(p);
        if (e < 0) return -1;
        if (expect(p, T_RPAREN, "')'") != 0) return -1;
        return e;
    }
    kavir_error(p->file, t->line, t->col,
                "expected an integer expression, found '%s'",
                t->kind == T_EOF ? "<end of file>" : t->lex);
    return -1;
}

static int tok_to_binop(TokKind k) {
    switch (k) {
    case T_PLUS:    return BIN_ADD;
    case T_MINUS:   return BIN_SUB;
    case T_STAR:    return BIN_MUL;
    case T_SLASH:   return BIN_DIV;
    case T_PERCENT: return BIN_REM;
    default:        return -1;
    }
}

static int parse_addsub(Parser *p) {
    int l = parse_muldiv(p);
    if (l < 0) return -1;
    for (;;) {
        const Token *t = peek(p);
        int op, r, node;
        if (t->kind != T_PLUS && t->kind != T_MINUS) return l;
        op = tok_to_binop(t->kind);
        advance(p);
        r = parse_muldiv(p);
        if (r < 0) return -1;
        node = expr_bin(p, op, l, r, t->line, t->col);
        if (node < 0) return -1;
        l = node;
    }
}

static int parse_muldiv(Parser *p) {
    int l = parse_unary(p);
    if (l < 0) return -1;
    for (;;) {
        const Token *t = peek(p);
        int op, r, node;
        if (t->kind != T_STAR && t->kind != T_SLASH && t->kind != T_PERCENT)
            return l;
        op = tok_to_binop(t->kind);
        advance(p);
        r = parse_unary(p);
        if (r < 0) return -1;
        node = expr_bin(p, op, l, r, t->line, t->col);
        if (node < 0) return -1;
        l = node;
    }
}

static int parse_expr(Parser *p) { return parse_addsub(p); }

/* Allocates a block from the program pool and parses statements into
 * it; returns the block index or -1. */
static int parse_block(Parser *p) {
    Block *blk;
    int bidx;
    const Token *t = peek(p);

    if (p->prog->nblocks >= KAVIR_MAX_BLOCKS) {
        kavir_error(p->file, t->line, t->col,
                    "too many blocks (M2 limit %d per program)",
                    KAVIR_MAX_BLOCKS);
        return -1;
    }
    bidx = p->prog->nblocks++;
    blk = &p->prog->blocks[bidx];
    blk->nstmts = 0;

    if (expect(p, T_LBRACE, "'{' to open the block") != 0) return -1;
    while (peek(p)->kind != T_RBRACE) {
        Stmt *st;
        if (blk->nstmts >= KAVIR_MAX_STMTS) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "too many statements in one block (M2 limit %d)",
                        KAVIR_MAX_STMTS);
            return -1;
        }
        if (peek(p)->kind == T_EOF) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "unexpected end of file inside a block (missing '}')");
            return -1;
        }
        st = &blk->stmts[blk->nstmts];
        memset(st, 0, sizeof(*st));
        st->expr_idx = st->str_idx = st->then_block = st->else_block = -1;
        st->line = peek(p)->line;
        st->col = peek(p)->col;

        switch (peek(p)->kind) {
        case T_RETURN: {
            const Token *kw = advance(p);
            int e;
            st->kind = ST_RETURN;
            e = parse_expr(p);
            if (e < 0) return -1;
            st->expr_idx = e;
            if (expect(p, T_SEMI, "';' after return value") != 0) return -1;
            (void)kw;
            break;
        }
        case T_PRINT: {
            advance(p);
            if (expect(p, T_LPAREN, "'(' after 'print'") != 0) return -1;
            if (peek(p)->kind == T_STR) {
                const Token *s = advance(p);
                if (p->prog->nstrings >= KAVIR_MAX_STRINGS) {
                    kavir_error(p->file, s->line, s->col,
                                "too many string literals (M2 limit %d per "
                                "program)", KAVIR_MAX_STRINGS);
                    return -1;
                }
                st->kind = ST_PRINT_STR;
                st->str_idx = p->prog->nstrings;
                memcpy(p->prog->strings[st->str_idx], s->str,
                       (size_t)s->str_len);
                p->prog->strings[st->str_idx][s->str_len] = '\0';
                p->prog->str_lens[st->str_idx] = s->str_len;
                p->prog->nstrings++;
            } else {
                int e = parse_expr(p);
                if (e < 0) return -1;
                st->kind = ST_PRINT;
                st->expr_idx = e;
            }
            if (expect(p, T_RPAREN, "')' to close print") != 0) return -1;
            if (expect(p, T_SEMI, "';' after print") != 0) return -1;
            break;
        }
        case T_IF: {
            int cond, then_b, else_b = -1;
            advance(p);
            if (expect(p, T_LPAREN, "'(' after 'if'") != 0) return -1;
            cond = parse_expr(p);
            if (cond < 0) return -1;
            if (expect(p, T_RPAREN, "')' after the if condition") != 0)
                return -1;
            then_b = parse_block(p);
            if (then_b < 0) return -1;
            if (peek(p)->kind == T_ELSE) {
                advance(p);
                else_b = parse_block(p);
                if (else_b < 0) return -1;
            }
            st->kind = ST_IF;
            st->expr_idx = cond;
            st->then_block = then_b;
            st->else_block = else_b;
            break;
        }
        case T_ELSE:
            kavir_error(p->file, peek(p)->line, peek(p)->col,
                        "unexpected 'else' (no matching 'if' before it)");
            return -1;
        default: {
            const Token *bad = peek(p);
            kavir_error(p->file, bad->line, bad->col,
                        "M2 statements are 'return', 'print' and 'if' "
                        "(found '%s'; let/while arrive in M3)",
                        bad->kind == T_EOF ? "<end of file>" : bad->lex);
            return -1;
        }
        }
        blk->nstmts++;
    }
    advance(p); /* consume '}' */
    return bidx;
}

static int parse_fndef(Parser *p, Program *prog) {
    Fn *fn;
    const Token *t = peek(p);

    if (prog->nfns >= KAVIR_MAX_FNS) {
        kavir_error(p->file, t->line, t->col,
                    "too many functions (M2 limit %d)", KAVIR_MAX_FNS);
        return -1;
    }
    fn = &prog->fns[prog->nfns];
    memset(fn, 0, sizeof(*fn));
    fn->line = t->line;
    fn->col = t->col;

    advance(p); /* consume 'fn' */

    t = peek(p);
    if (t->kind != T_IDENT) {
        kavir_error(p->file, t->line, t->col, "expected function name");
        return -1;
    }
    {
        size_t i = 0;
        while (t->lex[i] != '\0' && i < KAVIR_MAX_NAME - 1) {
            fn->name[i] = t->lex[i];
            i++;
        }
        fn->name[i] = '\0';
    }
    advance(p);

    if (expect(p, T_LPAREN, "'(' after function name") != 0) return -1;
    if (peek(p)->kind != T_RPAREN) {
        kavir_error(p->file, peek(p)->line, peek(p)->col,
                    "M2 functions take no parameters yet (parameters "
                    "arrive in M3)");
        return -1;
    }
    advance(p);
    if (expect(p, T_ARROW, "'->' before return type") != 0) return -1;
    if (peek(p)->kind != T_I32) {
        kavir_error(p->file, peek(p)->line, peek(p)->col,
                    "M2 return types are 'i32' only");
        return -1;
    }
    advance(p);
    fn->ret_is_i32 = 1;

    {
        int body = parse_block(p);
        if (body < 0) return -1;
        fn->body_block = body;
    }
    return 0;
}

int parse(const char *file, const Token *toks, int ntoks, Program *prog) {
    Parser p;
    p.file = file;
    p.toks = toks;
    p.ntoks = ntoks;
    p.pos = 0;
    p.prog = prog;

    memset(prog, 0, sizeof(*prog));

    if (toks[0].kind == T_EOF) {
        kavir_error(file, 1, 1, "empty source file");
        return -1;
    }

    while (peek(&p)->kind != T_EOF) {
        if (peek(&p)->kind != T_FN) {
            const Token *t = peek(&p);
            kavir_error(file, t->line, t->col,
                        "expected 'fn' at top level, found '%s'", t->lex);
            return -1;
        }
        if (parse_fndef(&p, prog) != 0) return -1;
        prog->nfns++;
    }
    return 0;
}
