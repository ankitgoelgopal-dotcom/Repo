#!/usr/bin/env python3
"""Verifies the math/printf/strtod/FPU output that sdk/examples/mathtest and fputest print on the guest,
against Python (IEEE-754 doubles + the host C library). Exit status 0 = everything matched.
usage: check_math.py <serial.log>"""
import math, re, struct, sys

def f(b): return struct.unpack("<d", struct.pack("<Q", int(b, 16)))[0]
def bits(x): return struct.unpack("<Q", struct.pack("<d", x))[0]
def ordered(x):                       # monotonic integer ordering of doubles, for ULP distance
    b = bits(x); return -(b & 0x7FFFFFFFFFFFFFFF) if b >> 63 else b
def ulps(a, b):
    if math.isnan(a) and math.isnan(b): return 0
    if math.isnan(a) or math.isnan(b): return 1 << 60
    if a == b: return 0
    return abs(ordered(a) - ordered(b))

def rnd(x): return math.copysign(math.floor(abs(x) + 0.5), x)
def num(fn):                          # python math funcs raise instead of returning inf/nan
    def g(*a):
        try: return float(fn(*a))
        except OverflowError: return math.inf
        except ValueError: return math.nan
    return g
M1 = {"sin": math.sin, "cos": math.cos, "tan": math.tan, "atan": math.atan, "exp": math.exp, "sinh": math.sinh, "cosh": math.cosh,
      "tanh": math.tanh, "cbrt": math.cbrt, "fabs": math.fabs, "floor": math.floor, "ceil": math.ceil, "trunc": math.trunc, "round": rnd,
      "log": math.log, "log2": math.log2, "log10": math.log10, "sqrt": math.sqrt, "log1p": math.log1p, "tgamma": math.gamma,
      "lgamma": math.lgamma, "asin": math.asin, "acos": math.acos, "erf": math.erf, "expm1": math.expm1}
M2 = {"pow": math.pow, "atan2": math.atan2, "hypot": math.hypot, "fmod": math.fmod, "fmax": lambda a, b: max(a, b),
      "fmin": lambda a, b: min(a, b), "copysign": math.copysign}
TOL = {"fabs": 0, "floor": 0, "ceil": 0, "trunc": 0, "round": 0, "sqrt": 0, "fmod": 0, "fmax": 0, "fmin": 0, "copysign": 0,
       "hypot": 1, "tgamma": 8, "lgamma": 8, "erf": 4, "cbrt": 2}
DEFAULT_TOL = 2

lines = [l.rstrip("\r\n") for l in open(sys.argv[1], errors="replace")]
fails, checked = [], 0
def ok(cond, what):
    global checked; checked += 1
    if not cond: fails.append(what)

for l in lines:
    p = l.split(" ")
    try:
        if p[0] == "M1" and p[1] in M1:
            x, got = f(p[2]), f(p[3]); want = num(M1[p[1]])(x)
            tol = TOL.get(p[1], DEFAULT_TOL)
            ok(ulps(got, want) <= tol or abs(got - want) <= 4e-16, f"{p[1]}({x!r}) = {got!r}, expected {want!r} ({ulps(got, want)} ulp)")
        elif p[0] == "M2" and p[1] in M2:
            x, y, got = f(p[2]), f(p[3]), f(p[4]); want = num(M2[p[1]])(x, y)
            tol = TOL.get(p[1], DEFAULT_TOL)
            ok(ulps(got, want) <= tol, f"{p[1]}({x!r},{y!r}) = {got!r}, expected {want!r}")
        elif p[0] == "F":
            v = f(p[1]); rest = l.split(" ", 2)[2]; fmt, out = rest.split(" ::", 1)
            ok(out == fmt % v, f"printf({fmt!r}, {v!r}) = {out!r}, expected {fmt % v!r}")
        elif p[0] == "I":
            v = int(p[1]); rest = l.split(" ", 2)[2]; fmt, out = rest.split(" ::", 1)
            pf = fmt.replace("ll", "").replace("l", "")
            if fmt == "%.0d" and v == 0: want = ""
            elif "#o" in pf:                      # C prefixes a single 0 (Python writes 0o); %#o of 0 is just "0"
                want = (pf.replace("#", "") % v) if v == 0 else (pf % v).replace("0o", "0")
            else: want = pf % v
            ok(out == want, f"printf({fmt!r}, {v}) = {out!r}, expected {want!r}")
        elif p[0] == "D":
            s = l.split(" ", 3)[3]; got = f(p[1]); used = int(p[2])
            m = re.match(r"\s*[+-]?(inf(inity)?|nan|0x[0-9a-f]*\.?[0-9a-f]*(p[+-]?\d+)?|(\d+\.?\d*|\.\d+)(e[+-]?\d+)?)", s, re.I)
            t = m.group(0).strip().lower()
            want = float.fromhex(t) if "0x" in t else float(t)
            ok(bits(got) == bits(want) and used == m.end(), f"strtod({s!r}) = {got!r} used {used}, expected {want!r} used {m.end()}")
    except Exception as e:
        fails.append(f"checker error on {l!r}: {e}")

def find(prefix): return [l for l in lines if l.startswith(prefix)]
def one(prefix):
    r = find(prefix); return r[0] if r else ""
ok(re.search(r"SP sqrt\(-1\)=-?nan log\(0\)=-inf pow\(0,-1\)=inf", one("SP sqrt")) is not None, "special values (sqrt/log/pow)")
ok(re.search(r"SP 1/0=inf -1/0=-inf 0/0=-?nan", one("SP 1/0")) is not None, "inf/nan printing")
ok("isnan=1 isinf=1 signbit(-0.0)=1 fpclassify(1e-310)=1" in one("SP isnan"), "isnan/isinf/signbit/fpclassify")
ok("SP fenv: divbyzero=1 invalid=0 round=1" == one("SP fenv"), f"fenv flags: {one('SP fenv')!r}")
ok(one("S1 ") == "S1 [     right][left      ][tru][Z][    y]", "string/char printf")
ok(one("S2 ").startswith("S2 12 "), "snprintf(NULL,0) length")
ok(one("L ") == "L -123 31 4294967295 -9223372036854775808 511", f"strtol family: {one('L ')!r}")
ok(one("Q ") == "Q 1 2 3 5 7 8 9", "qsort")
ok("MATHTEST DONE" in lines, "mathtest finished")

# FPU state across context switches: two tasks ran concurrently; results must equal the host's IEEE arithmetic
def fpu(x, y):
    for i in range(1, 1500001):
        x = x * 1.0000001 + 1e-9 * float(i)
        y = y * 0.9999999 + math.sqrt(x) * 1e-6
    return x, y
spans = []
for tag, (x0, y0) in (("A", (1.5, 0.5)), ("B", (2.5, 3.5))):
    r = one(f"FPU[{tag}]")
    m = re.match(r"FPU\[.\] x=(\w+) y=(\w+) start=(\d+) end=(\d+)", r)
    if not m: ok(False, f"FPU[{tag}] result missing"); continue
    wx, wy = fpu(x0, y0)
    ok(int(m.group(1), 16) == bits(wx) and int(m.group(2), 16) == bits(wy), f"FPU[{tag}] result {m.group(1)},{m.group(2)} != {bits(wx):016x},{bits(wy):016x}")
    spans.append((int(m.group(3)), int(m.group(4))))
if len(spans) == 2:
    overlap = min(spans[0][1], spans[1][1]) - max(spans[0][0], spans[1][0])
    ok(overlap >= 30, f"FPU tasks did not overlap in time (overlap {overlap} ms), so task switching was not exercised")

print(f"math check: {checked} checks, {len(fails)} failed")
for x in fails[:40]: print("  FAIL:", x)
sys.exit(1 if fails else 0)
