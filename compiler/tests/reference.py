#!/usr/bin/env python3
"""tests/reference.py — INDEPENDENT reference evaluator for the Kavir
surface through M3 (ADR-007 decision 2; ADR-008 decisions 1-4, 8;
ADR-010 decisions 1-9 for calls/recursion).

This file is written from the ADR PROSE ONLY, never from the compiler
sources: it is the third arm of every conformance case.

    ELF (stdout, stderr, exit)  ==  IR interpreter  ==  this evaluator

Semantics implemented here (ADR-007 + ADR-008 + ADR-010):
  - i32 signed arithmetic, evaluated at "runtime": left operand, then
    right operand, then the operation; statements in order; call
    arguments left-to-right, then the call;
  - comparisons < <= > >= == != yield i32 1/0 (no overflow possible);
  - an out-of-range result of + - * / % is a TRAP "integer overflow";
  - division/remainder truncate toward zero (C semantics);
  - division by zero is a TRAP "division by zero";
  - INT32_MIN / -1 and INT32_MIN % -1 are TRAPs "integer overflow";
  - print(int) writes decimal with '-' for negatives, nothing else;
  - print("...") writes the literal bytes; escapes \\n \\t \\r \\\" \\\\;
  - a trap writes "trap: <kind> at <file>:<line>:<col>\\n" to stderr and
    the process exits 101; <file> is the input path as given;
  - let x: i32 = e; binds immutably, let mut binds mutably; x = e; is
    an assignment STATEMENT (assignment is not an expression); flat
    per-function scope, no shadowing, parameters immutable; the
    initializer is evaluated BEFORE the binding exists;
  - functions may be called forward and recursively (signatures are
    collected first); arity must match; falling off the end of any
    function returns 0;
  - CALL DEPTH LIMIT (ADR-010, user decision 2026-10-26): entering a
    1025th simultaneous frame traps with
    "trap: call depth limit exceeded (1024) in fn 'NAME' at
     FILE:<def line>:<def col>"
    where NAME/position identify the callee whose entry was rejected;
  - unary minus on literals is numeral interpretation: -2147483648 is
    INT32_MIN; a bare 2147483648 (not under unary minus) is a compile
    error, as is any literal outside i32.

CLI:  python3 tests/reference.py <file.k>
      prints one JSON line: {"stdout": b64, "stderr": b64, "exit": n}

Module: expected_for(path) -> (stdout: bytes, stderr: bytes, exit: int)
Raises ReferenceError on anything it cannot parse — for programs the
compiler accepted that is a FINDING, not a nuisance.

Deep recursion runs on a dedicated thread with a large stack so the
1024-frame limit is exercisable without hitting CPython's own limits.
"""
import base64
import json
import sys
import threading

INT32_MIN = -2147483648
INT32_MAX = 2147483647
U64_MAX = 18446744073709551615
TRAP_EXIT = 101
CALL_DEPTH_LIMIT = 1024

KEYWORDS = {"fn", "return", "i32", "if", "else", "print",
            "let", "mut", "while"}


class ReferenceError(Exception):
    pass


# ---------------------------------------------------------------- lexer

class Tok:
    __slots__ = ("kind", "val", "s", "line", "col")

    def __init__(self, kind, val=None, s="", line=0, col=0):
        self.kind, self.val, self.s = kind, val, s
        self.line, self.col = line, col


def tokenize(src, path):
    toks = []
    line, col = 1, 1
    i, n = 0, len(src)

    def adv(k=1):
        nonlocal line, col, i
        for _ in range(k):
            if i < n and src[i] == "\n":
                line += 1
                col = 1
            else:
                col += 1
            i += 1

    while i < n:
        c = src[i]
        if c in " \t\r\n":
            adv()
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            while i < n and src[i] != "\n":
                adv()
            continue
        tl, tc = line, col
        if c == '"':
            adv()
            out = bytearray()
            while True:
                if i >= n or src[i] == "\n":
                    raise ReferenceError("%s:%d:%d: unterminated string"
                                         % (path, tl, tc))
                ch = src[i]
                if ch == '"':
                    adv()
                    break
                if ch == "\\":
                    adv()
                    if i >= n:
                        raise ReferenceError("%s:%d:%d: unterminated string"
                                             % (path, tl, tc))
                    e = src[i]
                    mapping = {"n": "\n", "t": "\t", "r": "\r",
                               "\\": "\\", '"': '"'}
                    if e not in mapping:
                        raise ReferenceError(
                            "%s:%d:%d: unknown escape \\%s" % (path, line, col, e))
                    out += mapping[e].encode("utf-8")
                    adv()
                    continue
                out += ch.encode("utf-8")
                adv()
            toks.append(Tok("STR", bytes(out), line=tl, col=tc))
            continue
        if c.isdigit():
            j = i
            while j < n and src[j].isdigit():
                j += 1
            text = src[i:j]
            v = int(text)
            if v > U64_MAX:
                raise ReferenceError("%s:%d:%d: integer literal too large"
                                     % (path, tl, tc))
            toks.append(Tok("INT", v, text, tl, tc))
            adv(j - i)
            continue
        if c.isalpha() or c == "_":
            j = i
            while j < n and (src[j].isalnum() or src[j] == "_"):
                j += 1
            word = src[i:j]
            kind = word if word in KEYWORDS else "IDENT"
            toks.append(Tok(kind, None, word, tl, tc))
            adv(j - i)
            continue
        two = src[i:i + 2]
        if two == "->":
            toks.append(Tok("ARROW", s="->", line=tl, col=tc))
            adv(2)
            continue
        if two in ("<=", ">=", "==", "!="):
            toks.append(Tok(two, s=two, line=tl, col=tc))
            adv(2)
            continue
        if c == "!":
            raise ReferenceError("%s:%d:%d: unexpected '!' (only '!=' exists)"
                                 % (path, tl, tc))
        single = {"+": "+", "-": "-", "*": "*", "/": "/", "%": "%",
                  "(": "(", ")": ")", "{": "{", "}": "}", ";": ";",
                  ":": ":", ",": ",", "=": "=", "<": "<", ">": ">"}
        if c in single:
            toks.append(Tok(single[c], s=c, line=tl, col=tc))
            adv()
            continue
        raise ReferenceError("%s:%d:%d: unexpected character %r"
                             % (path, tl, tc, c))
    toks.append(Tok("EOF", line=line, col=col))
    return toks


# --------------------------------------------------------------- parser
# expr: ('int', v, line, col) | ('neg', child, line, col)
#     | ('ident', name, line, col)
#     | ('call', name, [args], line, col)
#     | ('bin', op, a, b, opline, opcol)
# stmt: ('return', e) | ('printi', e) | ('prints', bytes)
#     | ('if', e, then, else) | ('while', e, body)
#     | ('let', name, is_mut, e) | ('assign', name, e)

class Parser:
    def __init__(self, toks, path):
        self.toks = toks
        self.pos = 0
        self.path = path

    def peek(self):
        return self.toks[self.pos]

    def next(self):
        t = self.toks[self.pos]
        if t.kind != "EOF":
            self.pos += 1
        return t

    def expect(self, kind, what):
        t = self.peek()
        if t.kind != kind:
            raise ReferenceError("%s:%d:%d: expected %s, found %r"
                                 % (self.path, t.line, t.col, what, t.s))
        return self.next()

    def expr(self):
        node = self.addsub()
        if self.peek().kind in ("<", "<=", ">", ">=", "==", "!="):
            op = self.next()
            rhs = self.addsub()
            node = ("bin", op.kind, node, rhs, op.line, op.col)
            if self.peek().kind in ("<", "<=", ">", ">=", "==", "!="):
                t = self.peek()
                raise ReferenceError(
                    "%s:%d:%d: comparison operators are non-associative"
                    % (self.path, t.line, t.col))
        return node

    def addsub(self):
        node = self.muldiv()
        while self.peek().kind in ("+", "-"):
            op = self.next()
            rhs = self.muldiv()
            node = ("bin", op.kind, node, rhs, op.line, op.col)
        return node

    def muldiv(self):
        node = self.unary()
        while self.peek().kind in ("*", "/", "%"):
            op = self.next()
            rhs = self.unary()
            node = ("bin", op.kind, node, rhs, op.line, op.col)
        return node

    def unary(self):
        t = self.peek()
        if t.kind == "-":
            self.next()
            lit = self.peek()
            if lit.kind != "INT":
                raise ReferenceError(
                    "%s:%d:%d: unary '-' applies to integer literals only"
                    % (self.path, lit.line, lit.col))
            self.next()
            return ("neg", ("int", lit.val, lit.line, lit.col), t.line, t.col)
        if t.kind == "INT":
            self.next()
            return ("int", t.val, t.line, t.col)
        if t.kind == "IDENT":
            self.next()
            if self.peek().kind == "(":
                self.next()
                args = []
                if self.peek().kind != ")":
                    while True:
                        args.append(self.expr())
                        if self.peek().kind == ",":
                            self.next()
                            continue
                        break
                self.expect(")", "')'")
                return ("call", t.s, args, t.line, t.col)
            return ("ident", t.s, t.line, t.col)
        if t.kind == "(":
            self.next()
            e = self.expr()
            self.expect(")", "')'")
            return e
        raise ReferenceError("%s:%d:%d: expected expression, found %r"
                             % (self.path, t.line, t.col, t.s))

    def block(self):
        self.expect("{", "'{'")
        stmts = []
        while self.peek().kind != "}":
            if self.peek().kind == "EOF":
                raise ReferenceError("%s: unexpected EOF in block"
                                     % self.path)
            stmts.append(self.stmt())
        self.next()
        return stmts

    def stmt(self):
        t = self.peek()
        if t.kind == "return":
            self.next()
            e = self.expr()
            self.expect(";", "';'")
            return ("return", e)
        if t.kind == "print":
            self.next()
            self.expect("(", "'('")
            if self.peek().kind == "STR":
                s = self.next()
                self.expect(")", "')'")
                self.expect(";", "';'")
                return ("prints", s.val)
            e = self.expr()
            self.expect(")", "')'")
            self.expect(";", "';'")
            return ("printi", e)
        if t.kind == "let":
            self.next()
            is_mut = False
            if self.peek().kind == "mut":
                self.next()
                is_mut = True
            name = self.expect("IDENT", "variable name")
            self.expect(":", "':'")
            self.expect("i32", "'i32'")
            self.expect("=", "'='")
            e = self.expr()
            self.expect(";", "';'")
            return ("let", name.s, is_mut, e)
        if t.kind == "while":
            self.next()
            self.expect("(", "'('")
            e = self.expr()
            self.expect(")", "')'")
            body = self.block()
            return ("while", e, body)
        if t.kind == "if":
            self.next()
            self.expect("(", "'('")
            e = self.expr()
            self.expect(")", "')'")
            then = self.block()
            els = []
            if self.peek().kind == "else":
                self.next()
                els = self.block()
            return ("if", e, then, els)
        if t.kind == "IDENT":
            self.next()
            self.expect("=", "'=' (assignment)")
            e = self.expr()
            self.expect(";", "';'")
            return ("assign", t.s, e)
        raise ReferenceError("%s:%d:%d: unexpected %r in block"
                             % (self.path, t.line, t.col, t.s))

    def program(self):
        fns = []
        while self.peek().kind != "EOF":
            fntok = self.expect("fn", "'fn'")
            name = self.expect("IDENT", "function name")
            self.expect("(", "'('")
            params = []
            if self.peek().kind != ")":
                while True:
                    p = self.expect("IDENT", "parameter name")
                    self.expect(":", "':'")
                    self.expect("i32", "'i32'")
                    params.append(p.s)
                    if self.peek().kind == ",":
                        self.next()
                        continue
                    break
            self.expect(")", "')'")
            self.expect("ARROW", "'->'")
            self.expect("i32", "'i32'")
            body = self.block()
            # definition position = the 'fn' keyword (matches the
            # compiler's fn->line/col used in trap messages)
            fns.append((name.s, params, body, fntok.line, fntok.col))
        return fns


# ------------------------------------------------------------ evaluator

class Trap(Exception):
    def __init__(self, kind, line, col):
        self.kind, self.line, self.col = kind, line, col


def c_div(a, b):
    """C truncated division on exact ints (callers pre-check traps)."""
    q = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        q = -q
    return q


def c_rem(a, b):
    return a - c_div(a, b) * b


CMP = {"<": lambda a, b: a < b, "<=": lambda a, b: a <= b,
       ">": lambda a, b: a > b, ">=": lambda a, b: a >= b,
       "==": lambda a, b: a == b, "!=": lambda a, b: a != b}


class ReturnSignal(Exception):
    """Early return propagating out of nested statement lists."""

    def __init__(self, value):
        self.value = value


class Evaluator:
    def __init__(self, path):
        self.out = bytearray()
        self.path = path
        self.fns = {}     # name -> (params, body, line, col)
        self.depth = 0

    def trap(self, kind, line, col):
        raise Trap(kind, line, col)

    def eval(self, node, env):
        kind = node[0]
        if kind == "int":
            v = node[1]
            # a bare literal above INT32_MAX never reaches execution: it
            # is a compile error; -2147483648 arrives via 'neg'
            if v > INT32_MAX:
                raise ReferenceError("literal %d out of i32 at execution"
                                     % v)
            return v
        if kind == "neg":
            child = node[1]
            if child[0] != "int":
                raise ReferenceError("unary '-' on a non-literal")
            v = -child[1]
            if v < INT32_MIN or v > INT32_MAX:
                raise ReferenceError("negated literal %d out of i32" % v)
            return v
        if kind == "ident":
            name = node[1]
            if name not in env:
                raise ReferenceError("undeclared variable %r" % name)
            return env[name]
        if kind == "call":
            return self.call(node[1], node[2], node[3], node[4], env)
        if kind == "bin":
            _, op, a, b, opline, opcol = node
            va = self.eval(a, env)   # evaluation order: lhs, rhs, op
            vb = self.eval(b, env)
            if op in CMP:
                return 1 if CMP[op](va, vb) else 0
            if op == "+":
                r = va + vb
            elif op == "-":
                r = va - vb
            elif op == "*":
                r = va * vb
            elif op == "/":
                if vb == 0:
                    self.trap("division by zero", opline, opcol)
                if va == INT32_MIN and vb == -1:
                    self.trap("integer overflow", opline, opcol)
                r = c_div(va, vb)
            elif op == "%":
                if vb == 0:
                    self.trap("division by zero", opline, opcol)
                if va == INT32_MIN and vb == -1:
                    self.trap("integer overflow", opline, opcol)
                r = c_rem(va, vb)
            else:
                raise ReferenceError("unknown operator %r" % op)
            if r < INT32_MIN or r > INT32_MAX:
                self.trap("integer overflow", opline, opcol)
            return r
        raise ReferenceError("unknown expression node %r" % (kind,))

    def call(self, name, argexprs, cline, ccol, caller_env):
        if name not in self.fns:
            raise ReferenceError("call to undefined function %r" % name)
        params, body, dline, dcol = self.fns[name]
        if len(argexprs) != len(params):
            raise ReferenceError(
                "function %r expects %d argument(s), got %d"
                % (name, len(params), len(argexprs)))
        # arguments evaluate left-to-right in the CALLER's environment,
        # before the call
        vals = [self.eval(a, caller_env) for a in argexprs]
        if self.depth + 1 >= CALL_DEPTH_LIMIT:
            # the callee would be frame CALL_DEPTH_LIMIT+1 (depth index
            # CALL_DEPTH_LIMIT): frame indices 0..CALL_DEPTH_LIMIT-1 are
            # legal — identical to the interpreter's frame-stack check
            # and the ELF's prologue check (ADR-010 section 3b)
            self.trap("call depth limit exceeded (%d) in fn '%s'"
                      % (CALL_DEPTH_LIMIT, name), dline, dcol)
        self.depth += 1
        env = dict(zip(params, vals))
        try:
            v = self.run_stmts(body, env)
        except ReturnSignal as r:
            v = r.value  # the ONLY catch point below the top level
        finally:
            self.depth -= 1
        return 0 if v is None else v

    def run_stmts(self, stmts, env):
        # NOTE: ReturnSignal propagates UNCAUGHT through nested statement
        # lists (if/while bodies) and is caught only at the call boundary
        # (in call()) or at the top level around main — catching it here
        # would swallow early returns and corrupt control flow.
        for st in stmts:
            k = st[0]
            if k == "printi":
                v = self.eval(st[1], env)
                self.out += str(v).encode("ascii")
            elif k == "prints":
                self.out += st[1]
            elif k == "if":
                c = self.eval(st[1], env)
                self.run_stmts(st[2] if c != 0 else st[3], env)
            elif k == "while":
                while self.eval(st[1], env) != 0:
                    self.run_stmts(st[2], env)
            elif k == "let":
                v = self.eval(st[3], env)  # before the binding exists
                # NOTE: the no-shadowing rule is checked ONCE per
                # let SITE (static check in expected_for); a let
                # inside a loop re-binds on every iteration.
                env[st[1]] = v
            elif k == "assign":
                if st[1] not in env:
                    raise ReferenceError("undeclared variable %r"
                                         % st[1])
                env[st[1]] = self.eval(st[2], env)
            elif k == "return":
                raise ReturnSignal(self.eval(st[1], env))
            else:
                raise ReferenceError("unknown statement %r" % (k,))
        return None


def _run_with_big_stack(fn):
    """Runs fn() on a thread with a large stack; returns its result or
    re-raises. Needed because 1024 Kavir frames recurse deeply in the
    evaluator's own Python frames."""
    box = {}

    def target():
        try:
            box["ok"] = fn()
        except BaseException as e:  # noqa: BLE001 - re-raised below
            box["err"] = e

    old = threading.stack_size()
    try:
        threading.stack_size(512 * 1024 * 1024)
    except (ValueError, RuntimeError):
        pass
    t = threading.Thread(target=target)
    t.start()
    t.join()
    try:
        threading.stack_size(old)
    except (ValueError, RuntimeError):
        pass
    if "err" in box:
        raise box["err"]
    return box.get("ok")


def _static_checks(fns, path):
    """Advisory static checks mirroring the documented compile rules
    (only reached for programs the compiler accepted; a raise here is a
    FINDING). Flat per-function scope: let names must be unique within a
    function (including params)."""
    for (name, params, body, line, col) in fns:
        seen = list(params)

        def walk(stmts):
            for st in stmts:
                if st[0] == "let":
                    if st[1] in seen:
                        raise ReferenceError(
                            "%s:%d:%d: duplicate variable %r in fn %r"
                            % (path, line, col, st[1], name))
                    seen.append(st[1])
                elif st[0] == "if":
                    walk(st[2])
                    walk(st[3])
                elif st[0] == "while":
                    walk(st[2])

        walk(body)


def expected_for(path):
    """Computes (stdout, stderr, exit) for a source file, independently
    of the compiler. Raises ReferenceError for anything unparsable."""
    with open(path, "rb") as f:
        src = f.read().decode("utf-8", "surrogateescape")
    toks = tokenize(src, path)
    fns = Parser(toks, path).program()
    _static_checks(fns, path)
    ev = Evaluator(path)
    for (name, params, body, line, col) in fns:
        ev.fns[name] = (params, body, line, col)
    if "main" not in ev.fns:
        raise ReferenceError("%s: no main" % path)

    def run():
        _, body, _, _ = ev.fns["main"]
        try:
            return ev.run_stmts(body, {})
        except ReturnSignal as r:
            return r.value

    sys.setrecursionlimit(1000000)
    try:
        v = _run_with_big_stack(run)
    except Trap as t:
        err = "trap: %s at %s:%d:%d\n" % (t.kind, path, t.line, t.col)
        return bytes(ev.out), err.encode("utf-8"), TRAP_EXIT
    except RecursionError:
        raise ReferenceError("%s: evaluator recursion exhausted" % path)
    rc = 0 if v is None else (v & 0xFF)
    return bytes(ev.out), b"", rc


def main(argv):
    if len(argv) != 2:
        sys.stderr.write("usage: reference.py <file.k>\n")
        return 2
    out, err, rc = expected_for(argv[1])
    print(json.dumps({
        "stdout": base64.b64encode(out).decode("ascii"),
        "stderr": base64.b64encode(err).decode("ascii"),
        "exit": rc,
    }))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
