#!/usr/bin/env python3
"""tests/reference.py — INDEPENDENT reference evaluator for the Kavir M2
surface (ADR-007 decision 2; ADR-008 decisions 1-4, 8).

This file is written from the ADR PROSE ONLY, never from the compiler
sources: it is the third arm of every M2 conformance case.

    ELF (stdout, stderr, exit)  ==  IR interpreter  ==  this evaluator

Semantics implemented here (ADR-007 + ADR-008):
  - i32 signed arithmetic, evaluated at "runtime": left operand, then
    right operand, then the operation; statements in order; if picks
    exactly one branch;
  - an out-of-range result of + - * / % is a TRAP "integer overflow";
  - division/remainder truncate toward zero (C semantics);
  - division by zero is a TRAP "division by zero";
  - INT32_MIN / -1 and INT32_MIN % -1 are TRAPs "integer overflow";
  - print(int) writes decimal with '-' for negatives, nothing else;
  - print("...") writes the literal bytes; escapes \\n \\t \\r \\\" \\\\;
  - a trap writes "trap: <kind> at <file>:<line>:<col>\\n" to stderr and
    the process exits 101; <file> is the input path as given;
  - main's return value & 0xFF is the exit code;
  - unary minus on literals is numeral interpretation: -2147483648 is
    INT32_MIN; a bare 2147483648 (not under unary minus) is a compile
    error, as is any literal outside i32.

CLI:  python3 tests/reference.py <file.k>
      prints one JSON line: {"stdout": b64, "stderr": b64, "exit": n}

Module: expected_for(path) -> (stdout: bytes, stderr: bytes, exit: int)
Raises ReferenceError on anything it cannot parse — for programs the
compiler accepted that is a FINDING, not a nuisance.
"""
import base64
import json
import sys

INT32_MIN = -2147483648
INT32_MAX = 2147483647
U64_MAX = 18446744073709551615
TRAP_EXIT = 101

KEYWORDS = {"fn", "return", "i32", "if", "else", "print"}


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
        single = {"+": "+", "-": "-", "*": "*", "/": "/", "%": "%",
                  "(": "(", ")": ")", "{": "{", "}": "}", ";": ";"}
        if c in single:
            toks.append(Tok(single[c], s=c, line=tl, col=tc))
            adv()
            continue
        raise ReferenceError("%s:%d:%d: unexpected character %r"
                             % (path, tl, tc, c))
    toks.append(Tok("EOF", line=line, col=col))
    return toks


# --------------------------------------------------------------- parser
# AST: ('int', v, line, col) | ('neg', child, line, col)
#      | ('bin', op, a, b, opline, opcol)
# stmt: ('return', e) | ('printi', e) | ('prints', bytes) | ('if', e, then, else)

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
        return self.addsub()

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
        raise ReferenceError("%s:%d:%d: unexpected %r in block"
                             % (self.path, t.line, t.col, t.s))

    def program(self):
        fns = []
        while self.peek().kind != "EOF":
            self.expect("fn", "'fn'")
            name = self.expect("IDENT", "function name")
            self.expect("(", "'('")
            self.expect(")", "')'")
            self.expect("ARROW", "'->'")
            self.expect("i32", "'i32'")
            body = self.block()
            fns.append((name.s, body))
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


class Evaluator:
    def __init__(self, path):
        self.out = bytearray()
        self.path = path

    def trap(self, kind, line, col):
        raise Trap(kind, line, col)

    def eval(self, node):
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
        if kind == "bin":
            _, op, a, b, opline, opcol = node
            va = self.eval(a)      # evaluation order: lhs, rhs, op
            vb = self.eval(b)
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

    def run_stmts(self, stmts):
        for st in stmts:
            k = st[0]
            if k == "printi":
                v = self.eval(st[1])
                self.out += str(v).encode("ascii")
            elif k == "prints":
                self.out += st[1]
            elif k == "if":
                c = self.eval(st[1])
                if c != 0:
                    self.run_stmts(st[2])
                else:
                    self.run_stmts(st[3])
            elif k == "return":
                v = self.eval(st[1])
                return v
            else:
                raise ReferenceError("unknown statement %r" % (k,))
        return None


def expected_for(path):
    """Computes (stdout, stderr, exit) for a source file, independently
    of the compiler. Raises ReferenceError for anything unparsable."""
    with open(path, "rb") as f:
        src = f.read().decode("utf-8", "surrogateescape")
    toks = tokenize(src, path)
    fns = Parser(toks, path).program()
    mains = [b for (n, b) in fns if n == "main"]
    if not mains:
        raise ReferenceError("%s: no main" % path)

    ev = Evaluator(path)
    rc = 0
    try:
        v = ev.run_stmts(mains[0])
        if v is not None:
            rc = v & 0xFF
    except Trap as t:
        err = ("trap: %s at %s:%d:%d\n" % (t.kind, path, t.line, t.col))
        return bytes(ev.out), err.encode("utf-8"), TRAP_EXIT
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
