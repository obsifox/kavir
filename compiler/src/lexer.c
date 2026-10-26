/* lexer.c — Kavir lexer (M1 subset).
 *
 * Tokens: keywords fn/return/i32, identifiers, integer literals,
 * punctuation ( ) { } ; ->, // line comments.
 * Every token carries a file:line:col position (C-313 diagnostics rule).
 */
#include "kavir.h"
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* ---------------- diagnostics (shared) ---------------- */

static int g_errors = 0;

void kavir_error(const char *file, int line, int col, const char *fmt, ...) {
    va_list ap;
    g_errors++;
    fprintf(stderr, "%s:%d:%d: error: ", file, line, col);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int kavir_error_count(void) { return g_errors; }

/* ---------------- lexer ---------------- */

typedef struct {
    const char *file;
    const char *src;
    int pos, line, col;
} Lexer;

static int cur(const Lexer *lx) { return (unsigned char)lx->src[lx->pos]; }

static int adv(Lexer *lx) {
    int c = (unsigned char)lx->src[lx->pos];
    if (c == 0) return 0;
    lx->pos++;
    if (c == '\n') { lx->line++; lx->col = 1; } else { lx->col++; }
    return c;
}

static void set_token(Token *t, TokKind kind, const char *file,
                      int line, int col, const char *lex, long long val) {
    t->kind = kind;
    t->int_val = val;
    t->line = line;
    t->col = col;
    if (lex != NULL) {
        size_t i = 0;
        while (lex[i] != '\0' && i < KAVIR_MAX_NAME - 1) {
            t->lex[i] = lex[i];
            i++;
        }
        t->lex[i] = '\0';
    } else {
        t->lex[0] = '\0';
    }
    (void)file;
}

static int is_ident_start(int c) {
    return isalpha(c) || c == '_';
}
static int is_ident_cont(int c) {
    return isalnum(c) || c == '_';
}

int lex(const char *file, const char *src, Token *toks, int max) {
    Lexer lx;
    int n = 0;

    lx.file = file;
    lx.src = src;
    lx.pos = 0;
    lx.line = 1;
    lx.col = 1;

    for (;;) {
        int c;
        int tok_line, tok_col;

        /* skip whitespace and // comments */
        for (;;) {
            c = cur(&lx);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                adv(&lx);
                continue;
            }
            if (c == '/' && lx.src[lx.pos + 1] == '/') {
                while (cur(&lx) != '\n' && cur(&lx) != 0) adv(&lx);
                continue;
            }
            break;
        }

        if (n >= max - 1) {
            kavir_error(file, lx.line, lx.col, "too many tokens (M1 limit %d)", max);
            return -1;
        }

        tok_line = lx.line;
        tok_col = lx.col;

        if (c == 0) {
            set_token(&toks[n], T_EOF, file, tok_line, tok_col, NULL, 0);
            n++;
            return n;
        }

        if (is_ident_start(c)) {
            char buf[KAVIR_MAX_NAME];
            size_t i = 0;
            while (is_ident_cont(cur(&lx)) && cur(&lx) != 0) {
                if (i < KAVIR_MAX_NAME - 1) buf[i++] = (char)cur(&lx);
                adv(&lx);
            }
            buf[i] = '\0';
            if (strcmp(buf, "fn") == 0)
                set_token(&toks[n], T_FN, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "return") == 0)
                set_token(&toks[n], T_RETURN, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "i32") == 0)
                set_token(&toks[n], T_I32, file, tok_line, tok_col, buf, 0);
            else
                set_token(&toks[n], T_IDENT, file, tok_line, tok_col, buf, 0);
            n++;
            continue;
        }

        if (isdigit(c)) {
            long long v = 0;
            while (isdigit(cur(&lx)) && cur(&lx) != 0) {
                v = v * 10 + (cur(&lx) - '0');
                if (v > 922337203685477580LL) { /* overflow guard */
                    kavir_error(file, tok_line, tok_col,
                                "integer literal too large");
                    return -1;
                }
                adv(&lx);
            }
            set_token(&toks[n], T_INT, file, tok_line, tok_col, NULL, v);
            n++;
            continue;
        }

        if (c == '-' && lx.src[lx.pos + 1] == '>') {
            adv(&lx); /* '-' */
            adv(&lx); /* '>' */
            set_token(&toks[n], T_ARROW, file, tok_line, tok_col, "->", 0);
            n++;
            continue;
        }

        switch (c) {
        case '(': set_token(&toks[n], T_LPAREN, file, tok_line, tok_col, "(", 0); adv(&lx); n++; continue;
        case ')': set_token(&toks[n], T_RPAREN, file, tok_line, tok_col, ")", 0); adv(&lx); n++; continue;
        case '{': set_token(&toks[n], T_LBRACE, file, tok_line, tok_col, "{", 0); adv(&lx); n++; continue;
        case '}': set_token(&toks[n], T_RBRACE, file, tok_line, tok_col, "}", 0); adv(&lx); n++; continue;
        case ';': set_token(&toks[n], T_SEMI, file, tok_line, tok_col, ";", 0); adv(&lx); n++; continue;
        default:
            kavir_error(file, tok_line, tok_col,
                        "unexpected character '%c' (0x%02x) — not part of the M1 surface",
                        (c >= 32 && c < 127) ? c : '?', (unsigned)c);
            return -1;
        }
    }
}
