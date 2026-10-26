/* lexer.c — Kavir lexer (M3 subset; the surface is TEMPORARY, ADR-008 §0).
 *
 * Tokens: keywords fn/return/i32/if/else/print/let/mut/while,
 * identifiers, integer literals, string literals (escapes \n \t \r \\ \"
 * , UTF-8 bytes pass through), punctuation ( ) { } ; -> : , = and
 * operators + - * / % < <= > >= == !=.
 * // line comments.
 * Every token carries a file:line:col position (C-313 diagnostics rule).
 */
#include "kavir.h"
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>

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
                      int line, int col, const char *lex,
                      unsigned long long val) {
    t->kind = kind;
    t->int_val = val;
    t->line = line;
    t->col = col;
    t->str_len = 0;
    t->str[0] = '\0';
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

/* T_STR token: decoded bytes live in t->str (max 127), len in t->len. */
static void set_token_str(Token *t, const char *file, int line, int col,
                          const char *bytes, int len) {
    int i;
    t->kind = T_STR;
    t->int_val = 0;
    t->line = line;
    t->col = col;
    t->str_len = len;
    for (i = 0; i < len; i++) t->str[i] = bytes[i];
    t->str[len] = '\0';
    t->lex[0] = '\0';
    (void)file;
}

static int is_ident_start(int c) {
    return isalpha(c) || c == '_';
}
static int is_ident_cont(int c) {
    return isalnum(c) || c == '_';
}

/* Scans a string literal (opening quote already under the cursor).
 * Returns 0 and fills out (bytes+len), or -1 with a diagnostic. */
static int lex_string(Lexer *lx, const char *file,
                      char *out, int *out_len) {
    int line = lx->line, col = lx->col;
    int len = 0;

    adv(lx); /* consume opening '"' */
    for (;;) {
        int c = cur(lx);
        if (c == 0 || c == '\n') {
            kavir_error(file, line, col,
                        "unterminated string literal (closing '\"' missing "
                        "before end of line; use \\n for a newline)");
            return -1;
        }
        if (c == '"') {
            adv(lx);
            *out_len = len;
            return 0;
        }
        if (c == '\\') {
            int e;
            adv(lx); /* consume backslash */
            e = cur(lx);
            switch (e) {
            case 'n':  if (len < KAVIR_MAX_STR - 1) out[len++] = '\n'; break;
            case 't':  if (len < KAVIR_MAX_STR - 1) out[len++] = '\t'; break;
            case 'r':  if (len < KAVIR_MAX_STR - 1) out[len++] = '\r'; break;
            case '\\': if (len < KAVIR_MAX_STR - 1) out[len++] = '\\'; break;
            case '"':  if (len < KAVIR_MAX_STR - 1) out[len++] = '"';  break;
            case 0:
                kavir_error(file, line, col, "unterminated string literal");
                return -1;
            default:
                kavir_error(file, lx->line, lx->col,
                            "unknown escape '\\%c' in string literal "
                            "(M2 supports \\n \\t \\r \\\\ \\\")",
                            (e >= 32 && e < 127) ? e : '?');
                return -1;
            }
            adv(lx);
            continue;
        }
        /* Any other byte (including >= 0x80 UTF-8 continuation bytes)
         * passes through unchanged (ADR-008 decision 4). */
        if (len >= KAVIR_MAX_STR - 1) {
            kavir_error(file, line, col,
                        "string literal too long (M2 limit %d bytes)",
                        KAVIR_MAX_STR - 1);
            return -1;
        }
        out[len++] = (char)c;
        adv(lx);
    }
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
            kavir_error(file, lx.line, lx.col,
                        "too many tokens (limit %d)", max);
            return -1;
        }

        tok_line = lx.line;
        tok_col = lx.col;

        if (c == 0) {
            set_token(&toks[n], T_EOF, file, tok_line, tok_col, NULL, 0);
            n++;
            return n;
        }

        if (c == '"') {
            if (lex_string(&lx, file, toks[n].str, &toks[n].str_len) != 0)
                return -1;
            set_token_str(&toks[n], file, tok_line, tok_col,
                          toks[n].str, toks[n].str_len);
            n++;
            continue;
        }

        if (is_ident_start(c)) {
            char buf[KAVIR_MAX_NAME];
            size_t i = 0, full_len = 0;
            while (is_ident_cont(cur(&lx)) && cur(&lx) != 0) {
                if (i < KAVIR_MAX_NAME - 1) buf[i++] = (char)cur(&lx);
                full_len++;
                adv(&lx);
            }
            buf[i] = '\0';
            if (full_len > KAVIR_MAX_NAME - 1) {
                /* M1.1 review item: over-long identifiers must fail
                 * loudly, never be silently truncated. */
                kavir_error(file, tok_line, tok_col,
                            "identifier exceeds the maximum length of %d "
                            "characters (%llu characters total)",
                            KAVIR_MAX_NAME - 1,
                            (unsigned long long)full_len);
                return -1;
            }
            if (strcmp(buf, "fn") == 0)
                set_token(&toks[n], T_FN, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "return") == 0)
                set_token(&toks[n], T_RETURN, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "i32") == 0)
                set_token(&toks[n], T_I32, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "if") == 0)
                set_token(&toks[n], T_IF, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "else") == 0)
                set_token(&toks[n], T_ELSE, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "print") == 0)
                set_token(&toks[n], T_PRINT, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "let") == 0)
                set_token(&toks[n], T_LET, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "mut") == 0)
                set_token(&toks[n], T_MUT, file, tok_line, tok_col, buf, 0);
            else if (strcmp(buf, "while") == 0)
                set_token(&toks[n], T_WHILE, file, tok_line, tok_col, buf, 0);
            else
                set_token(&toks[n], T_IDENT, file, tok_line, tok_col, buf, 0);
            n++;
            continue;
        }

        if (isdigit(c)) {
            /* ADR-007: accumulate UNSIGNED and check BEFORE the multiply,
             * so no signed-overflow UB is reachable. Values up to
             * UINT64_MAX lex fine; the checker rejects anything above
             * INT32_MAX (with the -2147483648 special case, ADR-008). */
            unsigned long long v = 0;
            while (isdigit(cur(&lx)) && cur(&lx) != 0) {
                unsigned long long d = (unsigned long long)(cur(&lx) - '0');
                if (v > (ULLONG_MAX - d) / 10ULL) {
                    kavir_error(file, tok_line, tok_col,
                                "integer literal too large (exceeds "
                                "18446744073709551615)");
                    return -1;
                }
                v = v * 10ULL + d;
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

        /* two-character operators first, then the one-character forms */
        if (c == '<' && lx.src[lx.pos + 1] == '=') {
            adv(&lx); adv(&lx);
            set_token(&toks[n], T_LE, file, tok_line, tok_col, "<=", 0);
            n++; continue;
        }
        if (c == '>' && lx.src[lx.pos + 1] == '=') {
            adv(&lx); adv(&lx);
            set_token(&toks[n], T_GE, file, tok_line, tok_col, ">=", 0);
            n++; continue;
        }
        if (c == '=' && lx.src[lx.pos + 1] == '=') {
            adv(&lx); adv(&lx);
            set_token(&toks[n], T_EQEQ, file, tok_line, tok_col, "==", 0);
            n++; continue;
        }
        if (c == '!' && lx.src[lx.pos + 1] == '=') {
            adv(&lx); adv(&lx);
            set_token(&toks[n], T_NE, file, tok_line, tok_col, "!=", 0);
            n++; continue;
        }
        if (c == '!') {
            kavir_error(file, tok_line, tok_col,
                        "unexpected character '!' (only '!=' exists; there "
                        "is no unary '!' in the language)");
            return -1;
        }

        switch (c) {
        case '(': set_token(&toks[n], T_LPAREN, file, tok_line, tok_col, "(", 0); adv(&lx); n++; continue;
        case ')': set_token(&toks[n], T_RPAREN, file, tok_line, tok_col, ")", 0); adv(&lx); n++; continue;
        case '{': set_token(&toks[n], T_LBRACE, file, tok_line, tok_col, "{", 0); adv(&lx); n++; continue;
        case '}': set_token(&toks[n], T_RBRACE, file, tok_line, tok_col, "}", 0); adv(&lx); n++; continue;
        case ';': set_token(&toks[n], T_SEMI, file, tok_line, tok_col, ";", 0); adv(&lx); n++; continue;
        case ':': set_token(&toks[n], T_COLON, file, tok_line, tok_col, ":", 0); adv(&lx); n++; continue;
        case ',': set_token(&toks[n], T_COMMA, file, tok_line, tok_col, ",", 0); adv(&lx); n++; continue;
        case '=': set_token(&toks[n], T_EQ, file, tok_line, tok_col, "=", 0); adv(&lx); n++; continue;
        case '<': set_token(&toks[n], T_LT, file, tok_line, tok_col, "<", 0); adv(&lx); n++; continue;
        case '>': set_token(&toks[n], T_GT, file, tok_line, tok_col, ">", 0); adv(&lx); n++; continue;
        case '+': set_token(&toks[n], T_PLUS, file, tok_line, tok_col, "+", 0); adv(&lx); n++; continue;
        case '-': set_token(&toks[n], T_MINUS, file, tok_line, tok_col, "-", 0); adv(&lx); n++; continue;
        case '*': set_token(&toks[n], T_STAR, file, tok_line, tok_col, "*", 0); adv(&lx); n++; continue;
        case '/': set_token(&toks[n], T_SLASH, file, tok_line, tok_col, "/", 0); adv(&lx); n++; continue;
        case '%': set_token(&toks[n], T_PERCENT, file, tok_line, tok_col, "%", 0); adv(&lx); n++; continue;
        default:
            kavir_error(file, tok_line, tok_col,
                        "unexpected character '%c' (0x%02x) — not part of "
                        "the language surface (syntax is temporary, "
                        "ADR-008 §0)",
                        (c >= 32 && c < 127) ? c : '?', (unsigned)c);
            return -1;
        }
    }
}
