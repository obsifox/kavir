/* parser.c — Kavir parser (M4 subset).
 *
 * Grammar (M4 — TEMPORARY, ADR-008 section 0; semantics are binding):
 *   program    := (structdef | enumdef | fndef)*
 *   structdef  := "struct" UIDENT "{" fielddef ("," fielddef)* "}"
 *   fielddef   := IDENT ":" type
 *   enumdef    := "enum" UIDENT "{" vardef ("," vardef)* "}"
 *   vardef     := UIDENT | UIDENT "(" type ("," type)* ")"
 *   type       := "i32" | UIDENT | "Option" "[" type "]"
 *   fndef      := "fn" IDENT "(" params? ")" "->" type block
 *   params     := param ("," param)*
 *   param      := IDENT ":" type
 *   block      := "{" stmt* "}"
 *   stmt       := "return" expr ";"
 *               | "print" "(" expr ")" ";"
 *               | "print" "(" STRING ")" ";"
 *               | "let" "mut"? IDENT ":" type "=" expr ";"
 *               | IDENT ("." IDENT)* "=" expr ";"     (assignment path)
 *               | "if" "(" expr ")" block ("else" block)?
 *               | "while" "(" expr ")" block
 *   expr       := addsub (cmpop addsub)?      (NON-associative, M3)
 *   primary    := INT | "-" INT | "(" expr ")"
 *               | IDENT (args)? (struct literal / call / path)
 *               | IDENT "." IDENT (args)?              (enum ctor)
 *               | "Option" "." ("None" | "Some" "(" expr ")")
 *               | "match" expr "{" (arm ",")* arm? "}"
 *   arm        := pattern "=>" expr
 *   pattern    := "_" | IDENT "." IDENT ("(" IDENT ("," IDENT)* ")")?
 *   postfix    := primary ("." IDENT)*    (field access; a payload call
 *                                           turns Type.Variant into a
 *                                           constructor expression)
 *
 * Types are parsed into the RawType pool (the checker resolves them
 * after all declarations are collected, so forward references between
 * structs work). Expressions are built into the program-wide Expr
 * pool; children are pool indices. Pools are bounded (ADR-008 decision
 * 7) and overflow with clean diagnostics.
 *
 * M4 notes: calls remain EXPRESSIONS only (ADR-010 decision 6). Unary
 * minus stays literal-only. A statement starting IDENT '.' is a field
 * assignment; a bare struct literal is not a statement.
 */
#include "kavir.h"
#include <string.h>
#include <stdarg.h>

typedef struct {
    const char *file;
    const Token *toks;
    int ntoks;
    int pos;
    int no_lit;   /* 1 = struct literals disabled (match scrutinee) */
    Program *prog;
} Parser;

static void copy_name(char *dst, const Token *t); /* fwd: below */

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

/* Adds a name to one of the small string pools; returns the pool index
 * or -1 with a diagnostic. */
static int pool_add_name(Parser *p, char (*pool)[KAVIR_MAX_NAME],
                         int *count, int cap, const Token *t,
                         const char *what) {
    if (*count >= cap) {
        kavir_error(p->file, t->line, t->col,
                    "too many %s (limit %d per program)", what, cap);
        return -1;
    }
    copy_name(pool[*count], t);
    return (*count)++;
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

/* unary := "-" INT | primary ; postfix := { "." IDENT }* with the
 * Type.Variant( payload-call form turning into an enum constructor.
 * Struct literals `Name { field: expr, ... }` are primary-position
 * only; `Option.None` / `Option.Some(e)` are dedicated primaries. */
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
                        "(computed operands are not in the language "
                        "scope)");
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
        int e, saved = p->no_lit;
        p->no_lit = 0; /* a parenthesized literal is fine */
        advance(p);
        e = parse_expr(p);
        p->no_lit = saved;
        if (e < 0) return -1;
        if (expect(p, T_RPAREN, "')'") != 0) return -1;
        return e;
    }
    if (t->kind == T_OPTION) {
        /* Option.None | Option.Some(expr) — T is inferred by the
         * checker from the expected type (ADR-011). */
        const Token *at = advance(p);
        Expr e;
        int node;
        memset(&e, 0, sizeof(e));
        e.kind = EX_OPT_CTOR;
        e.a = e.b = -1;
        e.ty = -1;
        e.line = at->line;
        e.col = at->col;
        if (expect(p, T_DOT, "'.' after 'Option'") != 0) return -1;
        if (peek(p)->kind != T_IDENT) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "expected 'None' or 'Some' after 'Option.'");
            return -1;
        }
        if (strcmp(peek(p)->lex, "None") == 0) {
            advance(p);
            e.aux = 0;
            e.nargs = 0;
        } else if (strcmp(peek(p)->lex, "Some") == 0) {
            advance(p);
            e.aux = 1;
            if (expect(p, T_LPAREN, "'(' after 'Option.Some'") != 0)
                return -1;
            if (e.nargs >= KAVIR_MAX_ARGS) {
                kavir_error(p->file, peek(p)->line, peek(p)->col,
                            "too many arguments");
                return -1;
            }
            e.args[e.nargs++] = 0;
            e.args[e.nargs - 1] = parse_expr(p);
            if (e.args[e.nargs - 1] < 0) return -1;
            if (expect(p, T_RPAREN, "')' to close Option.Some(..)") != 0)
                return -1;
        } else {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "expected 'None' or 'Some' after 'Option.' "
                        "(found '%s')", t->lex);
            return -1;
        }
        node = expr_alloc(p, &e);
        return node;
    }
    if (t->kind == T_MATCH) {
        /* match expr { arm, arm, ... } — arms pooled program-wide */
        const Token *at = advance(p);
        Expr e;
        int node, arm_start, own_arms;
        memset(&e, 0, sizeof(e));
        e.kind = EX_MATCH;
        e.a = -1;
        e.b = -1;
        e.ty = -1;
        e.line = at->line;
        e.col = at->col;
        {
            int saved = p->no_lit;
            p->no_lit = 1;
            e.a = parse_expr(p);
            p->no_lit = saved;
        }
        if (e.a < 0) return -1;
        if (expect(p, T_LBRACE, "'{' to open the match arms") != 0)
            return -1;
        arm_start = p->prog->narms;
        own_arms = 0;
        while (peek(p)->kind != T_RBRACE) {
            MatchArm *arm;
            if (p->prog->narms >= KAVIR_MAX_ARMS) {
                t = peek(p);
                kavir_error(p->file, t->line, t->col,
                            "too many match arms (limit %d per match)",
                            KAVIR_MAX_ARMS);
                return -1;
            }
            arm = &p->prog->arms[p->prog->narms];
            memset(arm, 0, sizeof(*arm));
            arm->variant = -1;
            arm->pat_ty = -1;
            arm->line = peek(p)->line;
            arm->col = peek(p)->col;
            if (peek(p)->kind == T_UNDERSCORE) {
                advance(p);
                arm->is_wild = 1;
            } else if (peek(p)->kind == T_OPTION) {
                /* Option.None / Option.Some(bind) patterns: the
                 * element type is fixed by the scrutinee; the pattern
                 * cannot name it (ADR-011) */
                advance(p);
                if (expect(p, T_DOT, "'.' after 'Option'") != 0) return -1;
                if (peek(p)->kind != T_IDENT) {
                    t = peek(p);
                    kavir_error(p->file, t->line, t->col,
                                "expected None or Some after Option. in "
                                "a match pattern");
                    return -1;
                }
                if (strcmp(peek(p)->lex, "None") == 0) {
                    advance(p);
                    arm->is_none = 1;
                    arm->nbinds = 0;
                } else {
                    if (strcmp(peek(p)->lex, "Some") != 0) {
                        t = peek(p);
                        kavir_error(p->file, t->line, t->col,
                                    "expected None or Some after "
                                    "Option., found '%s'", t->lex);
                        return -1;
                    }
                    advance(p);
                    arm->is_none = 0;
                    strcpy(arm->ty_name, "Option");
                                        if (peek(p)->kind != T_LPAREN) {
                        kavir_error(p->file, arm->line, arm->col,
                                    "Option.Some carries a payload; "
                                    "bind it: Option.Some(name)");
                        return -1;
                    }
                    advance(p);
                    arm->nbinds = 0;
                    while (peek(p)->kind != T_RPAREN) {
                        if (arm->nbinds >= KAVIR_MAX_VARIANT_WORDS) {
                            t = peek(p);
                            kavir_error(p->file, t->line, t->col,
                                        "too many pattern bindings "
                                        "(limit %d)",
                                        KAVIR_MAX_VARIANT_WORDS);
                            return -1;
                        }
                        if (peek(p)->kind != T_IDENT) {
                            t = peek(p);
                            kavir_error(p->file, t->line, t->col,
                                        "expected a binding name, "
                                        "found '%s'", t->lex);
                            return -1;
                        }
                        copy_name(arm->bind_names[arm->nbinds],
                                  advance(p));
                        arm->nbinds++;
                        if (peek(p)->kind == T_COMMA) {
                            advance(p);
                            continue;
                        }
                        break;
                    }
                    if (expect(p, T_RPAREN,
                               "')' to close the pattern bindings") != 0)
                        return -1;
                }
            } else if (peek(p)->kind == T_IDENT) {
                const Token *tn = advance(p);
                copy_name(arm->ty_name, tn);
                if (expect(p, T_DOT, "'.' between the type and variant "
                                        "names in a match pattern") != 0)
                    return -1;
                if (peek(p)->kind != T_IDENT) {
                    t = peek(p);
                    kavir_error(p->file, t->line, t->col,
                                "expected a variant name in the match "
                                "pattern, found '%s'", t->lex);
                    return -1;
                }
                copy_name(arm->variant_name, advance(p));
                if (peek(p)->kind == T_LPAREN) {
                    advance(p);
                    while (peek(p)->kind != T_RPAREN) {
                        if (arm->nbinds >= KAVIR_MAX_VARIANT_WORDS) {
                            t = peek(p);
                            kavir_error(p->file, t->line, t->col,
                                        "too many pattern bindings (limit "
                                        "%d per variant)",
                                        KAVIR_MAX_VARIANT_WORDS);
                            return -1;
                        }
                        if (peek(p)->kind != T_IDENT) {
                            t = peek(p);
                            kavir_error(p->file, t->line, t->col,
                                        "expected a binding name in the "
                                        "pattern, found '%s'", t->lex);
                            return -1;
                        }
                        copy_name(arm->bind_names[arm->nbinds],
                                  advance(p));
                        arm->nbinds++;
                        if (peek(p)->kind == T_COMMA) {
                            advance(p);
                            continue;
                        }
                        break;
                    }
                    if (expect(p, T_RPAREN, "')' to close the pattern "
                                            "bindings") != 0)
                        return -1;
                }
            } else {
                t = peek(p);
                kavir_error(p->file, t->line, t->col,
                            "expected '_' or 'Type.Variant' as a match "
                            "pattern, found '%s'",
                            t->kind == T_EOF ? "<end of file>" : t->lex);
                return -1;
            }
            if (expect(p, T_FATARROW, "'=>' between the pattern and the "
                                       "arm body") != 0)
                return -1;
            p->prog->narms++;
            own_arms++;
            arm->body = parse_expr(p);
            if (arm->body < 0) return -1;
            if (peek(p)->kind == T_COMMA) {
                advance(p);
                continue;
            }
            break;
        }
        if (expect(p, T_RBRACE, "'}' to close the match arms") != 0)
            return -1;
        e.aux = arm_start;
        /* count ONLY this match's own arms: arm bodies are parsed
         * inside the loop and a NESTED match appends its arms to the
         * same pool, so (narms - arm_start) would overcount */
        e.nargs = own_arms;
        if (e.nargs == 0) {
            kavir_error(p->file, at->line, at->col,
                        "a match needs at least one arm");
            return -1;
        }
        node = expr_alloc(p, &e);
        return node;
    }
    if (t->kind == T_IDENT) {
        /* variable / call / struct literal / constructor / field chain */
        const Token *id = advance(p);
        Expr e;
        int cur, segs = 0;
        if (peek(p)->kind == T_LPAREN) {
            /* plain call (M3 shape, unchanged) */
            int at_line = peek(p)->line, at_col = peek(p)->col;
            int n = 0;
            memset(&e, 0, sizeof(e));
            e.kind = EX_CALL;
            copy_name(e.name, id);
            e.ty = -1;
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
            cur = expr_alloc(p, &e);
            if (cur < 0) return -1;
            goto postfix;
        }
        if (peek(p)->kind == T_LBRACE && !p->no_lit) {
            /* struct literal: Name { field: expr, ... } — all fields,
             * in declaration order (checked later) */
            int at_line = peek(p)->line, at_col = peek(p)->col;
            char names[KAVIR_MAX_FIELDS][KAVIR_MAX_NAME];
            int pool_start = 0, n = 0;
            memset(&e, 0, sizeof(e));
            e.kind = EX_STRUCT_LIT;
            copy_name(e.name, id);
            e.ty = -1;
            e.line = at_line;
            e.col = at_col;
            e.aux = pool_start;
            advance(p); /* consume '{' */
            while (peek(p)->kind != T_RBRACE) {
                if (n >= KAVIR_MAX_FIELDS) {
                    kavir_error(p->file, peek(p)->line, peek(p)->col,
                                "struct literal has too many fields "
                                "(limit %d)", KAVIR_MAX_FIELDS);
                    return -1;
                }
                if (peek(p)->kind != T_IDENT) {
                    t = peek(p);
                    kavir_error(p->file, t->line, t->col,
                                "expected a field name inside the struct "
                                "literal, found '%s'", t->lex);
                    return -1;
                }
                copy_name(names[n], peek(p));
                advance(p);
                if (expect(p, T_COLON, "':' after the field name") != 0)
                    return -1;
                e.args[n] = parse_expr(p);
                if (e.args[n] < 0) return -1;
                n++;
                if (peek(p)->kind == T_COMMA) {
                    advance(p);
                    continue;
                }
                break;
            }
            if (expect(p, T_RBRACE, "'}' to close the struct literal") != 0)
                return -1;
            if (p->prog->nlitfields + n > KAVIR_MAX_LITFIELDS) {
                kavir_error(p->file, at_line, at_col,
                            "too many struct-literal field names "
                            "(limit %d per program)", KAVIR_MAX_LITFIELDS);
                return -1;
            }
            pool_start = p->prog->nlitfields;
            {
                int ci;
                for (ci = 0; ci < n; ci++) {
                    size_t li = 0;
                    while (names[ci][li] != '\0' &&
                           li < KAVIR_MAX_NAME - 1) {
                        p->prog->litfields[p->prog->nlitfields][li] =
                            names[ci][li];
                        li++;
                    }
                    p->prog->litfields[p->prog->nlitfields][li] = '\0';
                    p->prog->litfields[p->prog->nlitfields]
                                      [KAVIR_MAX_NAME - 1] = '\0';
                    p->prog->nlitfields++;
                }
            }
            e.aux = pool_start;
            e.nargs = n;
            cur = expr_alloc(p, &e);
            if (cur < 0) return -1;
            return cur;
        }
        /* bare identifier (possibly followed by a field chain) */
        cur = expr_ident(p, id);
        if (cur < 0) return -1;
    postfix:
        while (peek(p)->kind == T_DOT) {
            Expr f;
            const Token *seg;
            int node;
            advance(p); /* '.' */
            if (peek(p)->kind != T_IDENT) {
                t = peek(p);
                kavir_error(p->file, t->line, t->col,
                            "expected a field name after '.', found '%s'",
                            t->lex);
                return -1;
            }
            seg = advance(p);
            if (peek(p)->kind == T_LPAREN) {
                /* Type.Variant(payload...) constructor — legal only as
                 * the FIRST path step after a bare type name */
                int at_line = peek(p)->line, at_col = peek(p)->col;
                int n = 0;
                if (segs != 0) {
                    kavir_error(p->file, at_line, at_col,
                                "unexpected '(' after a field-access chain "
                                "(constructors look like Type.Variant(..))");
                    return -1;
                }
                memset(&e, 0, sizeof(e));
                e.kind = EX_ENUM_CTOR;
                copy_name(e.name, id);
                copy_name(e.name_b, seg);
                e.ty = -1;
                e.line = at_line;
                e.col = at_col;
                advance(p); /* '(' */
                if (peek(p)->kind != T_RPAREN) {
                    for (;;) {
                        int a;
                        if (n >= KAVIR_MAX_ARGS) {
                            kavir_error(p->file, peek(p)->line,
                                        peek(p)->col,
                                        "too many payload values (limit %d "
                                        "per variant)", KAVIR_MAX_ARGS);
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
                if (expect(p, T_RPAREN, "')' to close the constructor "
                                        "payload") != 0)
                    return -1;
                node = expr_alloc(p, &e);
                return node;
            }
            memset(&f, 0, sizeof(f));
            f.kind = EX_FIELD;
            f.a = cur;
            copy_name(f.name, seg);
            f.ty = -1;
            f.line = seg->line;
            f.col = seg->col;
            node = expr_alloc(p, &f);
            if (node < 0) return -1;
            cur = node;
            segs++;
        }
        return cur;
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

/* ---------------- M4: types, declarations, match ---------------- */

/* type := "i32" | IDENT | "Option" "[" type "]"
 * Appends a RawType and stores its pool index in *out. */
static int parse_type(Parser *p, int *out) {
    const Token *t = peek(p);
    RawType raw;
    memset(&raw, 0, sizeof(raw));
    raw.opt = -1;

    if (t->kind == T_I32) {
        strcpy(raw.base, "i32");
        advance(p);
    } else if (t->kind == T_IDENT) {
        copy_name(raw.base, t);
        advance(p);
    } else if (t->kind == T_OPTION) {
        advance(p);
        if (expect(p, T_LBRACKET, "'[' after 'Option'") != 0) return -1;
        if (parse_type(p, &raw.opt) != 0) return -1;
        if (expect(p, T_RBRACKET, "']' to close Option[..]") != 0) return -1;
        strcpy(raw.base, "Option");
    } else {
        kavir_error(p->file, t->line, t->col,
                    "expected a type ('i32', a struct/enum name, or "
                    "Option[...]'), found '%s'",
                    t->kind == T_EOF ? "<end of file>" : t->lex);
        return -1;
    }

    if (p->prog->nraws >= KAVIR_MAX_RAWS) {
        kavir_error(p->file, t->line, t->col,
                    "too many type annotations (limit %d per program)",
                    KAVIR_MAX_RAWS);
        return -1;
    }
    p->prog->raws[p->prog->nraws] = raw;
    *out = p->prog->nraws++;
    return 0;
}

static int parse_structdef(Parser *p) {
    TypeDef *ty;
    const Token *t;
    advance(p); /* 'struct' */

    if (p->prog->ntypes >= KAVIR_MAX_TYPES) {
        t = peek(p);
        kavir_error(p->file, t->line, t->col,
                    "too many type declarations (limit %d)",
                    KAVIR_MAX_TYPES);
        return -1;
    }
    ty = &p->prog->types[p->prog->ntypes];
    memset(ty, 0, sizeof(*ty));
    ty->kind = TY_STRUCT;

    t = peek(p);
    if (t->kind != T_IDENT) {
        kavir_error(p->file, t->line, t->col,
                    "expected a struct name after 'struct'");
        return -1;
    }
    copy_name(ty->name, t);
    advance(p);

    if (expect(p, T_LBRACE, "'{' to open the struct body") != 0) return -1;
    while (peek(p)->kind != T_RBRACE) {
        const Token *fname;
        if (ty->nfields >= KAVIR_MAX_FIELDS) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "struct '%s' has too many fields (limit %d)",
                        ty->name, KAVIR_MAX_FIELDS);
            return -1;
        }
        if (peek(p)->kind != T_IDENT) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "expected a field name in struct '%s', found "
                        "'%s'", ty->name, t->lex);
            return -1;
        }
        fname = advance(p);
        copy_name(ty->field_names[ty->nfields], fname);
        if (expect(p, T_COLON, "':' and the field type") != 0) return -1;
        if (parse_type(p, &ty->field_raw[ty->nfields]) != 0) return -1;
        ty->nfields++;
        if (peek(p)->kind == T_COMMA) {
            advance(p);
            continue;
        }
        break;
    }
    if (expect(p, T_RBRACE, "'}' to close the struct body") != 0) return -1;
    p->prog->ntypes++;
    return 0;
}

static int parse_enumdef(Parser *p) {
    TypeDef *ty;
    const Token *t;
    advance(p); /* 'enum' */

    if (p->prog->ntypes >= KAVIR_MAX_TYPES) {
        t = peek(p);
        kavir_error(p->file, t->line, t->col,
                    "too many type declarations (limit %d)",
                    KAVIR_MAX_TYPES);
        return -1;
    }
    ty = &p->prog->types[p->prog->ntypes];
    memset(ty, 0, sizeof(*ty));
    ty->kind = TY_ENUM;

    t = peek(p);
    if (t->kind != T_IDENT) {
        kavir_error(p->file, t->line, t->col,
                    "expected an enum name after 'enum'");
        return -1;
    }
    copy_name(ty->name, t);
    advance(p);

    if (expect(p, T_LBRACE, "'{' to open the enum body") != 0) return -1;
    while (peek(p)->kind != T_RBRACE) {
        const Token *vname;
        if (ty->nvariants >= KAVIR_MAX_VARIANTS) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "enum '%s' has too many variants (limit %d)",
                        ty->name, KAVIR_MAX_VARIANTS);
            return -1;
        }
        if (peek(p)->kind != T_IDENT) {
            t = peek(p);
            kavir_error(p->file, t->line, t->col,
                        "expected a variant name in enum '%s', found "
                        "'%s'", ty->name, t->lex);
            return -1;
        }
        vname = advance(p);
        copy_name(ty->variant_names[ty->nvariants], vname);
        if (peek(p)->kind == T_LPAREN) {
            advance(p);
            for (;;) {
                int r;
                if (ty->variant_nargs[ty->nvariants] >=
                    KAVIR_MAX_VARIANT_WORDS) {
                    t = peek(p);
                    kavir_error(p->file, t->line, t->col,
                                "variant payload too large (limit %d "
                                "words)", KAVIR_MAX_VARIANT_WORDS);
                    return -1;
                }
                if (parse_type(p, &r) != 0) return -1;
                ty->variant_raw[ty->nvariants]
                              [ty->variant_nargs[ty->nvariants]] = r;
                ty->variant_nargs[ty->nvariants]++;
                if (peek(p)->kind == T_COMMA) {
                    advance(p);
                    continue;
                }
                break;
            }
            if (expect(p, T_RPAREN, "')' to close the variant payload") != 0)
                return -1;
        }
        ty->nvariants++;
        if (peek(p)->kind == T_COMMA) {
            advance(p);
            continue;
        }
        break;
    }
    if (expect(p, T_RBRACE, "'}' to close the enum body") != 0) return -1;
    p->prog->ntypes++;
    return 0;
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
            if (parse_type(p, &st->raw_idx) != 0) return -1;
            st->ty = -1; /* resolved by the checker */
            if (expect(p, T_EQ, "'=' and an initializer in 'let'") != 0)
                return -1;
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
            /* assignment statement: root(.field)* = expr;  (calls are
             * NOT statements — ADR-010 decision 6; a bare struct
             * literal is not a statement either) */
            const Token *id = advance(p);
            st->kind = ST_ASSIGN;
            copy_name(st->name, id);
            st->path_start = -1;
            st->path_len = 0;
            st->word_off = -1;
            st->ty = -1;
            if (peek(p)->kind == T_DOT) {
                /* field path: segments after the root go into the
                 * paths pool (checker validates every step) */
                st->path_start = p->prog->npaths;
                while (peek(p)->kind == T_DOT) {
                    int seg;
                    advance(p);
                    if (peek(p)->kind != T_IDENT) {
                        t = peek(p);
                        kavir_error(p->file, t->line, t->col,
                                    "expected a field name after '.' in "
                                    "the assignment target, found '%s'",
                                    t->lex);
                        return -1;
                    }
                    seg = pool_add_name(p, p->prog->paths,
                                        &p->prog->npaths,
                                        KAVIR_MAX_PATHS, peek(p),
                                        "assignment path segments");
                    if (seg < 0) return -1;
                    advance(p);
                    st->path_len++;
                    if (st->path_len > KAVIR_MAX_ASSIGN_PATH) {
                        kavir_error(p->file, st->line, st->col,
                                    "assignment path too deep (limit %d "
                                    "field steps)", KAVIR_MAX_ASSIGN_PATH);
                        return -1;
                    }
                }
            }
            if (peek(p)->kind != T_EQ) {
                kavir_error(p->file, peek(p)->line, peek(p)->col,
                            "expected '=' after '%s' in a statement "
                            "(call statements and bare struct literals "
                            "do not exist; bind results with 'let x = "
                            "...')", id->lex);
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
            if (parse_type(p, &fn->param_raw[fn->nparams]) != 0) return -1;
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
    if (parse_type(p, &fn->ret_raw) != 0) return -1;

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
    p.no_lit = 0;
    p.prog = prog;

    memset(prog, 0, sizeof(*prog));

    if (toks[0].kind == T_EOF) {
        kavir_error(file, 1, 1, "empty source file");
        return -1;
    }

    while (peek(&p)->kind != T_EOF) {
        int k = peek(&p)->kind;
        if (k == T_STRUCT) {
            if (parse_structdef(&p) != 0) return -1;
            continue;
        }
        if (k == T_ENUM) {
            if (parse_enumdef(&p) != 0) return -1;
            continue;
        }
        if (k != T_FN) {
            const Token *t = peek(&p);
            kavir_error(file, t->line, t->col,
                        "expected 'fn', 'struct' or 'enum' at top level, "
                        "found '%s'", t->lex);
            return -1;
        }
        if (parse_fndef(&p, prog) != 0) return -1;
        prog->nfns++;
    }
    return 0;
}
