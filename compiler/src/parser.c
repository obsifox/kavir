/* parser.c — Kavir parser (M1 subset).
 *
 * Grammar (M1):
 *   program := fndef*
 *   fndef   := "fn" IDENT "(" ")" "->" "i32" block
 *   block   := "{" stmt* "}"
 *   stmt    := "return" expr ";"
 *   expr    := INT
 *
 * Anything outside this grammar is rejected with a file:line:col
 * diagnostic naming the milestone that will introduce it.
 */
#include "kavir.h"
#include <string.h>
#include <stdarg.h>

typedef struct {
    const char *file;
    const Token *toks;
    int ntoks;
    int pos;
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

static int parse_expr(Parser *p, Expr *out) {
    const Token *t = peek(p);
    if (t->kind == T_INT) {
        advance(p);
        out->kind = EX_INT;
        out->val = t->int_val;
        out->line = t->line;
        out->col = t->col;
        return 0;
    }
    kavir_error(p->file, t->line, t->col,
                "M1 expressions are integer literals only (arithmetic "
                "operators arrive in M2)");
    return -1;
}

static int parse_stmt(Parser *p, Stmt *out) {
    const Token *t = peek(p);
    out->line = t->line;
    out->col = t->col;

    if (t->kind == T_RETURN) {
        advance(p);
        out->kind = ST_RETURN;
        if (parse_expr(p, &out->expr) != 0) return -1;
        if (expect(p, T_SEMI, "';' after return value") != 0) return -1;
        return 0;
    }
    kavir_error(p->file, t->line, t->col,
                "M1 statements are 'return' only (if/while arrive in M2/M3)");
    return -1;
}

static int parse_block(Parser *p, Fn *fn) {
    if (expect(p, T_LBRACE, "'{' to open function body") != 0) return -1;
    fn->nstmts = 0;
    while (peek(p)->kind != T_RBRACE) {
        const Token *t;
        if (fn->nstmts >= KAVIR_MAX_STMTS) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "too many statements in one function (M1 limit %d)",
                        KAVIR_MAX_STMTS);
            return -1;
        }
        if (peek(p)->kind == T_EOF) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "unexpected end of file inside function body "
                        "(missing '}')");
            return -1;
        }
        if (parse_stmt(p, &fn->stmts[fn->nstmts]) != 0) return -1;
        fn->nstmts++;
    }
    advance(p); /* consume '}' */
    return 0;
}

static int parse_fndef(Parser *p, Program *prog) {
    Fn *fn;
    const Token *t = peek(p);

    if (prog->nfns >= KAVIR_MAX_FNS) {
        kavir_error(p->file, t->line, t->col,
                    "too many functions (M1 limit %d)", KAVIR_MAX_FNS);
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
                    "M1 functions take no parameters yet (parameters "
                    "arrive in M3)");
        return -1;
    }
    advance(p);
    if (expect(p, T_ARROW, "'->' before return type") != 0) return -1;
    if (peek(p)->kind != T_I32) {
        kavir_error(p->file, peek(p)->line, peek(p)->col,
                    "M1 return types are 'i32' only");
        return -1;
    }
    advance(p);
    fn->ret_is_i32 = 1;

    return parse_block(p, fn) == 0 ? 0 : -1;
}

int parse(const char *file, const Token *toks, int ntoks, Program *prog) {
    Parser p;
    p.file = file;
    p.toks = toks;
    p.ntoks = ntoks;
    p.pos = 0;

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
