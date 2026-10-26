/* check.c — Kavir semantic checker (M4).
 *
 * M4 additions over M3 (ADR-004 v0.1.0 subset; ADR-011 decisions):
 *   - type declarations: struct fields and enum variants (payloads are
 *     i32 or previously-nameable aggregate types — everything is a
 *     tuple of 32-bit words); C layout offsets computed here; recursive
 *     types rejected; Option[T] monomorphizations synthesized on
 *     demand (T inferred from the expected type at construction);
 *   - resolved signatures: parameters and return values may be
 *     aggregates; main must be `fn main() -> i32` with no parameters;
 *   - word-counted flat scopes: a variable of a k-word type occupies k
 *     consecutive vregs (ir.c rebuilds the same mapping);
 *   - struct literals: every field exactly once, in declaration order;
 *   - field access (rvalue) and field assignment through `let mut`
 *     roots (whole-value field assignment allowed, same-type rule);
 *   - enum constructors Type.Variant(e, ...); match with
 *     exhaustiveness (every variant covered, or a final _), duplicate
 *     patterns rejected, arm-local bindings that may not shadow outer
 *     variables (two arms may reuse a name — bindings vanish with the
 *     arm);
 *   - match arm bodies are i32 in M4 (the only aggregate deconstruction
 *     is pattern binding); print requires i32.
 *
 * Arithmetic is NOT evaluated or folded here (ADR-008 decision 2).
 */
#include "kavir.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define KAVIR_INT32_MIN_AS_ULL 2147483648ULL /* = (unsigned)INT32_MIN */

/* Flat per-function scope. vreg_base is the first of nwords
 * consecutive vregs (params first, then lets in declaration order) —
 * ir.c rebuilds the identical mapping independently. */
typedef struct {
    char name[KAVIR_MAX_NAME];
    int is_param;
    int is_mut;
    int ty;
    int nwords;
    int vreg_base;
} Var;

typedef struct {
    Var vars[KAVIR_MAX_VARS];
    int nvars;
    int nparams;
    int next_vreg; /* running vreg (word) allocation cursor */
} Scope;

static const TypeDef *find_type(const Program *prog, const char *name) {
    int i;
    for (i = 0; i < prog->ntypes; i++)
        if (strcmp(prog->types[i].name, name) == 0) return &prog->types[i];
    return NULL;
}

static int find_type_idx(const Program *prog, const char *name) {
    int i;
    for (i = 0; i < prog->ntypes; i++)
        if (strcmp(prog->types[i].name, name) == 0) return i;
    return -1;
}

static int ty_words(const Program *prog, int ty) {
    if (ty == KAVIR_TY_I32) return 1;
    return prog->types[ty - 1].size_words;
}

static const char *ty_name(const Program *prog, int ty) {
    if (ty == KAVIR_TY_I32) return "i32";
    return prog->types[ty - 1].name;
}

static int is_enum_ty(const Program *prog, int ty) {
    return ty > 0 && prog->types[ty - 1].kind == TY_ENUM;
}

static int is_option_ty(const Program *prog, int ty) {
    return ty > 0 && prog->types[ty - 1].is_option;
}

/* Layout state (per check() run): 0 = untouched, 1 = on the
 * computation stack (recursive type detected), 2 = laid out. */
static unsigned char layout_state[KAVIR_MAX_TYPES];

static int ensure_layout(const char *file, Program *prog, int ty_id);

/* Resolves a raw type expression to a type id, synthesizing Option[T]
 * monomorphizations on demand. depth caps Option[Option[...]] nesting.
 * The returned type is ALWAYS laid out (size_words valid). */
static int resolve_raw(const char *file, Program *prog, int raw_idx,
                       int depth, int line, int col) {
    const RawType *raw;
    int elem = KAVIR_TY_I32;

    if (raw_idx < 0 || raw_idx >= prog->nraws) {
        kavir_error(file, line, col, "internal: bad type annotation");
        return -1;
    }
    raw = &prog->raws[raw_idx];
    if (raw->opt < 0) {
        if (strcmp(raw->base, "i32") == 0) return KAVIR_TY_I32;
        {
            int ti = find_type_idx(prog, raw->base);
            if (ti < 0) {
                kavir_error(file, line, col,
                            "unknown type '%s' (type names are "
                            "capitalized and declared with 'struct' or "
                            "'enum')", raw->base);
                return -1;
            }
            /* layout happens in the final pass over ALL types (it
             * needs every forward reference resolved first, and it is
             * where recursive types are caught) */
            return ti + 1;
        }
    }
    /* Option[T] */
    if (depth >= KAVIR_MAX_OPT_DEPTH) {
        kavir_error(file, line, col,
                    "Option nesting too deep (limit %d)",
                    KAVIR_MAX_OPT_DEPTH);
        return -1;
    }
    elem = resolve_raw(file, prog, raw->opt, depth + 1, line, col);
    if (elem < 0) return -1;
    {
        char mono_name[KAVIR_MAX_NAME];
        int i;
        if (elem == KAVIR_TY_I32)
            snprintf(mono_name, sizeof(mono_name), "Option<i32>");
        else
            snprintf(mono_name, sizeof(mono_name), "Option<%s>",
                     prog->types[elem - 1].name);
        for (i = 0; i < prog->ntypes; i++)
            if (prog->types[i].is_option &&
                strcmp(prog->types[i].name, mono_name) == 0)
                return i + 1;
        if (prog->ntypes >= KAVIR_MAX_TYPES) {
            kavir_error(file, line, col,
                        "too many distinct Option types (limit %d)",
                        KAVIR_MAX_TYPES);
            return -1;
        }
        {
            TypeDef *mono = &prog->types[prog->ntypes];
            memset(mono, 0, sizeof(*mono));
            mono->kind = TY_ENUM;
            mono->is_option = 1;
            { int ci = 0;
              while (mono_name[ci] != '\0' && ci < KAVIR_MAX_NAME - 1) {
                  mono->name[ci] = mono_name[ci]; ci++; }
              mono->name[ci] = '\0'; }
            strcpy(mono->variant_names[0], "None");
            strcpy(mono->variant_names[1], "Some");
            mono->variant_nargs[0] = 0;
            mono->variant_nwords[0] = 0;
            mono->variant_ty[1][0] = elem;
            mono->elem_ty = elem;
            mono->variant_nargs[1] = 1;
                        mono->nvariants = 2;
            /* size_words finalized by ensure_layout below */
            prog->ntypes++;
            if (ensure_layout(file, prog, prog->ntypes) != 0) return -1;
            return prog->ntypes; /* id of the entry just appended */
        }
    }
}

/* Layout pass: fills field_off/size_words; rejects recursive types.
 * Called lazily from resolve_raw, so ty_words is valid for every type
 * id that resolve_raw has ever returned. */
static int ensure_layout(const char *file, Program *prog, int ty_id) {
    TypeDef *ty = &prog->types[ty_id - 1];
    int i, total;

    if (layout_state[ty_id - 1] == 2) return 0;
    if (layout_state[ty_id - 1] == 1) {
        kavir_error(file, 1, 1,
                    "recursive type '%s' (a type may not contain "
                    "itself, directly or indirectly)", ty->name);
        return -1;
    }
    layout_state[ty_id - 1] = 1;
    total = 0;
    if (ty->kind == TY_STRUCT) {
        for (i = 0; i < ty->nfields; i++) {
            int w;
            ty->field_off[i] = total;
            if (ty->field_ty[i] != KAVIR_TY_I32) {
                if (ensure_layout(file, prog, ty->field_ty[i]) != 0)
                    return -1;
            }
            w = ty_words(prog, ty->field_ty[i]);
            total += w;
        }
        if (total > KAVIR_MAX_STRUCT_WORDS) {
            kavir_error(file, 1, 1,
                        "struct '%s' is too large (%d words; limit %d)",
                        ty->name, total, KAVIR_MAX_STRUCT_WORDS);
            return -1;
        }
    } else {
        int maxw = 0;
        for (i = 0; i < ty->nvariants; i++) {
            int j, sum = 0;
            for (j = 0; j < ty->variant_nargs[i]; j++) {
                if (ty->variant_ty[i][j] != KAVIR_TY_I32) {
                    if (ensure_layout(file, prog, ty->variant_ty[i][j])
                        != 0)
                        return -1;
                }
                sum += ty_words(prog, ty->variant_ty[i][j]);
            }
            if (sum > KAVIR_MAX_VARIANT_WORDS) {
                kavir_error(file, 1, 1,
                            "variant payload of '%s' too large (%d words;"
                            " limit %d)", ty->name, sum,
                            KAVIR_MAX_VARIANT_WORDS);
                return -1;
            }
            ty->variant_nwords[i] = sum;
            if (sum > maxw) maxw = sum;
        }
        total = 1 + maxw;
    }
    ty->size_words = total;
    layout_state[ty_id - 1] = 2;
    return 0;
}

static int scope_find(const Scope *sc, const char *name) {
    int i;
    for (i = 0; i < sc->nvars; i++)
        if (strcmp(sc->vars[i].name, name) == 0) return i;
    return -1;
}

static int scope_add(const char *file, Scope *sc, const char *name,
                     int is_param, int is_mut, const Program *prog, int ty,
                     int line, int col) {
    int nw = ty_words(prog, ty);
    if (sc->nvars >= KAVIR_MAX_VARS) {
        kavir_error(file, line, col,
                    "too many variables (limit %d params + lets per "
                    "function)", KAVIR_MAX_VARS);
        return -1;
    }
    if (scope_find(sc, name) >= 0) {
        kavir_error(file, line, col,
                    "duplicate variable '%s' (no shadowing in the same "
                    "function)", name);
        return -1;
    }
    if (sc->next_vreg + nw > KAVIR_MAX_VREGS) {
        kavir_error(file, line, col,
                    "function frame too large (%d words; limit %d)",
                    sc->next_vreg + nw, KAVIR_MAX_VREGS);
        return -1;
    }
    strncpy(sc->vars[sc->nvars].name, name, KAVIR_MAX_NAME - 1);
    sc->vars[sc->nvars].name[KAVIR_MAX_NAME - 1] = '\0';
    sc->vars[sc->nvars].is_param = is_param;
    sc->vars[sc->nvars].is_mut = is_mut;
    sc->vars[sc->nvars].ty = ty;
    sc->vars[sc->nvars].nwords = nw;
    sc->vars[sc->nvars].vreg_base = sc->next_vreg;
    sc->next_vreg += nw;
    sc->nvars++;
    return 0;
}

static int find_fn(const Program *prog, const char *name) {
    int i;
    for (i = 0; i < prog->nfns; i++)
        if (strcmp(prog->fns[i].name, name) == 0) return i;
    return -1;
}

/* Checks an expression node; returns its resolved type id or -1.
 * expect_ty is the type the CONTEXT demands (used to infer Option[T];
 * -1 = unconstrained). */
static int expr_check(const char *file, Program *prog, int eidx,
                      Scope *sc, int expect_ty);

static int check_field_path(const char *file, Program *prog,
                            int root_ty, const Stmt *st,
                            int *out_off, int *out_ty) {
    int cur = root_ty, off = 0, i;
    for (i = 0; i < st->path_len; i++) {
        const char *seg = prog->paths[st->path_start + i];
        const TypeDef *td;
        int f;
        if (cur == KAVIR_TY_I32 ||
            prog->types[cur - 1].kind != TY_STRUCT) {
            kavir_error(file, st->line, st->col,
                        "cannot access field '%s' on a value of type "
                        "'%s' (only structs have named fields; enums "
                        "are deconstructed with match)", seg,
                        ty_name(prog, cur));
            return -1;
        }
        td = &prog->types[cur - 1];
        for (f = 0; f < td->nfields; f++)
            if (strcmp(td->field_names[f], seg) == 0) break;
        if (f >= td->nfields) {
            kavir_error(file, st->line, st->col,
                        "struct '%s' has no field '%s'", td->name, seg);
            return -1;
        }
        off += td->field_off[f];
        cur = td->field_ty[f];
    }
    *out_off = off;
    *out_ty = cur;
    return 0;
}

static int expr_check(const char *file, Program *prog, int eidx,
                      Scope *sc, int expect_ty) {
    Expr *e;

    if (eidx < 0 || eidx >= prog->nexprs) {
        kavir_error(file, 1, 1, "internal: expression index out of range");
        return -1;
    }
    e = &prog->exprs[eidx];

    switch (e->kind) {
    case EX_INT:
        if (e->val > (unsigned long long)INT32_MAX) {
            kavir_error(file, e->line, e->col,
                        "integer literal %llu does not fit in i32 "
                        "(range %d .. %d)", e->val, INT32_MIN, INT32_MAX);
            return -1;
        }
        e->ty = KAVIR_TY_I32;
        return e->ty;
    case EX_NEG:
        if (e->a < 0 || e->a >= prog->nexprs ||
            prog->exprs[e->a].kind != EX_INT) {
            kavir_error(file, e->line, e->col,
                        "internal: unary '-' operand is not a literal");
            return -1;
        }
        {
            Expr *inner = &prog->exprs[e->a];
            if (inner->val != KAVIR_INT32_MIN_AS_ULL &&
                inner->val > (unsigned long long)INT32_MAX) {
                kavir_error(file, inner->line, inner->col,
                            "integer literal %llu does not fit in i32 "
                            "(range %d .. %d)", inner->val,
                            INT32_MIN, INT32_MAX);
                return -1;
            }
        }
        e->ty = KAVIR_TY_I32;
        return e->ty;
    case EX_IDENT: {
        int vi = scope_find(sc, e->name);
        if (vi < 0) {
            kavir_error(file, e->line, e->col,
                        "undeclared variable '%s'", e->name);
            return -1;
        }
        e->ty = sc->vars[vi].ty;
        return e->ty;
    }
    case EX_BIN: {
        int lt, rt;
        lt = expr_check(file, prog, e->a, sc, KAVIR_TY_I32);
        if (lt < 0) return -1;
        rt = expr_check(file, prog, e->b, sc, KAVIR_TY_I32);
        if (rt < 0) return -1;
        if (lt != KAVIR_TY_I32 || rt != KAVIR_TY_I32) {
            kavir_error(file, e->line, e->col,
                        "arithmetic and comparisons require i32 operands "
                        "(got '%s' and '%s'; aggregates take part only "
                        "through calls, field access and match)",
                        ty_name(prog, lt), ty_name(prog, rt));
            return -1;
        }
        e->ty = KAVIR_TY_I32;
        return e->ty;
    }
    case EX_CALL: {
        int fi = find_fn(prog, e->name);
        int k;
        if (fi < 0) {
            kavir_error(file, e->line, e->col,
                        "call to undefined function '%s'", e->name);
            return -1;
        }
        if (e->nargs != prog->fns[fi].nparams) {
            kavir_error(file, e->line, e->col,
                        "function '%s' expects %d argument(s), got %d",
                        e->name, prog->fns[fi].nparams, e->nargs);
            return -1;
        }
        for (k = 0; k < e->nargs; k++) {
            int at = expr_check(file, prog, e->args[k], sc,
                                prog->fns[fi].param_ty[k]);
            if (at < 0) return -1;
            if (at != prog->fns[fi].param_ty[k]) {
                kavir_error(file, e->line, e->col,
                            "argument %d of '%s' must be '%s', got '%s'",
                            k + 1, e->name,
                            ty_name(prog, prog->fns[fi].param_ty[k]),
                            ty_name(prog, at));
                return -1;
            }
        }
        e->ty = prog->fns[fi].ret_ty;
        return e->ty;
    }
    case EX_STRUCT_LIT: {
        const TypeDef *td = find_type(prog, e->name);
        int i;
        if (td == NULL || td->kind != TY_STRUCT) {
            kavir_error(file, e->line, e->col,
                        "'%s' is not a declared struct (struct literals "
                        "look like 'Name { field: value, ... }')", e->name);
            return -1;
        }
        if (e->nargs != td->nfields) {
            kavir_error(file, e->line, e->col,
                        "struct '%s' has %d field(s), literal supplies %d",
                        td->name, td->nfields, e->nargs);
            return -1;
        }
        for (i = 0; i < e->nargs; i++) {
            const char *got = prog->litfields[e->aux + i];
            if (strcmp(got, td->field_names[i]) != 0) {
                kavir_error(file, e->line, e->col,
                            "field %d of '%s' must be '%s' (declaration "
                            "order), found '%s'", i + 1, td->name,
                            td->field_names[i], got);
                return -1;
            }
            if (expr_check(file, prog, e->args[i], sc, td->field_ty[i]) < 0)
                return -1;
        }
        e->ty = (int)(td - prog->types) + 1;
        return e->ty;
    }
    case EX_ENUM_CTOR: {
        const TypeDef *td = find_type(prog, e->name);
        int v, j;
        if (td == NULL || td->kind != TY_ENUM) {
            kavir_error(file, e->line, e->col,
                        "'%s' is not a declared enum (constructors look "
                        "like 'Name.Variant(value, ...)')", e->name);
            return -1;
        }
        for (v = 0; v < td->nvariants; v++)
            if (strcmp(td->variant_names[v], e->name_b) == 0) break;
        if (v >= td->nvariants) {
            kavir_error(file, e->line, e->col,
                        "enum '%s' has no variant '%s'", td->name,
                        e->name_b);
            return -1;
        }
        if (e->nargs != td->variant_nargs[v]) {
            kavir_error(file, e->line, e->col,
                        "variant '%s.%s' carries %d value(s), got %d",
                        td->name, td->variant_names[v],
                        td->variant_nargs[v], e->nargs);
            return -1;
        }
        for (j = 0; j < e->nargs; j++) {
            if (expr_check(file, prog, e->args[j], sc,
                           td->variant_ty[v][j]) < 0)
                return -1;
        }
        e->aux = v;
        e->ty = (int)(td - prog->types) + 1;
        return e->ty;
    }
    case EX_OPT_CTOR: {
        const TypeDef *td;
        if (expect_ty < 0 || !is_option_ty(prog, expect_ty)) {
            kavir_error(file, e->line, e->col,
                        "cannot infer the element type of Option.%s "
                        "(annotate the let, parameter or return type, "
                        "e.g. 'let o: Option[i32] = ...')",
                        e->aux == 0 ? "None" : "Some");
            return -1;
        }
        td = &prog->types[expect_ty - 1];
        if (e->aux == 0) { /* None */
            if (e->nargs != 0) {
                kavir_error(file, e->line, e->col,
                            "Option.None takes no value");
                return -1;
            }
        } else { /* Some */
            if (e->nargs != 1) {
                kavir_error(file, e->line, e->col,
                            "Option.Some takes exactly one value");
                return -1;
            }
            {
                int pt = expr_check(file, prog, e->args[0], sc,
                                    td->elem_ty);
                if (pt < 0) return -1;
                if (pt != td->elem_ty) {
                    kavir_error(file, e->line, e->col,
                                "Option.Some payload must be '%s', got "
                                "'%s'", ty_name(prog, td->elem_ty),
                                ty_name(prog, pt));
                    return -1;
                }
            }
        }
        e->ty = expect_ty;
        return e->ty;
    }
    case EX_FIELD: {
        const Expr *base = &prog->exprs[e->a];
        if (base->kind == EX_IDENT) {
            int ti = find_type_idx(prog, base->name);
            if (ti >= 0 && prog->types[ti].kind == TY_ENUM) {
                const TypeDef *td = &prog->types[ti];
                int v;
                for (v = 0; v < td->nvariants; v++)
                    if (strcmp(td->variant_names[v], e->name) == 0) break;
                if (v >= td->nvariants) {
                    kavir_error(file, e->line, e->col,
                                "enum '%s' has no variant '%s'", td->name,
                                e->name);
                    return -1;
                }
                if (td->variant_nargs[v] != 0) {
                    kavir_error(file, e->line, e->col,
                                "variant '%s.%s' carries a payload; call "
                                "it as '%s.%s(value, ...)'", td->name,
                                td->variant_names[v], td->name,
                                td->variant_names[v]);
                    return -1;
                }
                e->aux = v;
                e->b = 1; /* ir.c: this EX_FIELD is a bare ctor */
                e->ty = ti + 1;
                return e->ty;
            }
            if (ti >= 0 && prog->types[ti].kind == TY_STRUCT) {
                kavir_error(file, e->line, e->col,
                            "'%s' is a struct; construct it with a "
                            "literal: %s { field: value, ... }",
                            prog->types[ti].name, prog->types[ti].name);
                return -1;
            }
        }
        {
            int bt = expr_check(file, prog, e->a, sc, -1);
            const TypeDef *td;
            int f;
            if (bt < 0) return -1;
            if (bt == KAVIR_TY_I32 || prog->types[bt - 1].kind != TY_STRUCT)
            {
                kavir_error(file, e->line, e->col,
                            "cannot access field '%s' on a value of type "
                            "'%s' (only structs have named fields; enums "
                            "and Options are deconstructed with match)",
                            e->name, ty_name(prog, bt));
                return -1;
            }
            td = &prog->types[bt - 1];
            for (f = 0; f < td->nfields; f++)
                if (strcmp(td->field_names[f], e->name) == 0) break;
            if (f >= td->nfields) {
                kavir_error(file, e->line, e->col,
                            "struct '%s' has no field '%s'", td->name,
                            e->name);
                return -1;
            }
            e->aux = td->field_off[f];
            e->ty = td->field_ty[f];
            return e->ty;
        }
    }
    case EX_MATCH: {
        int st = expr_check(file, prog, e->a, sc, -1);
        const TypeDef *td;
        int i, arm0 = e->aux, narms = e->nargs;
        unsigned char covered[KAVIR_MAX_VARIANTS];
        int wild_seen = 0;
        for (i = 0; i < KAVIR_MAX_VARIANTS; i++) covered[i] = 0;
        if (st < 0) return -1;
        if (!is_enum_ty(prog, st)) {
            kavir_error(file, e->line, e->col,
                        "match requires an enum value (got '%s')",
                        ty_name(prog, st));
            return -1;
        }
        td = &prog->types[st - 1];
        for (i = 0; i < narms; i++) {
            MatchArm *arm = &prog->arms[arm0 + i];
            int save_vars, j, bt;
            if (wild_seen) {
                kavir_error(file, arm->line, arm->col,
                            "'_' must be the last match arm");
                return -1;
            }
            if (arm->is_wild) {
                if (arm->nbinds != 0) {
                    kavir_error(file, arm->line, arm->col,
                                "'_' binds nothing");
                    return -1;
                }
                wild_seen = 1;
            } else if (arm->is_none) {
                /* Option.None — requires an Option scrutinee */
                if (!is_option_ty(prog, st)) {
                    kavir_error(file, arm->line, arm->col,
                                "Option.None pattern requires an Option "
                                "scrutinee (scrutinee is '%s')",
                                ty_name(prog, st));
                    return -1;
                }
                if (arm->nbinds != 0) {
                    kavir_error(file, arm->line, arm->col,
                                "Option.None binds nothing");
                    return -1;
                }
                covered[0] = 1; /* variant 0 of every Option mono */
                arm->variant = 0;
                arm->pat_ty = st;
            } else if (strcmp(arm->ty_name, "Option") == 0) {
                /* Option.Some(bind...) */
                if (!is_option_ty(prog, st)) {
                    kavir_error(file, arm->line, arm->col,
                                "Option.Some pattern requires an Option "
                                "scrutinee (scrutinee is '%s')",
                                ty_name(prog, st));
                    return -1;
                }
                if (arm->nbinds != 1) {
                    kavir_error(file, arm->line, arm->col,
                                "Option.Some carries %d value(s); the "
                                "the pattern binds %d", arm->nbinds);
                    return -1;
                }
                arm->bind_ty[0] = td->elem_ty; /* Some carries one T */
                covered[1] = 1;
                arm->variant = 1;
                arm->pat_ty = st;
                {
                    int save_vars = sc->nvars;
                    int bt;
                    for (j = 0; j < arm->nbinds; j++) {
                        scope_add(file, sc, arm->bind_names[j], 0, 0,
                                  prog, arm->bind_ty[j], arm->line,
                                  arm->col);
                    }
                    bt = expr_check(file, prog, arm->body, sc,
                                    KAVIR_TY_I32);
                    sc->nvars = save_vars;
                    if (bt < 0) return -1;
                    if (bt != KAVIR_TY_I32) {
                        kavir_error(file, arm->line, arm->col,
                                    "match arm bodies must be i32 (got "
                                    "'%s')", ty_name(prog, bt));
                        return -1;
                    }
                }
                continue;
            } else {
                int v;
                if (strcmp(arm->ty_name, td->name) != 0) {
                    kavir_error(file, arm->line, arm->col,
                                "pattern type '%s' does not match the "
                                "scrutinee type '%s'", arm->ty_name,
                                td->name);
                    return -1;
                }
                for (v = 0; v < td->nvariants; v++)
                    if (strcmp(td->variant_names[v], arm->variant_name)
                        == 0)
                        break;
                if (v >= td->nvariants) {
                    kavir_error(file, arm->line, arm->col,
                                "enum '%s' has no variant '%s'", td->name,
                                arm->variant_name);
                    return -1;
                }
                if (covered[v]) {
                    kavir_error(file, arm->line, arm->col,
                                "duplicate pattern for variant '%s'",
                                td->variant_names[v]);
                    return -1;
                }
                covered[v] = 1;
                arm->variant = v;
                arm->pat_ty = st;
                if (arm->nbinds != td->variant_nargs[v]) {
                    kavir_error(file, arm->line, arm->col,
                                "variant '%s' carries %d value(s); the "
                                "pattern binds %d", td->variant_names[v],
                                td->variant_nargs[v], arm->nbinds);
                    return -1;
                }
                for (j = 0; j < arm->nbinds; j++)
                    arm->bind_ty[j] = td->variant_ty[v][j];
                /* arm-local bindings: not visible to other arms, and
                 * they may not shadow outer variables (M3 discipline) */
                save_vars = sc->nvars;
                for (j = 0; j < arm->nbinds; j++) {
                    if (scope_add(file, sc, arm->bind_names[j], 0, 0,
                                  prog, arm->bind_ty[j], arm->line,
                                  arm->col) != 0)
                        return -1;
                }
                bt = expr_check(file, prog, arm->body, sc, KAVIR_TY_I32);
                sc->nvars = save_vars; /* bindings vanish with the arm */
                if (bt < 0) return -1;
                if (bt != KAVIR_TY_I32) {
                    kavir_error(file, arm->line, arm->col,
                                "match arm bodies must be i32 (got '%s'; "
                                "deconstruct aggregates in the pattern "
                                "and return a word)", ty_name(prog, bt));
                    return -1;
                }
                continue;
            }
            {
                int bt = expr_check(file, prog, arm->body, sc,
                                    KAVIR_TY_I32);
                if (bt < 0) return -1;
                if (bt != KAVIR_TY_I32) {
                    kavir_error(file, arm->line, arm->col,
                                "match arm bodies must be i32 (got '%s')",
                                ty_name(prog, bt));
                    return -1;
                }
            }
        }
        if (!wild_seen) {
            for (i = 0; i < td->nvariants; i++) {
                if (!covered[i]) {
                    kavir_error(file, e->line, e->col,
                                "match is not exhaustive: variant '%s' "
                                "is not covered (add an arm or a final "
                                "'_')", td->variant_names[i]);
                    return -1;
                }
            }
        }
        e->ty = KAVIR_TY_I32;
        return e->ty;
    }
    }
    kavir_error(file, e->line, e->col, "internal: unknown expression kind");
    return -1;
}

static int stmt_check(const char *file, Program *prog, Stmt *st,
                      Scope *sc, int fn_ret_ty) {
    switch (st->kind) {
    case ST_RETURN: {
        int t = expr_check(file, prog, st->expr_idx, sc, fn_ret_ty);
        if (t < 0) return -1;
        if (t != fn_ret_ty) {
            kavir_error(file, st->line, st->col,
                        "return type mismatch: function returns '%s', "
                        "expression is '%s'", ty_name(prog, fn_ret_ty),
                        ty_name(prog, t));
            return -1;
        }
        return 0;
    }
    case ST_PRINT: {
        int t = expr_check(file, prog, st->expr_idx, sc, KAVIR_TY_I32);
        if (t < 0) return -1;
        if (t != KAVIR_TY_I32) {
            kavir_error(file, st->line, st->col,
                        "print requires an i32 value (got '%s'; "
                        "aggregates are not printable in M4)",
                        ty_name(prog, t));
            return -1;
        }
        return 0;
    }
    case ST_PRINT_STR:
        return 0; /* literal already validated by the lexer */
    case ST_LET: {
        int ty = resolve_raw(file, prog, st->raw_idx, 0, st->line, st->col);
        int it;
        if (ty < 0) return -1;
        /* the initializer runs BEFORE the binding exists */
        it = expr_check(file, prog, st->expr_idx, sc, ty);
        if (it < 0) return -1;
        if (it != ty) {
            kavir_error(file, st->line, st->col,
                        "let type mismatch: declared '%s', initializer "
                        "is '%s'", ty_name(prog, ty), ty_name(prog, it));
            return -1;
        }
        st->ty = ty;
        if (scope_add(file, sc, st->name, 0, st->is_mut, prog, ty,
                      st->line, st->col) != 0)
            return -1;
        return 0;
    }
    case ST_ASSIGN: {
        int vi = scope_find(sc, st->name);
        int rt;
        if (vi < 0) {
            kavir_error(file, st->line, st->col,
                        "undeclared variable '%s'", st->name);
            return -1;
        }
        if (sc->vars[vi].is_param) {
            kavir_error(file, st->line, st->col,
                        "cannot assign to parameter '%s' (parameters are "
                        "immutable)", st->name);
            return -1;
        }
        if (!sc->vars[vi].is_mut) {
            kavir_error(file, st->line, st->col,
                        "cannot assign to immutable variable '%s' "
                        "(declare it with 'let mut')", st->name);
            return -1;
        }
        if (st->path_len == 0) {
            st->word_off = 0;
            st->ty = sc->vars[vi].ty;
        } else {
            if (check_field_path(file, prog, sc->vars[vi].ty, st,
                                 &st->word_off, &st->ty) != 0)
                return -1;
        }
        rt = expr_check(file, prog, st->expr_idx, sc, st->ty);
        if (rt < 0) return -1;
        if (rt != st->ty) {
            kavir_error(file, st->line, st->col,
                        "assignment type mismatch: target is '%s', "
                        "value is '%s'", ty_name(prog, st->ty),
                        ty_name(prog, rt));
            return -1;
        }
        return 0;
    }
    case ST_IF: {
        Block *b;
        int s, t;
        t = expr_check(file, prog, st->expr_idx, sc, KAVIR_TY_I32);
        if (t < 0) return -1;
        if (t != KAVIR_TY_I32) {
            kavir_error(file, st->line, st->col,
                        "if condition must be i32 (got '%s')",
                        ty_name(prog, t));
            return -1;
        }
        b = &prog->blocks[st->then_block];
        for (s = 0; s < b->nstmts; s++)
            if (stmt_check(file, prog, &b->stmts[s], sc, fn_ret_ty) != 0)
                return -1;
        if (st->else_block >= 0) {
            b = &prog->blocks[st->else_block];
            for (s = 0; s < b->nstmts; s++)
                if (stmt_check(file, prog, &b->stmts[s], sc, fn_ret_ty)
                    != 0)
                    return -1;
        }
        return 0;
    }
    case ST_WHILE: {
        Block *b = &prog->blocks[st->body_block];
        int s, t;
        t = expr_check(file, prog, st->expr_idx, sc, KAVIR_TY_I32);
        if (t < 0) return -1;
        if (t != KAVIR_TY_I32) {
            kavir_error(file, st->line, st->col,
                        "while condition must be i32 (got '%s')",
                        ty_name(prog, t));
            return -1;
        }
        for (s = 0; s < b->nstmts; s++)
            if (stmt_check(file, prog, &b->stmts[s], sc, fn_ret_ty) != 0)
                return -1;
        return 0;
    }
    }
    kavir_error(file, st->line, st->col, "internal: unknown statement kind");
    return -1;
}

int check(const char *file, const Program *prog) {
    Program *mprog = (Program *)prog; /* annotation writes (ty fields) */
    int i, j;

    /* pass 0a: type declarations — names capitalized and unique */
    for (i = 0; i < prog->ntypes; i++) {
        const TypeDef *ty = &prog->types[i];
        if (ty->name[0] < 'A' || ty->name[0] > 'Z') {
            kavir_error(file, 1, 1,
                        "type name '%s' must start with an uppercase "
                        "letter (lowercase names are variables and "
                        "functions)", ty->name);
            return -1;
        }
        for (j = i + 1; j < prog->ntypes; j++) {
            if (strcmp(ty->name, prog->types[j].name) == 0) {
                kavir_error(file, 1, 1, "duplicate type name '%s'",
                            ty->name);
                return -1;
            }
        }
    }
    /* pass 0b: resolve user-type annotations (fields, variant payload
     * types); Option monomorphizations and layout happen lazily inside
     * resolve_raw, so monos are skipped here (they carry no raws). */
    for (i = 0; i < prog->ntypes; i++) {
        TypeDef *ty = &mprog->types[i];
        int f, v, w;
        if (ty->is_option) continue;
        for (f = 0; f < ty->nfields; f++) {
            ty->field_ty[f] = resolve_raw(file, mprog, ty->field_raw[f], 0,
                                          1, 1);
            if (ty->field_ty[f] < 0) return -1;
        }
        for (v = 0; v < ty->nvariants; v++) {
            for (w = 0; w < ty->variant_nargs[v]; w++) {
                int t = resolve_raw(file, mprog, ty->variant_raw[v][w], 0,
                                    1, 1);
                if (t < 0) return -1;
                ty->variant_ty[v][w] = t;
            }
        }
    }
    for (i = 0; i < prog->ntypes; i++) {
        if (ensure_layout(file, mprog, i + 1) != 0) return -1;
    }

    /* pass 1: function signatures */
    for (i = 0; i < prog->nfns; i++) {
        const Fn *fn = &prog->fns[i];
        int k;
        for (j = i + 1; j < prog->nfns; j++) {
            if (strcmp(fn->name, prog->fns[j].name) == 0) {
                kavir_error(file, prog->fns[j].line, prog->fns[j].col,
                            "duplicate function '%s'", fn->name);
                return -1;
            }
        }
        for (k = 0; k < fn->nparams; k++) {
            int m;
            for (m = 0; m < k; m++) {
                if (strcmp(fn->param_names[k], fn->param_names[m]) == 0) {
                    kavir_error(file, fn->line, fn->col,
                                "duplicate parameter '%s' in function "
                                "'%s'", fn->param_names[k], fn->name);
                    return -1;
                }
            }
        }
    }
    /* resolve signatures AFTER type names exist (annotations may name
     * types declared later in the file) */
    for (i = 0; i < prog->nfns; i++) {
        Fn *fn = &mprog->fns[i];
        int k, total = 0;
        for (k = 0; k < fn->nparams; k++) {
            fn->param_ty[k] = resolve_raw(file, mprog, fn->param_raw[k],
                                          0, fn->line, fn->col);
            if (fn->param_ty[k] < 0) return -1;
            total += ty_words(mprog, fn->param_ty[k]);
        }
        if (total + 8 > KAVIR_MAX_VREGS) {
            kavir_error(file, fn->line, fn->col,
                        "parameters of '%s' are too large (%d words)",
                        fn->name, total);
            return -1;
        }
        fn->ret_ty = resolve_raw(file, mprog, fn->ret_raw, 0, fn->line,
                                 fn->col);
        if (fn->ret_ty < 0) return -1;
    }
    {
        int mi = find_fn(prog, "main");
        if (mi < 0) {
            kavir_error(file, 1, 1,
                        "no 'main' function found (programs need "
                        "'fn main() -> i32')");
            return -1;
        }
        if (prog->fns[mi].nparams != 0) {
            kavir_error(file, prog->fns[mi].line, prog->fns[mi].col,
                        "main must take no parameters (the runtime calls "
                        "it with none)");
            return -1;
        }
        if (prog->fns[mi].ret_ty != KAVIR_TY_I32) {
            kavir_error(file, prog->fns[mi].line, prog->fns[mi].col,
                        "main must return i32");
            return -1;
        }
    }

    /* pass 2: bodies */
    for (i = 0; i < prog->nfns; i++) {
        const Fn *fn = &prog->fns[i];
        Block *body = (Block *)&prog->blocks[fn->body_block];
        Scope sc;
        int s;
        memset(&sc, 0, sizeof(sc));
        for (s = 0; s < fn->nparams; s++) {
            if (scope_add(file, &sc, fn->param_names[s], 1, 0, prog,
                          fn->param_ty[s], fn->line, fn->col) != 0)
                return -1;
        }
        for (s = 0; s < body->nstmts; s++)
            if (stmt_check(file, mprog, &body->stmts[s], &sc,
                           fn->ret_ty) != 0)
                return -1;
    }
    return 0;
}
