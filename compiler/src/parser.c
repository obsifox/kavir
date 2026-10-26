/* parser.c — Kavir parser (M3 subset).
 *
 * Grammar (M3 — TEMPORARY, ADR-008 section 0; semantics are binding):
 *   program    := fndef*
 *   fndef      := "fn" IDENT "(" params? ")" "->" "i32" block
 *   params     := param ("," param)*
 *   param      := IDENT ":" "i32"
 *   block      := "{" stmt* "}"
 *   stmt       := "return" expr ";"
 *               | "print" "(" expr ")" ";"
 *               | "print" "(" STRING ")" ";"
 *               | "let" "mut"? IDENT ":" "i32" "=" expr ";"
 *               | IDENT "=" expr ";"                    (assignment)
 *               | "if" "(" expr ")" block ("else" block)?
 *               | "while" "(" expr ")" block
 *   expr       := addsub (cmpop addsub)?      (NON-associative: one level;
 *              chained comparisons need parentheses — ADR-010 decision 2)
 *   cmpop      := "<" | "<=" | ">" | ">=" | "==" | "!="
 *   addsub     := muldiv (("+"|"-") muldiv)*
 *   muldiv     := unary (("*"|"/"|"%") unary)*
 *   unary      := "-" INT | primary
 *   primary    := INT | IDENT | IDENT "(" args? ")" | "(" expr ")"
 *
 * Expressions are built into the program-wide Expr pool; children are
 * pool indices. Blocks come from the program-wide Block pool. Pools are
 * bounded (ADR-008 decision 7) and overflow with clean diagnostics.
 *
 * M3 note: calls are EXPRESSIONS only (a bare call statement does not
 * exist; ADR-010 decision 6). Unary minus stays literal-only (ADR-008
 * decision 1, unchanged).
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
                    "expression too complex (limit %d expression nodes "
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

static void copy_name(char *dst, const Token *t) {
    size_t i = 0;
    while (t->lex[i] != '\0' && i < KAVIR_MAX_NAME - 1) {
        dst[i] = t->lex[i];
        i++;
    }
    dst[i] = '\0';
}

static int expr_ident(Parser *p, const Token *t) {
    Expr e;
    memset(&e, 0, sizeof(e));
    e.kind = EX_IDENT;
    copy_name(e.name, t);
    e.a = e.b = -1;
    e.line = t->line;
    e.col = t->col;
    return expr_alloc(p, &e);
}

static int parse_expr(Parser *p);
static int parse_muldiv(Parser *p);
static int parse_unary(Parser *p);

/* unary := "-" INT | primary
 * ADR-008 decision 1 (unchanged in M3): unary minus applies to integer
 * literals only and is resolved as literal interpretation (the
 * 2147483648 special case lives in the checker). */
static int parse_unary(Parser *p) {
    const Token *t = peek(p);
    if (t->kind == T_MINUS) {
        int inner, neg;
        int line = t->line, col = t->col;
        advance(p);
        t = peek(p);
        if (t->kind != T_INT) {
            kavir_error(p->file, t->line, t->col,
                        "unary '-' applies to integer literals only "
                        "(computed operands are not in the M3 scope)");
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
    if (t->kind == T_IDENT) {
        /* variable reference or call expression */
        const Token *id = advance(p);
        if (peek(p)->kind == T_LPAREN) {
            Expr e;
            int at_line = peek(p)->line, at_col = peek(p)->col;
            int n = 0;
            memset(&e, 0, sizeof(e));
            e.kind = EX_CALL;
            copy_name(e.name, id);
            e.line = at_line;
            e.col = at_col;
            advance(p); /* consume '(' */
            if (peek(p)->kind != T_RPAREN) {
                for (;;) {
                    int a;
                    if (n >= KAVIR_MAX_ARGS) {
                        kavir_error(p->file, peek(p)->line, peek(p)->col,
                                    "too many arguments (limit %d per "
                                    "call)", KAVIR_MAX_ARGS);
                        return -1;
                    }
                    a = parse_expr(p);
                    if (a < 0) return -1;
                    e.args[n++] = a;
                    if (peek(p)->kind == T_COMMA) {
                        advance(p);
                        continue;
                    }
                    break;
                }
            }
            e.nargs = n;
            if (expect(p, T_RPAREN, "')' to close the argument list") != 0)
                return -1;
            return expr_alloc(p, &e);
        }
        return expr_ident(p, id);
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
                "expected an expression, found '%s'",
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
    case T_LT:      return BIN_LT;
    case T_LE:      return BIN_LE;
    case T_GT:      return BIN_GT;
    case T_GE:      return BIN_GE;
    case T_EQEQ:    return BIN_EQ;
    case T_NE:      return BIN_NE;
    default:        return -1;
    }
}

static int is_cmp_tok(TokKind k) {
    return k == T_LT || k == T_LE || k == T_GT ||
           k == T_GE || k == T_EQEQ || k == T_NE;
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

/* expr := addsub (cmpop addsub)? — exactly ONE comparison level;
 * `a < b < c` is a compile error (use parentheses). ADR-010 decision 2. */
static int parse_expr(Parser *p) {
    int l = parse_addsub(p);
    if (l < 0) return -1;
    if (is_cmp_tok(peek(p)->kind)) {
        const Token *t = peek(p);
        int op, r, node;
        op = tok_to_binop(t->kind);
        advance(p);
        r = parse_addsub(p);
        if (r < 0) return -1;
        node = expr_bin(p, op, l, r, t->line, t->col);
        if (node < 0) return -1;
        if (is_cmp_tok(peek(p)->kind)) {
            kavir_error(p->file, peek(p)->line, peek(p)->col,
                        "comparison operators are non-associative "
                        "(chain them with parentheses)");
            return -1;
        }
        return node;
    }
    return l;
}

/* Allocates a block from the program pool and parses statements into
 * it; returns the block index or -1. */
static int parse_block(Parser *p) {
    Block *blk;
    int bidx;
    const Token *t = peek(p);

    if (p->prog->nblocks >= KAVIR_MAX_BLOCKS) {
        kavir_error(p->file, t->line, t->col,
                    "too many blocks (limit %d per program)",
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
                        "too many statements in one block (limit %d)",
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
        st->body_block = -1;
        st->line = peek(p)->line;
        st->col = peek(p)->col;

        switch (peek(p)->kind) {
        case T_RETURN: {
            advance(p);
            st->kind = ST_RETURN;
            st->expr_idx = parse_expr(p);
            if (st->expr_idx < 0) return -1;
            if (expect(p, T_SEMI, "';' after return value") != 0) return -1;
            break;
        }
        case T_PRINT: {
            advance(p);
            if (expect(p, T_LPAREN, "'(' after 'print'") != 0) return -1;
            if (peek(p)->kind == T_STR) {
                const Token *s = advance(p);
                if (p->prog->nstrings >= KAVIR_MAX_STRINGS) {
                    kavir_error(p->file, s->line, s->col,
                                "too many string literals (limit %d per "
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
                st->kind = ST_PRINT;
                st->expr_idx = parse_expr(p);
                if (st->expr_idx < 0) return -1;
            }
            if (expect(p, T_RPAREN, "')' to close print") != 0) return -1;
            if (expect(p, T_SEMI, "';' after print") != 0) return -1;
            break;
        }
        case T_LET: {
            const Token *id;
            advance(p);
            st->kind = ST_LET;
            if (peek(p)->kind == T_MUT) {
                advance(p);
                st->is_mut = 1;
            }
            if (peek(p)->kind != T_IDENT) {
                kavir_error(p->file, peek(p)->line, peek(p)->col,
                            "expected a variable name after 'let'");
                return -1;
            }
            id = advance(p);
            copy_name(st->name, id);
            if (expect(p, T_COLON, "':' and the type after the variable "
                                     "name") != 0)
                return -1;
            if (peek(p)->kind != T_I32) {
                kavir_error(p->file, peek(p)->line, peek(p)->col,
                            "M3 variables are 'i32' only");
                return -1;
            }
            advance(p);
            if (expect(p, T_EQ, "'=' and an initializer in 'let'")) return -1;
            st->expr_idx = parse_expr(p);
            if (st->expr_idx < 0) return -1;
            if (expect(p, T_SEMI, "';' after the let initializer") != 0)
                return -1;
            break;
        }
        case T_WHILE: {
            advance(p);
            st->kind = ST_WHILE;
            if (expect(p, T_LPAREN, "'(' after 'while'") != 0) return -1;
            st->expr_idx = parse_expr(p);
            if (st->expr_idx < 0) return -1;
            if (expect(p, T_RPAREN, "')' after the while condition") != 0)
                return -1;
            st->body_block = parse_block(p);
            if (st->body_block < 0) return -1;
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
        case T_IDENT: {
            /* assignment statement: name = expr; (calls are NOT
             * statements in M3 — ADR-010 decision 6) */
            const Token *id = advance(p);
            st->kind = ST_ASSIGN;
            copy_name(st->name, id);
            if (peek(p)->kind != T_EQ) {
                kavir_error(p->file, peek(p)->line, peek(p)->col,
                            "expected '=' after '%s' in a statement "
                            "(call statements do not exist; bind the "
                            "result with 'let x = f(...)')",
                            id->lex);
                return -1;
            }
            advance(p);
            st->expr_idx = parse_expr(p);
            if (st->expr_idx < 0) return -1;
            if (expect(p, T_SEMI, "';' after the assignment") != 0) return -1;
            break;
        }
        case T_ELSE:
            kavir_error(p->file, peek(p)->line, peek(p)->col,
                        "unexpected 'else' (no matching 'if' before it)");
            return -1;
        default: {
            const Token *bad = peek(p);
            kavir_error(p->file, bad->line, bad->col,
                        "statements are 'return', 'print', 'let', "
                        "'while', 'if' and assignments (found '%s')",
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
                    "too many functions (limit %d)", KAVIR_MAX_FNS);
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
    copy_name(fn->name, t);
    advance(p);

    if (expect(p, T_LPAREN, "'(' after function name") != 0) return -1;
    if (peek(p)->kind != T_RPAREN) {
        for (;;) {
            const Token *pid;
            if (fn->nparams >= KAVIR_MAX_ARGS) {
                kavir_error(p->file, peek(p)->line, peek(p)->col,
                            "too many parameters (limit %d per function)",
                            KAVIR_MAX_ARGS);
                return -1;
            }
            if (peek(p)->kind != T_IDENT) {
                kavir_error(p->file, peek(p)->line, peek(p)->col,
                            "expected a parameter name");
                return -1;
            }
            pid = advance(p);
            copy_name(fn->param_names[fn->nparams], pid);
            if (expect(p, T_COLON, "':' and the parameter type") != 0)
                return -1;
            if (peek(p)->kind != T_I32) {
                kavir_error(p->file, peek(p)->line, peek(p)->col,
                            "M3 parameter types are 'i32' only");
                return -1;
            }
            advance(p);
            fn->nparams++;
            if (peek(p)->kind == T_COMMA) {
                advance(p);
                continue;
            }
            break;
        }
    }
    if (expect(p, T_RPAREN, "')' to close the parameter list") != 0)
        return -1;
    if (expect(p, T_ARROW, "'->' before return type") != 0) return -1;
    if (peek(p)->kind != T_I32) {
        kavir_error(p->file, peek(p)->line, peek(p)->col,
                    "M3 return types are 'i32' only");
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
